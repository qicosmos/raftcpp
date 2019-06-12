#include <iostream>
#include "rpc_client.hpp"
#include "entity.h"
using namespace rest_rpc;
using namespace std;
using namespace raftcpp;

void send_add_request(const std::string& ip, uint16_t port) {
	auto peer = std::make_shared<rpc_client>(ip, port);
	bool r = peer->connect(3);
	if (!r) {
		std::cout << "connect failed!" << std::endl;
		return;
	}
	

	int reponse = peer->call<100000,int>("add", 1,1);
	std::cout << "result = " << reponse<<'\n';
}

int main(int argc, char** argv) {
	uint16_t port = 0;
	std::cout << "input leader port:" << '\n';
	std::cin >> port;
	std::cout << "port = " << port << '\n';
	std::string ip = "127.0.0.1";

	int start = 0;
	while (true) {
		std::cout << "enter 1 to start a request to leader;" << '\n';
		cin >> start;
		if (start == 1) {
			send_add_request(ip, port);
		}
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	return 0;
}