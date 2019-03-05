#include <iostream>
#include <fstream>
#include <rpc_server.h>
using namespace rest_rpc;
using namespace rpc_service;

#include "client.h"

#include "common.h"

void test() {
	//net_config conf1{ "127.0.0.1", "9000" };
	//net_config conf2{ "127.0.0.1", "9001" };
	//net_config conf3{ "127.0.0.1", "9002" };
	//all_net_config all_net_conf{ {conf1, conf2, conf3} };

	//iguana::string_stream ss;
	//iguana::json::to_json(ss, all_net_conf);
	//std::ofstream out("net_config.json", std::ios::binary);
	//out.write(ss.data(), ss.str().length());

	std::ifstream file("net_config.json", std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		return;
	}
	size_t size = file.tellg();
	std::string str;
	str.resize(size);
	file.seekg(0);
	file.read(&str[0], size);

	all_net_config all_net_conf{};
	iguana::json::from_json(all_net_conf, str.data(), str.size());
	std::cout << all_net_conf.all_net.size() << std::endl;
}

bool port_in_use(unsigned short port) {
	using namespace boost::asio;
	using ip::tcp;

	io_service svc;
	tcp::acceptor a(svc);

	boost::system::error_code ec;
	a.open(tcp::v4(), ec) || a.bind({ tcp::v4(), port }, ec);

	return ec == error::address_in_use;
}

void init() {
	std::ifstream file("net_config.json", std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		return;
	}
	size_t size = file.tellg();
	std::string str;
	str.resize(size);
	file.seekg(0);
	file.read(&str[0], size);

	all_net_config all_net_conf{};
	iguana::json::from_json(all_net_conf, str.data(), str.size());

	for (auto& net : all_net_conf.all_net) {
		if (!port_in_use(net.port)) {
			rpc_server server(net.port, 4);
			server.run();
			std::string str;
			std::cin >> str;
			break;
		}
	}
}

class node {
public:
	node(){
		init_from_config();
	}

	~node() {
		stop_conn_ = true;
		if (thd_)
			thd_->join();
	}

	void start() {
		for (auto& net : all_net_conf_.all_net) {
			if (!port_in_use(net.port)) {
				cur_port_ = net.port;
				std::cout << "current port "<< cur_port_ << std::endl;
				thd_ = std::make_shared<std::thread>([&net] {
					rpc_server server(net.port, 4);
					server.run();
					std::string str;
					std::cin >> str;
				});

				break;
			}
		}

		if (thd_ == nullptr) {
			std::cout << "start failed, all configuration ports have been token" << std::endl;
			throw std::invalid_argument("all ports have been token");
		}

		for (auto& net : all_net_conf_.all_net) {
			if (net.port == cur_port_) {
				continue;
			}

			clients_.push_back(std::make_shared<client>(net));
		}
	}

	void connect_others() {
		for (auto& client : clients_) {
			client->connect();
		}
	}

private:
	void init_from_config() {
		std::ifstream file("net_config.json", std::ios::binary | std::ios::ate);
		if (!file.is_open()) {
			std::cout << "lack of file net_config.json" << std::endl;
			return;
		}
		size_t size = file.tellg();
		std::string str;
		str.resize(size);
		file.seekg(0);
		file.read(&str[0], size);

		iguana::json::from_json(all_net_conf_, str.data(), str.size());
	}

	all_net_config all_net_conf_;
	std::shared_ptr<std::thread> thd_ = nullptr;
	int cur_port_ = 0;
	bool stop_conn_ = false;

	std::vector<std::shared_ptr<client>> clients_;
};

int main() {
	node nd;
	nd.start();
	nd.connect_others();

	std::string str;
	std::cin >> str;
	//init();
	//test();
}