#pragma once
#include <rpc_server.h>
#include <rpc_client.hpp>
using namespace rest_rpc;
using namespace rpc_service;
#include <random>

#include "entity.h"
#include "common.h"

namespace raftcpp {
	static std::default_random_engine g_generator;

	class node_t {
	public:
		node_t(const address& host, std::vector<address> peers, size_t thrd_num = 1) : election_timer_(timer_ios_), vote_timer_(timer_ios_),
			heartbeat_timer_(timer_ios_), work_(ios_), timer_work_(timer_ios_), current_peer_(host.port, thrd_num, 0),
			host_addr_(host), peers_addr_(std::move(peers)){
			current_peer_.register_handler("request_vote", &node_t::request_vote, this);
			current_peer_.register_handler("append_entry", &node_t::append_entry, this);
			current_peer_.register_handler("pre_request_vote", &node_t::pre_request_vote, this);
			current_peer_.register_handler("heartbeat", &node_t::heartbeat, this);
			current_peer_.async_run();
		}

		void init() {
			state_ = State::FOLLOWER;
			restart_election_timer(ELECTION_TIMEOUT);
		}

		int connect_peers(size_t timeout = 10) {
			int connected_num = 0;
			for (auto& addr : peers_addr_) {
				auto peer = std::make_shared<rpc_client>(addr.ip, addr.port);
				peer->set_connect_timeout(50);
				peer->set_error_callback([this, peer](boost::system::error_code ec) {
					if (ec) {
						peer->async_reconnect();
					}
				});

				bool r = peer->connect(timeout);
				if (r) {
					connected_num++;					
				}
				else {
					peer->async_reconnect();
				}

				peers_.push_back(peer);
			}

			return connected_num;
		}

		response_vote pre_request_vote(rpc_conn conn, request_vote_t args) {
			std::unique_lock<std::mutex> lock(mtx_);
			response_vote vote = {};
			vote.term = current_term_;

			if (args.term < current_term_) {
				return vote;
			}

			if (check_state()) {
				return vote;
			}

			//todo  for log index
			vote.vote_granted = true;
			return vote;
		}

		bool check_state() {
			if (state_ == State::LEADER) {
				if (active_num() + 1 > (peers_addr_.size() + 1) / 2) {
					return true;
				}
			}
			else if (state_ == State::FOLLOWER) {
				if (leader_id_ != -1 && !election_timeout_) {
					return true;
				}
			}

			return false;
		}

		response_vote request_vote(rpc_conn conn, request_vote_t args) {
			std::unique_lock<std::mutex> lock(mtx_);
			response_vote vote{};
			vote.vote_granted = false;
			std::cout << "当前term：" << current_term_ << " 请求term " << args.term <<'\n';
			do {
				if (args.term < current_term_) {
					break;
				}

				if (args.term > current_term_) {
					if (check_state()) {
						break;
					}
					
					step_down_follower(args.term);
				}

				if (args.term == 0|| vote_for_ != -1) {
					break;
				}

				if ((vote_for_ == -1|| vote_for_==args.from) && args.last_log_idx >= last_log_idx_) {
					vote_for_ = args.from;
					vote.vote_granted = true;
					step_down_follower(args.term);
				}

				//todo member changed
				//log
			} while (0);

			vote.term = current_term_;
			std::cout<<"投票请求来自："<<args.from << " 当前投给谁了："<<vote_for_ <<"granted: "<< vote.vote_granted << '\n';
			return vote;
		}

		int active_num() {
			int num = 0;
			for (auto& peer : peers_) {
				if (peer->has_connected()) {
					num++;
				}
			}

			return num;
		}

		res_heartbeat heartbeat(rpc_conn conn, req_heartbeat args) {
			std::unique_lock<std::mutex> lock(mtx_);
			std::cout << host_addr_.host_id<<" heartbeat from: "<<args.from<<'\n';
			res_heartbeat hb{ host_addr_.host_id, current_term_ };
			if (args.term < current_term_) {
				return hb;
			}

			if (state_ == State::FOLLOWER) {
				reset_leader_id(args.from);
				current_term_ = args.term;
				leader_commit_index_ = args.leader_commit_index;				
				hb.term = current_term_;
				restart_election_timer(random_election());
				return hb;
			}
			else if (state_ == State::CANDIDATE) {
				step_down_follower(current_term_);
				reset_leader_id(args.from);
				leader_commit_index_ = args.leader_commit_index;
				hb.term = current_term_;
				return hb;
			}
			
			return hb;
		}

		res_append_entry append_entry(rpc_conn conn, req_append_entry args) {
			//todo log/progress
			return {};
		}

		void restart_election_timer(int timeout) {
			election_timeout_ = false;
			std::cout << "restart_election_timer\n";
			election_timer_.expires_from_now(std::chrono::milliseconds(timeout));
			election_timer_.async_wait([this](const boost::system::error_code & ec) {
				if (ec) {
					return;
				}

				std::unique_lock<std::mutex> lock(mtx_);
				election_timeout();
			});
		}

		void election_timeout() {
			election_timeout_ = true;
			assert(state_ == State::FOLLOWER);
			//if no other peers form configure, just me, become leader
			if (peers_addr_.empty()) {
				become_candidate();
				return;
			}
			reset_leader_id();
			pre_vote();
		}

		void pre_vote() {
			request_vote_t vote{};
			vote.term = current_term_ + 1;
			vote.last_log_idx = 0;//todo, should from logs

			start_vote(true);
			restart_election_timer(ELECTION_TIMEOUT);
		}

		void become_candidate() {
			boost::system::error_code ignore;
			election_timer_.cancel(ignore);

			std::cout << "become candidate" << std::endl;
			reset_leader_id();
			state_ = State::CANDIDATE;
			current_term_++;
			vote_for_ = host_addr_.host_id;
			restart_vote_timer();

			//const LogId last_log_id = _log_manager->last_log_id(true);

			start_vote();
		}

		void handle_majority(int count, bool is_pre_vote) {
			if (count > (peers_addr_.size() + 1) / 2) {
				if (is_pre_vote) {
					become_candidate();
				}
				else {
					become_leader();
				}
			}
		}

		void start_vote(bool is_pre_vote = false){
			auto counter = std::make_shared<int>(1);

			handle_majority(*counter, is_pre_vote);

			request_vote_t vote{};
			uint64_t term = current_term_;
			vote.term = current_term_;
			vote.last_log_idx = last_log_idx_;
			vote.last_log_term = last_log_term_;
			vote.from = host_addr_.host_id;
			std::string rpc_name = is_pre_vote ? "pre_request_vote" : "request_vote";
			for (auto& peer : peers_) {
				if (!peer->has_connected())
					continue;

				peer->async_call(rpc_name, [this, term, counter, is_pre_vote](boost::system::error_code ec, string_view data) {
					if (ec) {
						//timeout 
						//todo
					}

					auto resp_vote = as<response_vote>(data);
					std::unique_lock<std::mutex> lock(mtx_);
					if (state_ != State::CANDIDATE) {
						return;
					}

					if (current_term_ != term) {
						return;
					}

					if (resp_vote.term > current_term_) {
						step_down_follower(resp_vote.term);
						return;
					}

					if (resp_vote.vote_granted) {
						(*counter)++;
					}
					
					handle_majority(*counter, is_pre_vote);
				}, vote);
			}
		}

		void become_leader() {
			if (state_ != State::CANDIDATE) {
				return;
			}

			std::cout << "become leader" << std::endl;
			vote_timer_.cancel();
			state_ = State::LEADER;
			reset_leader_id(host_addr_.host_id);
			restart_heartbeat_timer();
		}

		void restart_heartbeat_timer() {
			heartbeat_timer_.expires_from_now(std::chrono::milliseconds(HEARTBEAT_PERIOD));
			heartbeat_timer_.async_wait([this](const boost::system::error_code & ec) {
				if (ec) {
					return;
				}

				start_heartbeat();
			});
		}

		void start_heartbeat() {
			std::unique_lock<std::mutex> lock(mtx_);
			req_append_entry entry{};
			entry.from = host_addr_.host_id;
			entry.term = current_term_;
			entry.leader_commit_index = leader_commit_index_;
			//entry.prev_log_index = 
			//entry.prev_log_term = 

			int test = 0;
			for (auto& peer : peers_) {
				if (!peer->has_connected())
					continue;
				std::cout << ++test<<" "<< host_addr_.host_id << " heartbeat to: " << '\n';
				peer->async_call("heartbeat", [this](boost::system::error_code ec, string_view data) {
					if (ec) {
						//timeout 
						//todo
					}

					res_append_entry resp_entry = as<res_append_entry>(data);
					//todo progress
				}, entry);
			}

			restart_heartbeat_timer();
		}

		void restart_vote_timer() {
			vote_timer_.expires_from_now(std::chrono::milliseconds(VOTE_TIMEOUT));
			vote_timer_.async_wait([this](const boost::system::error_code & ec) {
				if (ec) {
					return;
				}

				vote_timeout();
			});
		}

		void vote_timeout() {
			std::unique_lock<std::mutex> lock(mtx_);
			if (state_ != State::CANDIDATE) {
				return;
			}

			step_down_follower(current_term_);
		}

		void step_down_follower(uint64_t term) {
			if (state_ == State::CANDIDATE) {
				vote_timer_.cancel();
			}
			else if (state_ == State::LEADER) {
				heartbeat_timer_.cancel();
			}

			std::cout << "become follower" << std::endl;
			if (term > current_term_) {
				vote_for_ = -1;
			}
			current_term_ = term;
			state_ = State::FOLLOWER;
			restart_election_timer(random_election());
			reset_leader_id();
		}

		void reset_leader_id(int id = -1) {
			leader_id_ = id;
		}

		void run() {
			std::thread thd([this] {timer_ios_.run(); });
			ios_.run();
			thd.join();
		}

		template<typename T>
		T rand(T n) {
			std::uniform_int_distribution<T> dis(0, n - 1);
			return dis(g_generator);
		}

		int random_election() {
			return ELECTION_TIMEOUT + rand(ELECTION_TIMEOUT);
		}

	private:
		State state_;
		int leader_id_ = -1;
		uint64_t current_term_ = 0;
		uint64_t last_log_idx_ = 0;
		uint64_t last_log_term_ = 0;
		uint64_t leader_commit_index_ = 0;
		int vote_for_ = -1;

		rpc_server current_peer_;
		address host_addr_;
		std::vector<address> peers_addr_;

		std::vector<std::shared_ptr<rpc_client>> peers_;

		asio::io_service ios_;
		boost::asio::io_service::work work_;

		asio::io_service timer_ios_;
		boost::asio::io_service::work timer_work_;
		boost::asio::steady_timer election_timer_;
		boost::asio::steady_timer vote_timer_;
		boost::asio::steady_timer heartbeat_timer_;
		bool election_timeout_ = false;

		std::mutex mtx_;
	};
}

