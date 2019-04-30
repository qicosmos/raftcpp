#pragma once
#include <cstdint>
#include <vector>
#include <msgpack.hpp>

enum class State {
	FOLLOWER,
	CANDIDATE,
	LEADER
};

struct entry {
	/** the entry's term at the point it was created */
	uint64_t term;

	/** the entry's unique ID */
	uint64_t id;

	/** type of entry */
	int32_t type;

	MSGPACK_DEFINE(term, id, type)
};

struct request_vote_t {
	/** currentTerm, to force other leader/candidate to step down */
	uint64_t term;

	 /** candidate requesting vote */
	int candidate_id;

	/** index of candidate's last log entry */
	uint64_t last_log_idx;

	/** term of candidate's last log entry */
	uint64_t last_log_term;

	MSGPACK_DEFINE(term, candidate_id, last_log_idx, last_log_term)
};

struct response_vote {
	/** currentTerm, for candidate to update itself */
	uint64_t term = 0;

	/** true means candidate received vote */
	bool vote_granted = false;

	int candidate_id = -1;

	MSGPACK_DEFINE(term, vote_granted, candidate_id)
};

struct req_append_entry {
	int id;
	uint64_t term;
	uint64_t prev_log_index;
	uint64_t prev_log_term;
	uint64_t leader_commit_index;
	std::vector<entry> entries;
	MSGPACK_DEFINE(id, term, prev_log_index, prev_log_term, leader_commit_index, entries)
};

struct res_append_entry {
	uint64_t term;
	bool success;
	uint64_t current_index;
	uint64_t first_index;
	MSGPACK_DEFINE(term, success, current_index, first_index)
};