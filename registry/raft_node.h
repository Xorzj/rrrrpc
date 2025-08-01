#ifndef RAFT_NODE_H
#define RAFT_NODE_H

#include "raft_state_machine.h"
#include "../common/network.h"
#include "../proto/distributed_registry.pb.h"
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <random>
#include <atomic>
#include <memory>

enum class RaftRole {
    FOLLOWER,
    CANDIDATE,
    LEADER
};

struct RaftPeer {
    std::string node_id;
    std::string host;
    int port;
    int64_t next_index;
    int64_t match_index;
};

class RaftNode {
public:
    RaftNode(const std::string& node_id, const std::string& host, int port,
             const std::vector<RaftPeer>& peers, 
             std::shared_ptr<RaftStateMachine> state_machine);
    ~RaftNode();
    
    void start();
    void stop();
    
    // Raft RPC接口
    void handleRequestVote(const rpc::RequestVoteRequest& request, rpc::RequestVoteResponse& response);
    void handleAppendEntries(const rpc::AppendEntriesRequest& request, rpc::AppendEntriesResponse& response);
    
    // 客户端接口
    bool proposeLogEntry(const std::string& operation, const std::string& data);
    bool isLeader() const { return role_ == RaftRole::LEADER; }
    std::string getLeaderId() const { return leader_id_; }
    RaftRole getRole() const { return role_; }
    
    // 集群状态
    void getClusterStatus(rpc::ClusterStatusResponse& response);

private:
    // 基本信息
    std::string node_id_;
    std::string host_;
    int port_;
    std::vector<RaftPeer> peers_;
    std::shared_ptr<RaftStateMachine> state_machine_;
    
    // Raft状态
    std::atomic<RaftRole> role_;
    std::atomic<int64_t> current_term_;
    std::string voted_for_;
    std::string leader_id_;
    
    // 日志
    std::vector<rpc::LogEntry> log_;
    std::atomic<int64_t> commit_index_;
    std::atomic<int64_t> last_applied_;
    
    // Leader状态
    std::vector<int64_t> next_index_;
    std::vector<int64_t> match_index_;
    
    // 线程和同步
    std::thread election_timer_thread_;
    std::thread heartbeat_thread_;
    std::thread log_applier_thread_;
    std::atomic<bool> running_;
    
    mutable std::mutex state_mutex_;
    mutable std::mutex log_mutex_;
    std::condition_variable log_condition_;
    
    // 随机数生成器
    std::random_device rd_;
    std::mt19937 gen_;
    
    // 网络
    std::unique_ptr<TcpServer> server_;
    
    // 内部方法
    void electionTimerLoop();
    void heartbeatLoop();
    void logApplierLoop();
    void startElection();
    void becomeLeader();
    void becomeFollower(int64_t term, const std::string& leader_id = "");
    void becomeCandidate();
    
    bool sendRequestVote(const RaftPeer& peer, const rpc::RequestVoteRequest& request);
    bool sendAppendEntries(const RaftPeer& peer, const rpc::AppendEntriesRequest& request);
    
    void handleConnection(std::shared_ptr<TcpConnection> conn);
    void resetElectionTimer();
    int getElectionTimeout();
    
    // 日志操作
    int64_t getLastLogIndex() const;
    int64_t getLastLogTerm() const;
    bool appendLogEntry(const rpc::LogEntry& entry);
    void applyCommittedEntries();
};

#endif