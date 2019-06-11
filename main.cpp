#include <iostream>
#include <fstream>
#include <filesystem>
#include <string_view>
#include "timer.hpp"
#include "nodes.hpp"
#include "consensus.hpp"
using namespace raftcpp;

struct person {
	std::string foo(int& a) {
		return std::to_string(a);
	}

	void foo1(const double& a) {
		std::cout << a << std::endl;
	}
	int id = 0;
};

template<typename T>
void foo(T t) {
	std::cout << t << std::endl;
}

void foo(int a, double b) {
	std::cout << a + b << std::endl;
}

void foo1(std::string s) {
	std::cout << s << std::endl;
}

void foo3(int a, const std::string& b, std::shared_ptr<int> c) {
	std::cout << "foo2\n";
}

void test_msg_bus() {
	using T = typename function_traits<decltype(&person::foo)>::bare_tuple_type;
	message_bus& bus = message_bus::get();
	bus.subscribe<msg_cancel_vote_timer>(&foo3);
	auto c = std::make_shared<int>(2);
	bus.send_msg<msg_cancel_vote_timer>(1, std::string("b"), c);

	//person p;
	//bus.subscribe<msg_pre_vote>(&person::foo, &p);
	//bus.subscribe<msg_vote>(&person::foo1, &p);
	//bus.subscribe<msg_pre_vote>([] {});

	//std::string s = bus.send_msg<msg_pre_vote, std::string>(2);
	//bus.send_msg<msg_vote>(1.5);

	//bus.subscribe<for_test>(&foo1);
	//bus.send_msg<for_test>(std::string("test"));

	//bus.subscribe<for_test1>([](int t) {
	//	std::cout << t << std::endl;
	//});
	//bus.send_msg<for_test1>(2);
}

int main() {
	//test_msg_bus();
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
	
	timer_t timer;
	consensus cons(host.host_id, (int)peers.size());

	nodes_t nodes(host, peers, cons, 1);

	while (true) {
		int connected_num = nodes.connect_peers(1);
		if (connected_num < (peers.size() + 1) / 2) {
			std::cout << "not enough peers" << std::endl;
		}
		else {
			break;
		}
	}

	//node.init();
	//node.run();

	std::string str;
	std::cin >> str;
}