#include <iostream>
#include <fstream>
#include <filesystem>
#include <string_view>
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
}

void test_wait_for_heartbeat() {
	raft_server server({});

	std::thread thd([&server] {
		std::this_thread::sleep_for(std::chrono::milliseconds(HEARTBEAT_PERIOD/2));
		server.set_heartbeat_flag(true);
	});
	
	bool r = server.wait_for_heartbeat();
	assert(!r);
	thd.join();
}

int main() {
	//test_wait_for_heartbeat();

	auto [r, conf] = get_conf("./conf/peers.conf");
	if (!r) {
		std::cout << "parse config file failed" << std::endl;
		return -1;
	}

	raft_server server(conf);
	bool some_connected = server.connect_peers(3);
	if (!some_connected) {
		std::cout << "connect peers all failed" << std::endl;
	}

	server.main_loop();
	std::string str;
	std::cin >> str;
}