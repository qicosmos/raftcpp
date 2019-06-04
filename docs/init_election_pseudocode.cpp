//init election pseudocode ignore lock
#include <string>
#include <vector>

enum RoleState{
    Follower,
    Candidate,
    Leader
};

enum Tick{
    ElectionTick = 150,
    HeartBeatTick = 20
};

class RaftState{
public:
    RoleState GetState();
    uint64_t GetTerm();
    void ResetRefresh();
    bool GetRefresh();
    void Vote(uint64_t term) {
        if (term > term_) {
            term = term_;
            refresh_ = true;
            state_ = Follower;
        }
    }
    void Heartbeat(uint64_t term) {
        if (term > term_) {
            term = term_;
            refresh_ = true;
            state_ = Follower;
        }else if (term == term_){
            refresh_ = true;
        }
    }
    void BecomeFollower() {state_ = Follower;}
    void BecomeCandidate() {
        state_ = Candidate;
        term_++;
    }
    void HoldCandidate() {term_++;}
    void BecomeLeader() {state_ = Leader;}
private:
    uint64_t                    term_ = 0;
    RoleState                   state_ = Follower;
    bool                        refresh_ = false;
};

class RaftClient{
public:
    bool RpcVote(uint64_t term);
    void RpcHeartBeat(uint64_t term);
};

class RaftServer{
public:
    //call RollStatus Vote
    void RpcVote();
    //call RollStatus HeartBeat
    void RpcHeartBeat();
    //call Looper in new thread
    void StartLooper();
private:
    void Wait(uint64_t ms);
    //[Election, Election + random(Election)]
    uint64_t ElectionTimeout();
    void Looper() {
        uint64_t count;
        while(!stop_) {
            switch (state_.GetState()) {
            case Follower:
                state_.ResetRefresh();
                Wait(ElectionTimeout());
                //fallthrough Candidate immediately
                if (!state_.GetRefresh()) state_.BecomeCandidate();
                break;
            case Candidate:
                count = 0;
                for (auto &client : clients_) {
                    auto ok = client.RpcVote(state_.GetTerm());
                    if (ok) count++;
                }
                if (count + 1 > (clients_.size() + 1) / 2) {
                    //fallthrough Leader immediately
                    state_.BecomeLeader();
                }else {
                    Wait(ElectionTimeout());
                    state_.HoldCandidate();
                }
                break;
            case Leader:
                for (auto &client : clients_) {
                    client.RpcHeartBeat(state_.GetTerm());
                }
                Wait(HeartBeatTick);
                break;
            default: /*asset*/ break;
            }
        }
    }
    RaftState                   state_;
    bool                        stop_ = false;
    std::vector<RaftClient>     clients_;
};
