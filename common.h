#pragma once
#include <string>
#include <vector>
#include <any>
#include <iguana/json.hpp>
#include "entity.h"
#include "log.hpp"

namespace raftcpp {
	struct address {
		std::string ip;
		int port;
	};
	REFLECTION(address, ip, port);

	struct config {
		int host_id;
		address host_addr;
		std::vector<address> peers_addr;
	};
	REFLECTION(config, host_id, host_addr, peers_addr);

	struct raft_node {
		std::any udata;  /*一般保存与其它机器的连接信息，由使用者决定怎么实现连接*/

		int next_idx; /*对于每一个服务器，需要发送给他的下一个日志条目的索引值（初始化为领导人最后索引值加一）*/
		int match_idx; /*对于每一个服务器，已经复制给他的日志的最高索引值*/

		int flags; /*有三种取值，是相或的关系 1:该机器有给我投票 2:该机器有投票权  3: 该机器有最新的日志*/

		int id; /*机器对应的id值，这个每台机器在全局都是唯一的*/
	};

	struct raft_server_private {
		/* 所有服务器比较固定的状态: */

		/* 服务器最后一次知道的任期号（初始化为 0，持续递增） */
		uint64_t current_term = 0;
		int peer_id; //unique id of the node
		/* 记录在当前分期内给哪个Candidate投过票，
		   */
		int voted_for = -1;
		int voted_count;

		/* 日志条目集；每一个条目包含一个用户状态机执行的指令，和收到时的任期号 */
		log_t log;

		/* 变动比较频繁的变量: */

		/* 已知的最大的已经被提交的日志条目的索引值 */
		uint64_t commit_idx;

		/* 最后被应用到状态机的日志条目索引值（初始化为 0，持续递增） */
		uint64_t last_applied_idx;

		int current_leader = -1;

		/* 三种状态：follower/leader/candidate */
		State state;

		int election_timeout;
		int election_timeout_rand; //随机的选举时间
		int heartbeat_timeout;
	};
}