#pragma once
#include <iguana/json.hpp>

struct net_config {
	std::string ip;
	int port;
};
REFLECTION(net_config, ip, port);

struct all_net_config {
	std::vector<net_config> all_net;
};
REFLECTION(all_net_config, all_net);
