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

config conf{ {{"127.0.0.1", 9000, 0}, {"127.0.0.1", 9001, 1}, {"127.0.0.1", 9002, 2}} };

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


bool reconnect_leader(std::shared_ptr<rpc_client> peer,uint64_t leader_id) {
	assert(leader_id >= 0 && leader_id <= conf.peers_addr.size() - 1);
	peer->close();
	peer->update_addr(conf.peers_addr[leader_id].ip, conf.peers_addr[leader_id].port);
	return peer->connect(3);
}

int main(int argc, char** argv) {
	
	uint16_t port = 0;
	std::cout << "input the port: " << '\n';
	std::cin >> port;
	auto peer = std::make_shared<rpc_client>("127.0.0.1", port);
	std::thread th([peer] {
		peer->run();
		});
	th.detach();

//	std::random_device rd;
//	auto index = leader_id;
/*
step:	
do { index = rd() % conf.peers_addr.size(); } while (dead_nodes.find(index)!=dead_nodes.end());
	if (!(index >= 0 && index <= conf.peers_addr.size() - 1)) {
		std::cout << "no leader in current cluster." << std::endl;
		return -1;
	}
	peer->close();
	peer->update_addr(conf.peers_addr[index].ip, conf.peers_addr[index].port);
	*/
	bool r = peer->connect(3);
	if (!r) {
		std::cout << "connect failed. port =" << peer->port() << std::endl;
		return 0;
	}
	//leader is not right, select leader
	/*
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
	*/
	
//	std::cout << "leader id = " << leader_id << ", server port=" << peer->port()<<'\n';
	
	while (peer->has_connected())
	{
		//send request	
	


			peer->async_call("add_one", [](auto ec, auto data) {
				try
				{
				if (ec) {
					std::cout << "add error" << std::endl;
					return;
				}
				auto result = as<int>(data);
				if (result == -1) {

				}
				std::cout << "result = " << result << std::endl;
				}
				catch (std::exception& ec) {
					std::cout << "exception: " << ec.what() << std::endl;
				}
				}, 1, 1);
		
	
		//std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	dead_nodes.insert(leader_id);
	//peer->close();
	std::this_thread::sleep_for(std::chrono::seconds(1));
//	goto step;

	return 0;
}