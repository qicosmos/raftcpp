#pragma once
#include <rpc_server.h>
#include <rpc_client.hpp>
using namespace rest_rpc;
using namespace rpc_service;
#include "common.h"
#include "message_bus.hpp"

namespace raftcpp {
	std::mutex g_print_mtx;
	void print(std::string str) {
		std::unique_lock<std::mutex> lock(g_print_mtx);
		std::cout << str;
	}

	class nodes_t {
	public:
		nodes_t(const address& host, std::vector<address> peers, size_t thrd_num = 1) : current_peer_(host.port, thrd_num, 0),
			host_addr_(host), peers_addr_(std::move(peers)), bus_(message_bus::get()) {
			current_peer_.register_handler("request_vote", &nodes_t::request_vote, this);
			current_peer_.register_handler("append_entry", &nodes_t::append_entry, this);
			current_peer_.register_handler("pre_request_vote", &nodes_t::pre_request_vote, this);
			current_peer_.register_handler("heartbeat", &nodes_t::heartbeat, this);
			current_peer_.async_run();

			bus_.subscribe<msg_broadcast_request_vote>(&nodes_t::broadcast_request_vote, this);
			bus_.subscribe<msg_broadcast_request_heartbeat>(&nodes_t::broadcast_request_heartbeat, this);
			bus_.subscribe<msg_active_num>(&nodes_t::active_num, this);
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

		void broadcast_request_vote(bool is_pre_vote, uint64_t term, std::shared_ptr<int> counter, request_vote_t vote) {
			std::string rpc_name = is_pre_vote ? "pre_request_vote" : "request_vote";

			for (auto& peer : peers_) {
				if (!peer->has_connected())
					continue;

				peer->async_call(rpc_name, [this, term, counter, is_pre_vote](boost::system::error_code ec, string_view data) {
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

		void broadcast_request_heartbeat(req_append_entry entry) {
			for (auto& peer : peers_) {
				if (!peer->has_connected())
					continue;
				print("send heartbeat\n");
				peer->async_call("heartbeat", [this](boost::system::error_code ec, string_view data) {
					if (ec) {
						//timeout 
						//todo
						return;
					}

					res_append_entry resp_entry = as<res_append_entry>(data);
					bus_.send_msg<msg_handle_response_of_request_heartbeat>(resp_entry);
				}, entry);
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

		std::vector<std::shared_ptr<rpc_client>> peers_;
		message_bus& bus_;
	};
}