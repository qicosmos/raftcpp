#pragma once
#include <cstdint>
#include <vector>
#include <msgpack.hpp>

namespace raftcpp {
	enum class State {
		FOLLOWER,
		CANDIDATE,
		LEADER
	};

	struct entry_t {
		/** the entry's term at the point it was created */
		uint64_t term;

		/** the entry's unique ID */
		uint64_t index;

		/** type of entry */
		int32_t type;

		std::string data;

		MSGPACK_DEFINE(term, index, type, data)
	};

	struct request_vote_t {
		/** currentTerm, to force other leader/candidate to step down */
		uint64_t term;

		/** candidate requesting vote */
		int from;

		/** index of candidate's last log entry */
		uint64_t last_log_idx;

		/** term of candidate's last log entry */
		uint64_t last_log_term;

		MSGPACK_DEFINE(term, from, last_log_idx, last_log_term)
	};

	struct response_vote {
		/** currentTerm, for candidate to update itself */
		uint64_t term = 0;

		/** true means candidate received vote */
		bool vote_granted = false;

		MSGPACK_DEFINE(term, vote_granted)
	};

	struct req_append_entry {
		int from;
		uint64_t term;
		uint64_t prev_log_index;
		uint64_t prev_log_term;
		uint64_t leader_commit_index;
		std::vector<entry_t> entries;
		MSGPACK_DEFINE(from, term, prev_log_index, prev_log_term, leader_commit_index, entries)
	};

	struct res_append_entry {
		int from = 0;
		uint64_t term = 0;
		bool reject = false;
		uint64_t last_log_index = 0;
		uint64_t reject_hint =0 ; //the position of log rejecting
		MSGPACK_DEFINE(from, term, reject, last_log_index, reject_hint);
	};

	struct req_heartbeat {
		int from;
		uint64_t term;
		uint64_t leader_commit_index;
		MSGPACK_DEFINE(from, term, leader_commit_index);
	};

	struct res_heartbeat {
		int from;
		uint64_t term;
		MSGPACK_DEFINE(from, term);
	};

	enum entry_type {
		entry_type_data= 1,
	};
}