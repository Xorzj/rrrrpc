#include "raft_node.h"
#include "../common/serializer.h"
#include <iostream>
#include <chrono>
#include <algorithm>

RaftNode::RaftNode(const std::string& node_id, const std::string& host, int port,
                   const std::vector<RaftPeer>& peers, 
                   std::shared_ptr<RaftStateMachine> state_machine)
    : node_id_(node_id), host_(host), port_(port), peers_(peers), 
      state_machine_(state_machine), role_(RaftRole::FOLLOWER),
      current_term_(0), commit_index_(0), last_applied_(0),
      running_(false), gen_(rd_()) {
    
    // 初始化Leader状态
    next_index_.resize(peers_.size(), 1);
    match_index_.resize(peers_.size(), 0);
    
    // 创建网络服务器
    server_ = std::make_unique<TcpServer>(port_);
    server_->setConnectionHandler([this](std::shared_ptr<TcpConnection> conn) {
        handleConnection(conn);
    });
    
    std::cout << "[Raft] 节点 " << node_id_ << " 初始化完成，监听端口 " << port_ << std::endl;
}

RaftNode::~RaftNode() {
    stop();
}

void RaftNode::start() {
    if (running_.load()) {
        return;
    }
    
    running_ = true;
    
    // 启动网络服务器
    if (!server_->start()) {
        std::cerr << "[Raft] 启动服务器失败" << std::endl;
        return;
    }
    
    // 启动各种线程
    election_timer_thread_ = std::thread(&RaftNode::electionTimerLoop, this);
    heartbeat_thread_ = std::thread(&RaftNode::heartbeatLoop, this);
    log_applier_thread_ = std::thread(&RaftNode::logApplierLoop, this);
    
    std::cout << "[Raft] 节点 " << node_id_ << " 启动成功" << std::endl;
    
    // 运行服务器主循环
    server_->run();
}

void RaftNode::stop() {
    if (!running_.load()) {
        return;
    }
    
    running_ = false;
    
    // 停止服务器
    if (server_) {
        server_->stop();
    }
    
    // 等待线程结束
    log_condition_.notify_all();
    
    if (election_timer_thread_.joinable()) {
        election_timer_thread_.join();
    }
    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
    if (log_applier_thread_.joinable()) {
        log_applier_thread_.join();
    }
    
    std::cout << "[Raft] 节点 " << node_id_ << " 已停止" << std::endl;
}

void RaftNode::electionTimerLoop() {
    while (running_.load()) {
        int timeout = getElectionTimeout();
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
        
        if (!running_.load()) break;
        
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (role_ != RaftRole::LEADER) {
            std::cout << "[Raft] 选举超时，开始新的选举" << std::endl;
            startElection();
        }
    }
}

void RaftNode::heartbeatLoop() {
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 50ms心跳间隔
        
        if (!running_.load()) break;
        
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (role_ == RaftRole::LEADER) {
            // 发送心跳给所有follower
            for (size_t i = 0; i < peers_.size(); ++i) {
                rpc::AppendEntriesRequest request;
                request.set_term(current_term_.load());
                request.set_leader_id(node_id_);
                request.set_leader_commit(commit_index_.load());
                
                std::lock_guard<std::mutex> log_lock(log_mutex_);
                int64_t prev_log_index = next_index_[i] - 1;
                request.set_prev_log_index(prev_log_index);
                
                if (prev_log_index > 0 && prev_log_index <= static_cast<int64_t>(log_.size())) {
                    request.set_prev_log_term(log_[prev_log_index - 1].term());
                } else {
                    request.set_prev_log_term(0);
                }
                
                // 发送新的日志条目（如果有的话）
                for (int64_t j = next_index_[i]; j <= static_cast<int64_t>(log_.size()); ++j) {
                    *request.add_entries() = log_[j - 1];
                }
                
                sendAppendEntries(peers_[i], request);
            }
        }
    }
}

void RaftNode::logApplierLoop() {
    while (running_.load()) {
        std::unique_lock<std::mutex> lock(log_mutex_);
        log_condition_.wait(lock, [this] { 
            return !running_.load() || last_applied_.load() < commit_index_.load(); 
        });
        
        if (!running_.load()) break;
        
        applyCommittedEntries();
    }
}

void RaftNode::startElection() {
    becomeCandidate();
    
    current_term_++;
    voted_for_ = node_id_;
    
    std::cout << "[Raft] 开始选举，任期 " << current_term_.load() << std::endl;
    
    // 为自己投票
    int votes = 1;
    
    // 向所有peer请求投票
    rpc::RequestVoteRequest request;
    request.set_term(current_term_.load());
    request.set_candidate_id(node_id_);
    request.set_last_log_index(getLastLogIndex());
    request.set_last_log_term(getLastLogTerm());
    
    for (const auto& peer : peers_) {
        if (sendRequestVote(peer, request)) {
            votes++;
        }
    }
    
    // 检查是否获得多数票
    int majority = (peers_.size() + 1) / 2 + 1;
    if (votes >= majority) {
        std::cout << "[Raft] 获得 " << votes << " 票，成为Leader" << std::endl;
        becomeLeader();
    } else {
        std::cout << "[Raft] 只获得 " << votes << " 票，选举失败" << std::endl;
        becomeFollower(current_term_.load());
    }
}

void RaftNode::becomeLeader() {
    role_ = RaftRole::LEADER;
    leader_id_ = node_id_;
    
    // 初始化Leader状态
    std::fill(next_index_.begin(), next_index_.end(), getLastLogIndex() + 1);
    std::fill(match_index_.begin(), match_index_.end(), 0);
    
    std::cout << "[Raft] 节点 " << node_id_ << " 成为Leader，任期 " << current_term_.load() << std::endl;
}

void RaftNode::becomeFollower(int64_t term, const std::string& leader_id) {
    role_ = RaftRole::FOLLOWER;
    current_term_ = term;
    voted_for_ = "";
    leader_id_ = leader_id;
    
    std::cout << "[Raft] 节点 " << node_id_ << " 成为Follower，任期 " << term 
              << "，Leader: " << leader_id << std::endl;
}

void RaftNode::becomeCandidate() {
    role_ = RaftRole::CANDIDATE;
    leader_id_ = "";
    
    std::cout << "[Raft] 节点 " << node_id_ << " 成为Candidate" << std::endl;
}

bool RaftNode::proposeLogEntry(const std::string& operation, const std::string& data) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (role_ != RaftRole::LEADER) {
        std::cout << "[Raft] 只有Leader可以提议日志条目" << std::endl;
        return false;
    }
    
    rpc::LogEntry entry;
    entry.set_term(current_term_.load());
    entry.set_index(getLastLogIndex() + 1);
    entry.set_operation(operation);
    entry.set_data(data);
    
    if (appendLogEntry(entry)) {
        std::cout << "[Raft] 提议日志条目成功: " << operation << std::endl;
        return true;
    }
    
    return false;
}

void RaftNode::handleConnection(std::shared_ptr<TcpConnection> conn) {
    std::string data = conn->receive();
    if (data.empty()) {
        return;
    }
    
    // 尝试解析为RequestVote请求
    rpc::RequestVoteRequest vote_request;
    if (Serializer::deserialize(data, &vote_request)) {
        rpc::RequestVoteResponse vote_response;
        handleRequestVote(vote_request, vote_response);
        
        std::string response_data = Serializer::serialize(vote_response);
        conn->send(response_data);
        return;
    }
    
    // 尝试解析为AppendEntries请求
    rpc::AppendEntriesRequest append_request;
    if (Serializer::deserialize(data, &append_request)) {
        rpc::AppendEntriesResponse append_response;
        handleAppendEntries(append_request, append_response);
        
        std::string response_data = Serializer::serialize(append_response);
        conn->send(response_data);
        return;
    }
    
    std::cout << "[Raft] 收到未知类型的消息" << std::endl;
}

int RaftNode::getElectionTimeout() {
    std::uniform_int_distribution<> dis(150, 300); // 150-300ms随机超时
    return dis(gen_);
}

int64_t RaftNode::getLastLogIndex() const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    return log_.size();
}

int64_t RaftNode::getLastLogTerm() const {
    std::lock_guard<std::mutex> lock(log_mutex_);
    if (log_.empty()) {
        return 0;
    }
    return log_.back().term();
}

bool RaftNode::appendLogEntry(const rpc::LogEntry& entry) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    log_.push_back(entry);
    log_condition_.notify_one();
    return true;
}

void RaftNode::applyCommittedEntries() {
    while (last_applied_.load() < commit_index_.load()) {
        int64_t index = last_applied_.load() + 1;
        if (index <= static_cast<int64_t>(log_.size())) {
            state_machine_->applyLogEntry(log_[index - 1]);
            last_applied_++;
        } else {
            break;
        }
    }
}

// 这里省略了handleRequestVote, handleAppendEntries, sendRequestVote, sendAppendEntries等方法的实现
// 由于篇幅限制，这些方法将在下一个文件中实现

void RaftNode::getClusterStatus(rpc::ClusterStatusResponse& response) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    response.set_leader_id(leader_id_);
    
    // 添加当前节点信息
    auto* node_info = response.add_nodes();
    node_info->set_node_id(node_id_);
    node_info->set_host(host_);
    node_info->set_port(port_);
    node_info->set_term(current_term_.load());
    
    switch (role_) {
        case RaftRole::LEADER:
            node_info->set_role("leader");
            break;
        case RaftRole::FOLLOWER:
            node_info->set_role("follower");
            break;
        case RaftRole::CANDIDATE:
            node_info->set_role("candidate");
            break;
    }
    
    // 添加peer信息（简化版本，实际应该查询peer状态）
    for (const auto& peer : peers_) {
        auto* peer_info = response.add_nodes();
        peer_info->set_node_id(peer.node_id);
        peer_info->set_host(peer.host);
        peer_info->set_port(peer.port);
        peer_info->set_role("unknown");
        peer_info->set_term(0);
    }
}