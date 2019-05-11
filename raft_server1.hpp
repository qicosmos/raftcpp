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
				switch (state_) {
				case State::FOLLOWER:
					follower();
					break;
				case State::CANDIDATE:
					on_election_timer();
					break;
				case State::LEADER:
					on_heartbeat_timer_or_send_trigger();
					break;
				}
			}
		}

		//rpc service
		response_vote request_vote(connection* conn, const request_vote_t& args) {
			if (state_ != State::CANDIDATE) {
				return { current_term_, false };
			}

			// step down before handling RPC if need be
			if (args.term > current_term_) {
				current_term_ = args.term;
				state_ = State::FOLLOWER;
				log_state();
				vote_for_ = -1;
			}

			// don't vote for out-of-date candidates
			if (args.term < current_term_) {
				return { current_term_, false };
			}

			// don't double vote
			if (vote_for_ != -1 && vote_for_ != args.candidate_id) {
				return { current_term_, false };
			}

			//check log, do it later
			//...

			vote_for_ = args.candidate_id;

			//reset election timer
			reset_election_timer();

			return { current_term_, true };
		}

		res_append_entry append_entry(connection* conn, const req_append_entry& args) {
			// step down before handling RPC if need be
			if (args.term >= current_term_) {
				current_term_ = args.term;
				state_ = State::FOLLOWER;

				//for log
				auto now = std::chrono::high_resolution_clock::now();
				auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - last_time_);
				if (seconds > std::chrono::seconds(3)) {
					log_state();
					last_time_ = now;
				}

				current_leader_ = args.id;
				reset_election_timer();
				vote_for_ = -1;
			}

			if (args.term < current_term_) {
				return { current_term_, false };
			}

			reset_election_timer();

			//check log, do it later
			//...

			return { current_term_, true };
		}

		void follower() {
			if (wait_for_heartbeat()) {
				if (wait_for_election()) {
					state_ = State::CANDIDATE;
					log_state();
				}
				else {
					//not election timeout, restart to wait for heartbeat
					heartbeat_flag_ = false;

					//for log
					auto now = std::chrono::high_resolution_clock::now();
					auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now - last_time_);
					if (seconds > std::chrono::seconds(3)) {
						log_state();
						last_time_ = now;
					}
				}
			}
		}

		void on_election_timer() {
			if (state_ == State::LEADER) {
				return;
			}

			//maybe no acitve peer
			auto active_num = get_active_num();
			if (active_num == 0) {
				std::cout << "become leader" << std::endl;
				vote_for_ = -1;
				state_ = State::LEADER;
				log_state();
				vote_count_ = 0;
				current_leader_ = conf_.host_id;
				reset_election_timer();
				return;
			}

			current_term_ += 1;
			auto election_term = current_term_;
			vote_for_ = -1;
			state_ = State::CANDIDATE;
			log_state();
			//vote_count_ += 1; //Provote

			auto futures = broadcast_request_vote();
			try {
				for (auto& future : futures) {
					auto status = future.wait_for(std::chrono::milliseconds(RPC_TIMEOUT));
					if (status == std::future_status::timeout) {
						continue;
					}

					auto [term, granted] = future.get().as<response_vote>();
					if (term > current_term_) {
						current_term_ = term;
						state_ = State::FOLLOWER;
						log_state();
						vote_for_ = -1;
					}

					if (granted) {
						vote_count_ += 1;
					}
				}

				if (current_term_ != election_term) {
					return;
				}

				if (vote_count_ + 1 <= conf_.peers_addr.size() / 2) {
					state_ = State::FOLLOWER;
					log_state();
					return;
				}

				vote_count_ += 1;
				state_ = State::LEADER;
				log_state();
				current_leader_ = conf_.host_id;
				reset_election_timer();
				//trigger sending of AppendEntries
				on_heartbeat_timer_or_send_trigger();
			}
			catch (const std::exception & ex) {
				std::cout << ex.what() << std::endl;
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

		void reset_election_timer() {
			election_flag_ = true;
			election_cond_.notify_one();
		}

		void on_heartbeat_timer_or_send_trigger() {
			if (state_ != State::LEADER) {
				return;
			}

			auto send_term = current_term_;
			auto futures = broadcast_append_entries();
			try {
				for (auto& future : futures) {
					auto status = future.wait_for(std::chrono::milliseconds(HEARTBEAT_PERIOD));
					if (status == std::future_status::timeout) {
						continue;
					}

					auto response = future.get().as<res_append_entry>();
					if (response.term > current_term_) {
						current_term_ = response.term;
						state_ = State::FOLLOWER;
						log_state();
						vote_for_ = -1;
					}

					if (current_term_ != send_term) {
						return;
					}

					if (!response.success) {
						//todo
					}
					else {
						//todo
					}
				}
			}
			catch (const std::exception & ex) {
				std::cout << ex.what() << std::endl;
			}
		}

		std::vector<std::future<req_result>> broadcast_append_entries() {
			req_append_entry entry{ conf_.host_id, current_term_ };
			std::vector<std::future<req_result>> futures;
			for (auto& peer : peers_) {
				if (!peer->has_connected()) {
					continue;
				}

				auto future = peer->async_call("append_entry", entry);
				futures.push_back(std::move(future));
			}

			return futures;
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

		bool wait_for_election() {
			assert(state_ != State::LEADER);
			std::unique_lock<std::mutex> lock(heartbeat_mtx_);
			bool result = election_cond_.wait_for(lock, std::chrono::milliseconds(ELECTION_TIMEOUT),
				[this] { return election_flag_.load(); });

			if (!result) {//timeout
				election_flag_ = true;
			}
			else {
				election_flag_ = false;
			}

			return election_flag_;
		}

		//for test
		//void set_heartbeat_flag(bool b) {
		//	heartbeat_flag_ = b;
		//	heartbeat_cond_.notify_one();
		//}

		int get_active_num() {
			return std::count_if(peers_.begin(), peers_.end(), [](auto & peer) {
				return peer->has_connected();
			});
		}

		void log_state() {
			switch (state_) {
			case State::FOLLOWER:
				std::cout << "become FOLLOWER" << std::endl;
				break;
			case State::CANDIDATE:
				std::cout << "become CANDIDATE" << std::endl;
				break;
			case State::LEADER:
				std::cout << "become LEADER" << std::endl;
				break;
			}
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

		//for log
		std::chrono::time_point<std::chrono::high_resolution_clock> last_time_ = std::chrono::high_resolution_clock::now();
	};
}