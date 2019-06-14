#pragma once
#include <rpc_server.h>
#include <rpc_client.hpp>
using namespace rest_rpc;
using namespace rpc_service;
#include "common.h"
#include "message_bus.hpp"
#include "mem_log.hpp"
#include "consensus.hpp"
#include "cond.h"
#include "NanoLog.hpp"

namespace raftcpp {
	class nodes_t {
	public:
		nodes_t(const address& host, std::vector<address> peers, consensus& cons, size_t thrd_num = 1) :
			current_peer_(host.port, thrd_num, 0), host_addr_(host), cons_(cons),
			peers_addr_(std::move(peers)), bus_(message_bus::get()) {
			current_peer_.register_handler("request_vote", &nodes_t::request_vote, this);
			current_peer_.register_handler("append_entry", &nodes_t::append_entry, this);
			current_peer_.register_handler("pre_request_vote", &nodes_t::pre_request_vote, this);
			current_peer_.register_handler("heartbeat", &nodes_t::heartbeat, this);
			current_peer_.register_handler("add", &nodes_t::add, this);
			current_peer_.register_handler("ask_leader", &nodes_t::ask_leader, this);
			current_peer_.async_run();

			bus_.subscribe<msg_broadcast_request_vote>(&nodes_t::broadcast_request_vote, this);
			bus_.subscribe<msg_broadcast_request_heartbeat>(&nodes_t::broadcast_request_heartbeat, this);

			bus_.subscribe<msg_active_num>(&nodes_t::active_num, this);
		}

		int connect_peers(size_t timeout = 10) {
			int connected_num = 0;
			for (auto& addr : peers_addr_) {
				if (addr.host_id == host_addr_.host_id)
					continue;
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

				std::thread thd([this, &addr, peer] {
					while (true) {
						std::unique_lock<std::mutex> lock_guard(mtx_);
						bool result = state_changed_.wait_for(lock_guard,std::chrono::seconds(1), [this, &addr, peer] {
							return peer->has_connected() && cons_.state() == State::LEADER && addr.progress.match < mem_log_t::get().last_index();
							});

						if(result)
							send_entries(peer, addr);
					}
					});
				thd.detach();
			}

			return connected_num;
		}

		response_vote pre_request_vote(rpc_conn conn, request_vote_t args) {
			return bus_.send_msg<msg_pre_request_vote, response_vote>(args);
		}

		response_vote request_vote(rpc_conn conn, request_vote_t args) {
			return bus_.send_msg<msg_request_vote, response_vote>(args);
		}

		res_heartbeat heartbeat(rpc_conn conn, req_heartbeat args) {
			return bus_.send_msg<msg_heartbeat, res_heartbeat>(args);
		}

		res_append_entry append_entry(rpc_conn conn, req_append_entry args) {
			//todo log/progress
			return bus_.send_msg<msg_append_entry, res_append_entry>(args);
		}

		res_ask_leader ask_leader(rpc_conn conn) {
			res_ask_leader res;
			if (cons_.state() == State::LEADER) {
				res.is_leader = true;
				res.leader_id = host_addr_.host_id;
			}
			else {
				res.is_leader = false;
				res.leader_id = cons_.leader_id();
			}
			return res;

		}

		int add(rpc_conn conn, int a, int b) {
			//先做异常恢复处理 TODO
			/*
			req_id---log_index from map
			entry = get_log(req_id)
			commit
			apply
			response
			*/
			LOG_INFO << "enter rpc add================";
			auto req_id = conn.lock()->request_id();
			if (cons_.req_log_map().find(req_id) != cons_.req_log_map().end()) {
				auto log_index = cons_.req_log_map()[req_id];
				if (log_index >= cons_.applied_index()) {
					LOG_INFO << "return from pos 1";
					return 2;
				}
				else if (log_index >= cons_.commit_index()) {
					{
						std::unique_lock<std::mutex> lock_guard(mtx_);
						state_changed_.wait(lock_guard, [log_index, this]() {
							return this->cons_.applied_index() >= log_index;
							});
					}
					LOG_INFO << "return from pos 2";
					return 2;
				}
				else {
					{
						std::unique_lock<std::mutex> lock_guard(mtx_);
						state_changed_.wait(lock_guard, [log_index, this]() {
							return this->cons_.commit_index() >= log_index;
							});
					}
					{
						std::unique_lock<std::mutex> lock_guard(mtx_);
						state_changed_.wait(lock_guard, [log_index, this]() {
							return this->cons_.applied_index() >= log_index;
							});
					}
					LOG_INFO << "return from pos 3";
					return 2;
				}
			}
			else {
				auto conn_sp = conn.lock();
				const std::vector<char>& body = conn_sp->body();
				std::string data = std::string(body.begin(), body.end());
				if (!cons_.replicate(req_id, std::move(data))) {
					LOG_INFO << "return from pos 4";
					return -1;
				}

				cons_.wait_apply();
				LOG_INFO << "return from pos 5";
				return 2;

			}




			//to string
			//append log
			//wait for majority commit
			//wait for apply
			//response
		}


		void send_entries(std::shared_ptr<rpc_client> peer, address& addr) {
			//todo progress
			auto& log = mem_log_t::get();
			auto& pr = addr.progress;
			if (!pr.pause && pr.match < log.last_index()) {
				LOG_INFO << "node {id=" << host_addr_.host_id << "} start send entries to node{id=" << addr.host_id << "}";
				std::cout << "should start append entries to (" << addr.host_id << "," << addr.ip << "," << addr.port << ")" << "\n";
				req_append_entry req;
				req.term = cons_.current_term();// current_term();
				req.leader_commit_index = cons_.commit_index();//commit_index();

				req.from = host_addr_.host_id;
				req.prev_log_index = pr.match;
				req.prev_log_term = log.get_term(req.prev_log_index);
				req.entries = std::move(log.get_entries(pr.next));
				if (req.entries.empty())
					return;
				pr.pause = true;
				peer->async_call<100000>("append_entry", [this, &pr, &peer, &addr](boost::system::error_code ec, string_view data, uint64_t) {
					if (ec) {
						//timeout 
						//todo
						std::cout << "async call append_entry timeout!" << std::endl;
						return;
					}
					try {

						auto res_append = as<res_append_entry>(data);
						LOG_INFO << "receive append entries response from node{id=" << addr.host_id << "}, with reject=" << res_append.reject
							<< ",reject_hint=" << res_append.reject_hint << ",last_log_index=" << res_append.last_log_index;
						if (res_append.reject) {
							if (res_append.reject_hint > pr.match) {
								pr.match = res_append.reject_hint;
							}
							
							if (res_append.reject_hint + 1 > pr.next) {
								pr.next = res_append.reject_hint + 1;
							}
							pr.pause = false;
							send_entries(peer, addr);
							//TODO send entry again
						}
						else {
							if (res_append.last_log_index > pr.match) {
								pr.match = res_append.last_log_index;
							}
							if (res_append.last_log_index + 1 > pr.next) {
								pr.next = res_append.last_log_index + 1;
							}
							LOG_INFO << "try to update commit index";
							advance_commit();
							pr.pause = false;
						}
					}
					catch (std::exception & e) {
						std::cout << "append entry got exception:" << e.what() << '\n';
					}

					}, req);
			}
		}

		void advance_commit() {
			std::unique_lock<std::mutex> lock_guard(mtx_);
			if (cons_.state() != State::LEADER)
				return;
			std::vector<uint64_t> vec;
			for (auto& it : peers_addr_) {
				if (it.host_id == host_addr_.host_id)
					continue;
				vec.push_back(it.progress.match);
			}
			vec.push_back(mem_log_t::get().last_index());
			std::sort(vec.begin(), vec.end());
			auto new_commit_index = vec[(vec.size() - 1) / 2];

			if (new_commit_index > cons_.commit_index()) {
				LOG_INFO << "update commit index from " << cons_.commit_index() << " to " << new_commit_index;
				cons_.set_commit_index(new_commit_index);
				state_changed_.notify_all();
			}
		}

		void broadcast_request_vote(bool is_pre_vote, uint64_t term, std::shared_ptr<int> counter, request_vote_t vote) {
			std::string rpc_name = is_pre_vote ? "pre_request_vote" : "request_vote";

			for (auto& peer : peers_) {
				if (!peer->has_connected())
					continue;
				/*
				vote.from = host_addr_.host_id;
				vote.last_log_idx = mem_log_t::get().last_index();
				vote.last_log_term = mem_log_t::get().get_term(vote.last_log_idx);
				vote.term = cons_.current_term();
				*/
				peer->async_call(rpc_name, [this, term, counter, is_pre_vote](boost::system::error_code ec, string_view data, uint64_t) {
					if (ec) {
						//timeout
						//todo
						return;
					}

					auto resp_vote = as<response_vote>(data);
					bus_.send_msg<msg_handle_response_of_request_vote>(resp_vote, term, counter, is_pre_vote);
					}, vote);
			}
		}

		void broadcast_request_heartbeat(req_heartbeat req) {
			//LOG_INFO << "broadcast_heartbeat";
			for (auto& peer : peers_) {
				if (!peer->has_connected())
					continue;
				print("send heartbeat\n");
				req.leader_commit_index = cons_.commit_index();
				req.from = cons_.leader_id();
				
				peer->async_call("heartbeat", [this](boost::system::error_code ec, string_view data, uint64_t) {
					if (ec) {
						//timeout 
						//todo
						return;
					}

					res_heartbeat resp_entry = as<res_heartbeat>(data);
					bus_.send_msg<msg_handle_response_of_request_heartbeat>(resp_entry);
					}, req);
			}
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

	private:
		rpc_server current_peer_;
		address host_addr_;
		std::vector<address> peers_addr_;

		consensus& cons_;

		std::vector<std::shared_ptr<rpc_client>> peers_;
		message_bus& bus_;



		bool stop_check_ = false;
	};
}