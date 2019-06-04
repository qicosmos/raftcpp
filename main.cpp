#include <iostream>
#include <fstream>
#include <filesystem>
#include <string_view>
#include "node.hpp"
using namespace raftcpp;

/*
#include "raft_server1.hpp"
using namespace raftcpp;

std::pair<bool, config> get_conf(std::string_view path) {
	std::ifstream ifile(path.data(), std::ios::binary);
	if (!ifile) {
		return {};
	}

	auto size = std::filesystem::file_size(path.data());
	std::string str;
	str.resize(size);
	ifile.read(&str[0], size);
	config conf{};
	bool r = iguana::json::from_json(conf, str.data(), str.size());
	if (!r) {
		return {};
	}

	return { true, conf };
}*/

//void test_wait_for_heartbeat() {
//	raft_server server({});
//
//	std::thread thd([&server] {
//		std::this_thread::sleep_for(std::chrono::milliseconds(HEARTBEAT_PERIOD/2));
//		server.set_heartbeat_flag(true);
//	});
//	
//	bool r = server.wait_for_heartbeat();
//	assert(!r);
//	thd.join();
//}

int main() {
	config conf{ {{"127.0.0.1", 9000, 0}, {"127.0.0.1", 9001, 1}, {"127.0.0.1", 9002, 2}} };
	address host{};
	std::vector<address> peers;

	{
		std::string str;
		while (true) {
			std::cin >> str;
			if (str == "stop") {
				break;
			}

			int num = atoi(str.data());
			if ((num==0&&str!="0")||num >= conf.peers_addr.size()) {
				std::cout << "bad config" << std::endl;
				continue;
			}
			
			auto it = conf.peers_addr.begin() + num;
			host = *it;
			conf.peers_addr.erase(it);
			peers = std::move(conf.peers_addr);
			break;
		}
	}
	

	node_t node(host, peers);

	while (true) {
		int connected_num = node.connect_peers(1);
		if (connected_num < (peers.size() + 1) / 2) {
			std::cout << "not enough peers" << std::endl;
		}
		else {
			break;
		}
	}

	node.init();

	node.run();
	//test_wait_for_heartbeat();

	//auto [r, conf] = get_conf("./conf/peers.conf");
	//if (!r) {
	//	std::cout << "parse config file failed" << std::endl;
	//	return -1;
	//}

	//raft_server server(conf);
	//bool some_connected = server.connect_peers(3);
	//if (!some_connected) {
	//	std::cout << "connect peers all failed" << std::endl;
	//}

	//server.main_loop();
	std::string str;
	std::cin >> str;
}