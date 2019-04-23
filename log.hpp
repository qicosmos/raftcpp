#pragma once
#include <deque>
#include "entity.h"

namespace raftcpp {
	class log_t {
	public:
		std::pair<uint64_t, uint64_t> append(const std::vector<entry>& new_entries) {
			uint64_t firstIndex = start_index_ + entries_.size();
			uint64_t lastIndex = firstIndex + new_entries.size() - 1;
			for (auto& entry : new_entries) {
				entries_.push_back(entry);
			}
			return { firstIndex, lastIndex };
		}

		const entry& get_entry(uint64_t log_index) const {
			uint64_t offset = log_index - start_index_;
			return entries_.at(offset);
		}

		uint64_t get_log_start_index() const {
			return start_index_;
		}

		uint64_t get_last_log_index() const {
			return start_index_ + entries_.size() - 1;
		}

	private:
		uint64_t start_index_ = 1;
		std::deque<entry> entries_;
	};
}
