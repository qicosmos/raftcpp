#include <iostream>
#include <random>
#include "rpc_client.hpp"
#include "entity.h"
#include "common.h"
#include <unordered_map>

using namespace rest_rpc;
using namespace std;
using namespace raftcpp;

std::unordered_map<uint64_t, std::string> g_req_body_map;

config conf{ {{"127.0.0.1", 9000, 0}, {"127.0.0.1", 9001, 1}, {"127.0.0.1", 9002, 2}, {"127.0.0.1", 9003, 3}, {"127.0.0.1", 9004, 4}} };

std::set<int64_t> dead_nodes;
int64_t leader_id = -1;

void try_find_leader(std::shared_ptr<rpc_client> peer) {
	bool connect_leader = false;
	while (!connect_leader) {
		auto res = peer->call<20000, res_ask_leader>("ask_leader");
		if (!res.is_leader) {
			// find the leader 
			if (res.leader_id != -1) {
				if (!(res.leader_id >= 0 && res.leader_id <= conf.peers_addr.size() - 1)) {
					std::cout << "no leader in current cluster." << std::endl;
					return ;
				}
				auto new_addr = conf.peers_addr[res.leader_id].ip;
				auto new_port = conf.peers_addr[res.leader_id].port;
				peer->update_addr(new_addr, new_port);
				peer->close();
				bool r = peer->connect(3);
				if (!r) {
					std::cout << "connect new addr {" << new_addr << "," << new_port << "} failed!" << '\n';
					return;
				}
				leader_id = res.leader_id;
				std::cout << "connect new addr {" << new_addr << "," << new_port << "} successfully!" << '\n';
				return;
			}
			else {
				std::cout << "no leader in current cluster!" << '\n';
				return;
			}
		}
		else {
			connect_leader = true;
			std::cout << "connect leader {id= " << res.leader_id << "} successful!\n";
			break;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(200));
	}
}

void send_add_request(std::shared_ptr<rpc_client> peer) {
	//std::default_random_engine engine;
	//std::uniform_int_distribution<unsigned> u(0, conf.peers_addr.size());
	
	try_find_leader(peer);
	auto ret = peer->async_call<10000>("add", [peer](auto ec, auto data, auto req_id) {
		if (ec) {
			std::cout << "add error" << std::endl;

			return;
		}

		int result = as<int>(data);
		std::cout << "result=" << result << std::endl;
		}, 1, 2);
	g_req_body_map.insert({ ret.first,ret.second});
}

bool reconnect_leader(std::shared_ptr<rpc_client> peer,uint64_t leader_id) {
	assert(leader_id >= 0 && leader_id <= conf.peers_addr.size() - 1);
	peer->close();
	peer->update_addr(conf.peers_addr[leader_id].ip, conf.peers_addr[leader_id].port);
	return peer->connect(3);
}

int main(int argc, char** argv) {
	
	auto peer = std::make_shared<rpc_client>("127.0.0.1", 9000);
	std::thread th([peer] {
		peer->run();
		});
	th.detach();

	std::random_device rd;
	auto index = leader_id;
step:	
	do { index = rd() % conf.peers_addr.size(); } while (dead_nodes.find(index)!=dead_nodes.end());
	if (!(index >= 0 && index <= conf.peers_addr.size() - 1)) {
		std::cout << "no leader in current cluster." << std::endl;
		return -1;
	}
	peer->close();
	peer->update_addr(conf.peers_addr[index].ip, conf.peers_addr[index].port);
	bool r = peer->connect(3);
	if (!r) {
		std::cout << "connect failed. port =" << peer->port() << std::endl;
		return 0;
	}
	//leader is not right, select leader
	auto result = peer->call<res_ask_leader>("ask_leader");
	if (!result.is_leader) {
		if (!(result.leader_id >= 0 && result.leader_id <= conf.peers_addr.size() - 1)) {
			std::cout << "no leader in current cluster." << std::endl;
			return -1;
		}
		leader_id = result.leader_id;
		if (!reconnect_leader(peer, leader_id)){
			std::cout << "connect failed,port=" <<peer->port()<< std::endl;
			return -1;
		}
	}
	else {
		leader_id = result.leader_id;
		if (!peer->connect(3)) {
			std::cout << "connect fail,port=" << peer->port()<< std::endl;
			return 0;
		}
	}
	
	std::cout << "leader id = " << leader_id << ", server port=" << peer->port()<<'\n';
	
	while (peer->has_connected())
	{
		//send request
		if (g_req_body_map.empty()) {
			
			auto ret = peer->async_call<10000>("add", [peer](auto ec, auto data, auto req_id) {
				if (ec) {
					std::cout << "add error" << std::endl;
					return;
				}
				auto result = as<int>(data);
				g_req_body_map.clear();
				std::cout << "result = " << result << std::endl;
				}, 1, 1);
			g_req_body_map.insert({ ret.first,ret.second });
		}
		else {
			auto req_id = (g_req_body_map.begin())->first;
			auto body = (g_req_body_map.begin())->second;
			peer->re_call<10000>(req_id, "add", [peer](auto ec, auto data, auto req_id) {
				if (ec) {
					std::cout << "add error" << std::endl;
					return;
				}
				auto result = as<int>(data);
				std::cout << "result = " << result << std::endl;
				g_req_body_map.clear();
				}, body);
		}
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}
	dead_nodes.insert(leader_id);
	peer->close();
	std::this_thread::sleep_for(std::chrono::seconds(1));
	goto step;
	//leader down
/*
	peer->close();
	
	auto new_index = leader_id;
	do { new_index = rd()%conf.peers_addr.size(); } while (new_index ==leader_id);
	assert(new_index >= 0 && new_index <= conf.peers_addr.size() - 1);
	peer->update_addr(conf.peers_addr[new_index].ip, conf.peers_addr[new_index].port);

	auto original_index = rd() % conf.peers_addr.size();
	std::cout << "random server id: " << original_index << std::endl;;
	std::cout << "will connect to ip = " << conf.peers_addr[original_index].ip << ", port = " << conf.peers_addr[original_index].port << std::endl;
	peer->update_addr(conf.peers_addr[original_index].ip, conf.peers_addr[original_index].port);

	peer->set_error_callback([peer,  &rd](auto ec) {
		if (ec) {
		
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			uint64_t new_index = 0;
			do { new_index = rd() % conf.peers_addr.size(); } while (new_index == leader_id);
			assert(new_index >= 0 && new_index <= 2);
			peer->update_addr(conf.peers_addr[new_index].ip, conf.peers_addr[new_index].port);
			peer->async_connect();
			bool r = peer->connect(3);
			if (!r) {
				std::cout << "connect failed!" << std::endl;
				return;
			}
			try_find_leader(peer);
			if (!g_req_body_map.empty()) {
				for (auto m : g_req_body_map) {
					auto req_id = m.first;
					auto body = m.second;
					peer->re_call<10000>(req_id, "add", [peer](auto ec, auto data, auto req_id) {
						if (ec) {
						}
						if (g_req_body_map.find(req_id) != g_req_body_map.end()) {
							g_req_body_map.erase(g_req_body_map.find(req_id));
						}
						int result = as<int>(data);
						std::cout << "result=" << std::endl;
						}, body);
				}
			}
			
			peer->async_reconnect();//normal
		}
		});
	bool r = peer->connect(3);
	if (!r) {
		std::cout << "connect failed!" << std::endl;
		return -1;
	}
	while (true) {
		std::cout << "enter 1 to start a request to leader;" << '\n';
		cin >> start;
		if (start == 1) {
			send_add_request(peer);
		}
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	*/
	return 0;
}