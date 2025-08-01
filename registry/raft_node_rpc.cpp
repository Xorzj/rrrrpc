#include "raft_node.h"
#include "../common/serializer.h"
#include <iostream>

void RaftNode::handleRequestVote(const rpc::RequestVoteRequest& request, rpc::RequestVoteResponse& response) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    std::cout << "[Raft] 收到投票请求: 候选人=" << request.candidate_id() 
              << ", 任期=" << request.term() << std::endl;
    
    response.set_term(current_term_.load());
    response.set_vote_granted(false);
    
    // 如果请求的任期小于当前任期，拒绝投票
    if (request.term() < current_term_.load()) {
        std::cout << "[Raft] 拒绝投票: 请求任期过旧" << std::endl;
        return;
    }
    
    // 如果请求的任期大于当前任期，更新当前任期并成为follower
    if (request.term() > current_term_.load()) {
        becomeFollower(request.term());
    }
    
    // 检查是否已经投票
    bool can_vote = voted_for_.empty() || voted_for_ == request.candidate_id();
    
    if (can_vote) {
        // 检查候选人的日志是否至少和自己一样新
        int64_t last_log_index = getLastLogIndex();
        int64_t last_log_term = getLastLogTerm();
        
        bool log_ok = (request.last_log_term() > last_log_term) ||
                      (request.last_log_term() == last_log_term && 
                       request.last_log_index() >= last_log_index);
        
        if (log_ok) {
            voted_for_ = request.candidate_id();
            response.set_vote_granted(true);
            resetElectionTimer();
            
            std::cout << "[Raft] 投票给候选人: " << request.candidate_id() << std::endl;
        } else {
            std::cout << "[Raft] 拒绝投票: 候选人日志不够新" << std::endl;
        }
    } else {
        std::cout << "[Raft] 拒绝投票: 已经投票给 " << voted_for_ << std::endl;
    }
}

void RaftNode::handleAppendEntries(const rpc::AppendEntriesRequest& request, rpc::AppendEntriesResponse& response) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    response.set_term(current_term_.load());
    response.set_success(false);
    
    // 如果请求的任期小于当前任期，拒绝
    if (request.term() < current_term_.load()) {
        std::cout << "[Raft] 拒绝AppendEntries: 请求任期过旧" << std::endl;
        return;
    }
    
    // 如果请求的任期大于等于当前任期，成为follower
    if (request.term() >= current_term_.load()) {
        becomeFollower(request.term(), request.leader_id());
        resetElectionTimer();
    }
    
    std::lock_guard<std::mutex> log_lock(log_mutex_);
    
    // 检查日志一致性
    if (request.prev_log_index() > 0) {
        if (request.prev_log_index() > static_cast<int64_t>(log_.size()) ||
            log_[request.prev_log_index() - 1].term() != request.prev_log_term()) {
            
            std::cout << "[Raft] 拒绝AppendEntries: 日志不一致" << std::endl;
            return;
        }
    }
    
    // 删除冲突的日志条目
    if (request.entries_size() > 0) {
        int64_t new_index = request.prev_log_index() + 1;
        
        // 删除从new_index开始的所有日志条目
        if (new_index <= static_cast<int64_t>(log_.size())) {
            log_.erase(log_.begin() + new_index - 1, log_.end());
        }
        
        // 添加新的日志条目
        for (const auto& entry : request.entries()) {
            log_.push_back(entry);
            std::cout << "[Raft] 添加日志条目: index=" << entry.index() 
                      << ", term=" << entry.term() 
                      << ", operation=" << entry.operation() << std::endl;
        }
    }
    
    // 更新commit index
    if (request.leader_commit() > commit_index_.load()) {
        int64_t new_commit_index = std::min(request.leader_commit(), 
                                           static_cast<int64_t>(log_.size()));
        commit_index_ = new_commit_index;
        log_condition_.notify_one();
        
        std::cout << "[Raft] 更新commit index: " << new_commit_index << std::endl;
    }
    
    response.set_success(true);
    response.set_match_index(log_.size());
    
    if (request.entries_size() == 0) {
        // 这是心跳消息
        std::cout << "[Raft] 收到来自Leader " << request.leader_id() << " 的心跳" << std::endl;
    }
}

bool RaftNode::sendRequestVote(const RaftPeer& peer, const rpc::RequestVoteRequest& request) {
    try {
        TcpClient client;
        if (!client.connect(peer.host, peer.port)) {
            std::cout << "[Raft] 连接peer失败: " << peer.node_id << std::endl;
            return false;
        }
        
        std::string request_data = Serializer::serialize(request);
        if (!client.send(request_data)) {
            std::cout << "[Raft] 发送投票请求失败: " << peer.node_id << std::endl;
            return false;
        }
        
        std::string response_data = client.receive();
        if (response_data.empty()) {
            std::cout << "[Raft] 接收投票响应失败: " << peer.node_id << std::endl;
            return false;
        }
        
        rpc::RequestVoteResponse response;
        if (!Serializer::deserialize(response_data, &response)) {
            std::cout << "[Raft] 解析投票响应失败: " << peer.node_id << std::endl;
            return false;
        }
        
        // 检查响应的任期
        if (response.term() > current_term_.load()) {
            std::lock_guard<std::mutex> lock(state_mutex_);
            becomeFollower(response.term());
            return false;
        }
        
        if (response.vote_granted()) {
            std::cout << "[Raft] 获得来自 " << peer.node_id << " 的投票" << std::endl;
            return true;
        } else {
            std::cout << "[Raft] " << peer.node_id << " 拒绝投票" << std::endl;
            return false;
        }
        
    } catch (const std::exception& e) {
        std::cout << "[Raft] 发送投票请求异常: " << peer.node_id << " - " << e.what() << std::endl;
        return false;
    }
}

bool RaftNode::sendAppendEntries(const RaftPeer& peer, const rpc::AppendEntriesRequest& request) {
    try {
        TcpClient client;
        if (!client.connect(peer.host, peer.port)) {
            return false;
        }
        
        std::string request_data = Serializer::serialize(request);
        if (!client.send(request_data)) {
            return false;
        }
        
        std::string response_data = client.receive();
        if (response_data.empty()) {
            return false;
        }
        
        rpc::AppendEntriesResponse response;
        if (!Serializer::deserialize(response_data, &response)) {
            return false;
        }
        
        // 检查响应的任期
        if (response.term() > current_term_.load()) {
            std::lock_guard<std::mutex> lock(state_mutex_);
            becomeFollower(response.term());
            return false;
        }
        
        // 更新peer的索引信息
        auto peer_it = std::find_if(peers_.begin(), peers_.end(),
            [&peer](const RaftPeer& p) { return p.node_id == peer.node_id; });
        
        if (peer_it != peers_.end()) {
            size_t peer_index = std::distance(peers_.begin(), peer_it);
            
            if (response.success()) {
                // 成功，更新next_index和match_index
                next_index_[peer_index] = response.match_index() + 1;
                match_index_[peer_index] = response.match_index();
                
                // 检查是否可以提交更多日志条目
                updateCommitIndex();
            } else {
                // 失败，减少next_index重试
                if (next_index_[peer_index] > 1) {
                    next_index_[peer_index]--;
                }
            }
        }
        
        return response.success();
        
    } catch (const std::exception& e) {
        return false;
    }
}

void RaftNode::updateCommitIndex() {
    if (role_ != RaftRole::LEADER) {
        return;
    }
    
    std::lock_guard<std::mutex> log_lock(log_mutex_);
    
    // 找到大多数节点都已复制的最大索引
    for (int64_t index = log_.size(); index > commit_index_.load(); --index) {
        if (log_[index - 1].term() != current_term_.load()) {
            continue; // 只能提交当前任期的日志条目
        }
        
        int count = 1; // 包括leader自己
        for (size_t i = 0; i < match_index_.size(); ++i) {
            if (match_index_[i] >= index) {
                count++;
            }
        }
        
        int majority = (peers_.size() + 1) / 2 + 1;
        if (count >= majority) {
            commit_index_ = index;
            log_condition_.notify_one();
            
            std::cout << "[Raft] 提交日志到索引: " << index << std::endl;
            break;
        }
    }
}

void RaftNode::resetElectionTimer() {
    // 这个方法在实际实现中应该重置选举定时器
    // 由于我们使用的是简单的sleep循环，这里只是一个占位符
    // 在更复杂的实现中，可以使用条件变量或其他同步机制
}