#pragma once
#include <memory>
#include <random>
#include <rpc_server.h>
#include <rpc_client.hpp>
using namespace rest_rpc;
using namespace rpc_service;
#include "common.h"
#include "entity.h"

namespace raftcpp {
	class raft_server {
	public:
		raft_server(const config& conf, size_t thrd_num = 1) : current_peer_(conf.host_addr.port, thrd_num),
			conf_(conf), state_(State::FOLLOWER) {
			current_peer_.register_handler("request_vote", &raft_server::request_vote, this);
			current_peer_.register_handler("append_entry", &raft_server::append_entry, this);
			current_peer_.run();
		}

		void main_loop() {
			while (true) {
				switch (state_) {
				case State::FOLLOWER:
					follower();
					break;
				case State::CANDIDATE:
					candidate();
					break;
				case State::LEADER:
					leader();
					break;
				}
			}
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

		//private:
		void follower() {
			if (wait_for_heartbeat()) {
				if (wait_for_election0()) {
					state_ = State::CANDIDATE;
				}
				else {
					//not election timeout, reset heartbeat flag
					heartbeat_flag_ = false;
				}
			}
		}

		void candidate() {
			current_term_++;
			vote_count_++;
			vote_for_ = conf_.host_id;
			current_leader_ = -1;

			//reset election timer
			election_flag_ = false;
			std::future<bool> future = std::async(std::launch::async, [this] {
				return wait_for_election();
			});

			auto futures = broadcast_request_vote();
			handle_request_vote_response(futures);

			//check if election timeout 
			bool timeout = future.get();
			if (timeout) {
				return;
			}

			//if detect a leader, become follower
			if (current_leader_ != -1 && state_ != State::FOLLOWER) {
				become_follower();
			}
		}

		std::vector<std::future<req_result>> broadcast_request_vote() {
			request_vote_t vote{ current_term_, conf_.host_id, 0, 0 }; //ommit log now, later improve it
			std::vector<std::future<req_result>> futures;
			for (auto& client : peers_) {
				if (!client->has_connected()) {
					continue;
				}

				std::cout << "request vote" << std::endl;
				auto future = client->async_call("request_vote", vote);
				futures.push_back(std::move(future));
			}

			return futures;
		}

		void handle_request_vote_response(std::vector <std::future<req_result>>& futures) {
			for (auto& future : futures) {
				auto status = future.wait_for(std::chrono::milliseconds(ELECTION_TIMEOUT));
				if (status == std::future_status::timeout) {
					continue;
				}

				try {
					auto response = future.get().as<response_vote>();
					if (election_flag_ == false) {
						election_flag_ = true;
						election_cond_.notify_one();
					}

					if (state_ != State::CANDIDATE) {//if not candidate, ommit other response						
						return;
					}

					if (response.term > current_term_) {
						current_leader_ = response.candidate_id; //accept the leader

						become_follower();
						continue;
					}

					if (response.vote_granted) {
						vote_count_ += 1;
						int half = (int)conf_.peers_addr.size() / 2;
						if (vote_count_ >= (half + 1)) { //become leader
							state_ = State::LEADER;
							current_leader_ = conf_.host_id;
						}
					}
				}
				catch (const std::exception & ex) {
					std::cout << ex.what() << std::endl;
				}
			}
		}

		void become_follower() {
			election_flag_ = false;
			heartbeat_flag_ = false;
			state_ = State::FOLLOWER;
			vote_count_ = 0;
		}

		std::vector<std::future<req_result>> broadcast_append_entries() {
			req_append_entry entry{};
			std::vector<std::future<req_result>> futures;
			for (auto& peer : peers_) {
				auto future = peer->async_call("append_entry", entry);
				futures.push_back(std::move(future));
			}

			return futures;
		}

		void handle_append_entries_response(std::vector <std::future<req_result>>& futures) {
			for (auto& future : futures) {
				auto status = future.wait_for(std::chrono::milliseconds(HEARTBEAT_PERIOD));
				if (status == std::future_status::timeout) {
					continue;
				}

				try {
					auto response = future.get().as<res_append_entry>();
					if (heartbeat_flag_ == false) {
						heartbeat_flag_ = true;
						heartbeat_cond_.notify_one();
					}
					std::cout << "broadcast heartbeat" << std::endl;

					if (response.term > current_term_) {
						current_leader_ = -1;
						become_follower();
					}

					//TODO
				}
				catch (const std::exception & ex) {
					std::cout << ex.what() << std::endl;
				}
			}
		}

		void leader() {
			auto futures = broadcast_append_entries(); //broadcast heartbeat to maintain the domain
			handle_append_entries_response(futures);
		}

		//rpc service
		response_vote request_vote(connection* conn, const request_vote_t& args) {
			if (current_leader_ != -1 && !election_flag_) {
				return {};
			}

			if (args.term < current_term_) {
				return {};
			}

			if (state_ == State::CANDIDATE) {
				heartbeat_flag_ = true;
				heartbeat_cond_.notify_one();
				election_flag_ = true;
				election_cond_.notify_one();
			}

			response_vote resp{ current_term_, true, conf_.host_id }; //ommit log now, improve later
			return resp;
		}

		res_append_entry append_entry(connection* conn, const req_append_entry& args) {
			res_append_entry response{ current_term_, false };

			if (state_ == State::FOLLOWER) {
				heartbeat_flag_ = true;
				heartbeat_cond_.notify_one();
			}

			if (args.term < current_term_) {
				return response;
			}

			if (args.term > current_term_) {
				response.term = args.term;
			}

			state_ = State::FOLLOWER;

			if (current_leader_ == -1) {
				current_leader_ = args.id;
			}
			else {
				assert(current_leader_ == args.id);
			}

			response.success = true;

			return response;////ommit log now, improve later
		}

		//true: timeout, false: not timeout
		bool wait_for_heartbeat() {
			assert(state_ == State::FOLLOWER);
			std::unique_lock<std::mutex> lock(heartbeat_mtx_);
			bool result = heartbeat_cond_.wait_for(lock, std::chrono::milliseconds(HEARTBEAT_PERIOD),
				[this] {return heartbeat_flag_.load(); });

			if (!result) {//timeout
				heartbeat_flag_ = true;
			}
			else {
				heartbeat_flag_ = false;
			}

			return heartbeat_flag_;
		}

		bool wait_for_election0() {
			assert(state_ == State::FOLLOWER);
			std::unique_lock<std::mutex> lock(heartbeat_mtx_);
			bool result = heartbeat_cond_.wait_for(lock, std::chrono::milliseconds(HEARTBEAT_PERIOD),
				[this] { return !heartbeat_flag_.load(); });

			return !result;
		}

		bool wait_for_election() {
			assert(state_ == State::CANDIDATE);
			std::unique_lock<std::mutex> lock(heartbeat_mtx_);
			bool result = heartbeat_cond_.wait_for(lock, std::chrono::milliseconds(HEARTBEAT_PERIOD),
				[this] { return current_leader_ != -1; });

			return !result;
		}

		//for test
		void set_heartbeat_flag(bool b) {
			heartbeat_flag_ = b;
			heartbeat_cond_.notify_one();
		}

		using peer = rpc_client;
	private:
		std::atomic<State> state_ = State::FOLLOWER;
		rpc_server current_peer_;
		std::vector<std::shared_ptr<peer>> peers_;
		config conf_;

		std::atomic_bool heartbeat_flag_ = false;
		std::mutex heartbeat_mtx_;
		std::condition_variable heartbeat_cond_;

		std::atomic_bool election_flag_ = false;
		std::mutex election_mtx_;
		std::condition_variable election_cond_;

		uint64_t current_term_ = 0;
		int vote_for_ = -1; //vote who
		int vote_count_ = 0;
		int current_leader_ = -1;
	};
}