#pragma once
#include <memory>
#include <random>
#include <rpc_server.h>
#include <rpc_client.hpp>
using namespace rest_rpc;
using namespace rpc_service;
#include "common.h"
#include "entity.h"

namespace raftcpp{

	static constexpr int LOOP_PERIOD = 1000;//milliseconds
	static constexpr int ELECTION_TIMEOUT = 500;//milliseconds
	static constexpr int HEARTBEAT_PERIOD = 500/2;//milliseconds
	static std::default_random_engine g_generator;

/*
	problems need to be solved: 
	1.should use atomic to make sure thread safe;
*/
class raft_server {
public:
	raft_server(const config& conf, size_t thrd_num = 1) : rpc_server_(conf.host_addr.port, thrd_num),
		conf_(conf), state_(State::FOLLOWER){
		me_.peer_id = conf.host_id;
		me_.election_timeout = 500;
		me_.election_timeout_rand = 500;
		me_.heartbeat_timeout = 500 / 2;
		rpc_server_.register_handler("request_vote", &raft_server::request_vote, this);
		rpc_server_.register_handler("append_entry", &raft_server::append_entry, this);
		rpc_server_.run();
	}

	bool connect_peers(size_t timeout = 10) {
		//assert(conf_.peers_addr.size() >= 2);
		for (auto& addr : conf_.peers_addr) {
			auto peer = std::make_shared<rpc_client>(addr.ip, addr.port);
			peer->set_error_callback([this, peer](auto ec) {
				if (ec) {
					peer->async_reconnect();
				}
			});

			bool r = peer->connect(timeout);
			if (!r) {
				peer->async_reconnect();
			}

			peers_.push_back(peer);
		}

		return !peers_.empty();
	}

	void main_loop() {
		while (true) {
			switch (me_.state) {
			case State::FOLLOWER:
				follower();
			break;
			case State::CANDIDATE:
				candidate();
				break;
			case State::LEADER:
				leader();
				break;
			default:
				break;
			}
		}
	}

private:
	//rpc service
	response_vote request_vote(connection* conn, const request_vote_t& args) {
		//reject request if we have a leader(not election timeout)
		//TODO

		response_vote reply{};
		reply.term = me_.current_term;

		if (args.term > me_.current_term) {
			become_follower(args.term);
			me_.current_leader = -1;
		}

		auto vote_granted = get_vote(args);

		reply.vote_granted = vote_granted;

		return reply;
	}

	bool get_vote(const request_vote_t& args) {
		if (args.term < me_.current_term) {
			//outdated request
			return false;
		}

		if (me_.voted_for != -1) {
			//has already voted
			return false;
		}

		if (me_.commit_idx == 0) {
			return true;
		}

		if (me_.current_term < args.last_log_term) {
			return true;
		}

		if (me_.current_term == args.last_log_term && me_.commit_idx <= args.last_log_idx) {
			return true;
		}

		return false;
	}

	//rpc service
	res_append_entry append_entry(connection* conn, const req_append_entry& args) {
		is_heartbeat_timeout_ = true;
		is_election_timeout_ = true;

		res_append_entry reply{};
		reply.term = me_.current_term;

		if (args.term < me_.current_term) {
			reply.success = false;
			return reply;
		}
		else if (args.term > me_.current_term) {
			become_follower(args.term);
		}

		if (args.entries.empty()) {
			reply.success = true;
		}

		return reply;
	}

	void follower() {
		if (heartbeat_timeout()) {
			std::cout << "heartbeat timeout" << std::endl;
			if (election_timeout()) {
				std::cout << "election timeout" << std::endl;
				become_candidate();
			}
		}
	}

	void candidate() {
		become_candidate();
		if (heartbeat_timeout()) {
			std::cout << "heartbeat timeout" << std::endl;
			if (election_timeout()) {
				std::cout << "election timeout" << std::endl;
				become_candidate();
			}
		}
	}

	void leader() {
		become_leader();		
		broadcast_append_entries(); //send heartbeat log to maintain the authority
		std::this_thread::sleep_for(std::chrono::milliseconds(HEARTBEAT_PERIOD));
	}

	void become_candidate() {
		std::cout << "become candidate, current_term: "<< me_.current_term << std::endl;
		me_.current_term += 1;
		me_.voted_for = me_.peer_id;
		me_.voted_count = 1;
		set_state(State::CANDIDATE);

		// [election_timeout, 2 * election_timeout)
		random_election_timeout();

		broadcast_request_vote();
	}

	void become_follower(uint64_t term) {
		std::cout << "become follower" << std::endl;
		me_.current_term = term;
		set_state(State::FOLLOWER);
		me_.voted_for = -1;
	}

	void become_leader() {
		set_state(State::LEADER);
	}

	void broadcast_request_vote() {
		request_vote_t vote{ me_.current_term, me_.peer_id, me_.log.get_last_log_index(), get_last_log_term() };
		std::vector <std::future<req_result>> futures;
		for (auto& client : peers_) {
			if (!client->has_connected()) {
				continue;
			}

			std::cout << "request vote" << std::endl;
			auto future = client->async_call("request_vote", vote);
			futures.push_back(std::move(future));
		}

		handle_vote_response(futures);
	}

	void handle_vote_response(std::vector <std::future<req_result>>& futures) {
		for (auto& future : futures) {
			auto status = future.wait_for(std::chrono::milliseconds(50));
			if (status == std::future_status::timeout) {
				//network timeout, reconnect. TODO
				continue;
			}

			try {
				auto response = future.get().as<response_vote>();
				if (me_.state != State::CANDIDATE) {
					return;
				}

				if (response.term > me_.current_term) {
					become_follower(response.term);
				}

				//same term
				if (response.vote_granted) {
					me_.voted_count += 1;
					if (is_majority(conf_.peers_addr.size(), me_.voted_count)) {
						std::cout << "become leader" << std::endl;
						become_leader();
						broadcast_append_entries(); //send heartbeat log to maintain the authority
					}
				}
			}
			catch (const std::exception& ex) {
				//TODO
				std::cout << ex.what() << std::endl;
			}
		}
	}

	void broadcast_append_entries() {
		req_append_entry entry{};
		std::vector <std::future<req_result>> futures;
		for (auto& client : peers_) {
			auto future = client->async_call("append_entry", entry);
			futures.push_back(std::move(future));
		}

		handle_append_entries_response(futures);
	}

	void handle_append_entries_response(std::vector <std::future<req_result>>& futures) {
		for (auto& future : futures) {
			auto status = future.wait_for(std::chrono::milliseconds(50));
			if (status == std::future_status::timeout) {
				//network timeout, reconnect. TODO
				continue;
			}

			try {
				std::cout << "broadcast heartbeat" << std::endl;
				auto response = future.get().as<res_append_entry>();
				if (response.term > me_.current_term) {
					become_follower(response.term);
				}
				//TODO
			}
			catch (const std::exception & ex) {
				//TODO
				std::cout << ex.what() << std::endl;
			}
		}
	}

	int active_node_num() {
		int active_num = 0;
		for (auto& peer : peers_) {
			if (peer->has_connected()) {
				active_num++;
			}
		}

		return active_num;
	}

	bool is_majority(int num_nodes, int vote_count) {
		//if (num_nodes < vote_count) {
		//	return false;
		//}

		int half = num_nodes / 2;
		return half + 1 <= vote_count;
	}

	uint64_t get_last_log_term() {
		auto& log = me_.log;
		uint64_t lastLogIndex = log.get_last_log_index();
		if (lastLogIndex >= log.get_log_start_index()) {
			return log.get_entry(lastLogIndex).term;
		}
		else {
			return 0;
		}
	}

	//true: timeout, false:not timeout
	bool heartbeat_timeout() {
		std::unique_lock<std::mutex> lock(heartbeat_mtx_);
		bool result = heartbeat_cond_.wait_for(lock, std::chrono::milliseconds(HEARTBEAT_PERIOD),
			[this] {return is_heartbeat_timeout_.load(); });

		is_heartbeat_timeout_ = false;

		return !result;
	}

	bool election_timeout() {
		std::unique_lock<std::mutex> lock(election_mtx_);
		bool result = election_cond_.wait_for(lock, std::chrono::milliseconds(me_.election_timeout_rand),
			[this] {return is_election_timeout_.load(); });

		is_election_timeout_ = false;

		return !result;
	}

	void set_state(State state) {
		if (state == State::LEADER) {
			me_.current_leader = me_.peer_id;
		}

		me_.state = state;
	}

	void random_election_timeout() {
		me_.election_timeout_rand = ELECTION_TIMEOUT + rand(ELECTION_TIMEOUT);
	}

	template<typename T>
	T rand(T n) {
		std::uniform_int_distribution<T> dis(0, n - 1);
		return dis(g_generator);
	}

private:
	using peer_t = rpc_client;
	rpc_server rpc_server_;
	std::vector<std::shared_ptr<peer_t>> peers_;
	config conf_;

	State state_;
	raft_server_private me_{};

	//check heartbeat timeout
	std::atomic_bool is_heartbeat_timeout_ = false;
	std::mutex heartbeat_mtx_;
	std::condition_variable heartbeat_cond_;

	//check election timeout
	std::atomic_bool is_election_timeout_ = false;
	std::mutex election_mtx_;
	std::condition_variable election_cond_;
};

}