#include <iostream>
#include <fstream>
#include <filesystem>
#include <string_view>
#include "raft_server.hpp"
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

int main() {
	auto [r, conf] = get_conf("./conf/peers.conf");
	if (!r) {
		std::cout << "parse config file failed" << std::endl;
		return -1;
	}

	raft_server server(conf);
	bool all_connected = server.connect_peers(-1);
	if (!all_connected) {
		std::cout << "connect peers failed" << std::endl;
		return -1;
	}

	std::cout << "all connected" << std::endl;

	server.main_loop();
	std::string str;
	std::cin >> str;
}