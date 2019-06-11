#pragma once
#include "entity.h"

namespace raftcpp {
	class mem_log_t {
	public:
		static mem_log_t& get() {
			static mem_log_t instance;
			return instance;
		}

		std::pair<uint64_t, uint64_t> append_may_truncate(const std::vector<entry_t>& new_entries) {
			if (new_entries.empty()) {
				return { last_index(),last_index() };
			}
			auto first_index = new_entries.front().index;
			assert(first_index <= last_index() + 1);
			if (first_index == last_index() + 1) {
				entries_.insert(entries_.end(), new_entries.begin(), new_entries.end());
				return { last_index() - new_entries.size() + 1,last_index() };
			}
			if (first_index <= entries_.front().index) {
				entries_.clear();
				entries_.insert(entries_.begin(), new_entries.begin(), new_entries.end());
				return { last_index() - new_entries.size() + 1,last_index() };
			}
			auto pos = std::find_if(entries_.begin(), entries_.end(), [first_index](const entry_t& entry) {return entry.index == first_index; });
			entries_.erase(pos, entries_.end());
			entries_.insert(entries_.end(), new_entries.begin(), new_entries.end());
			return { last_index() - new_entries.size() + 1,last_index() };
		}

		std::pair<uint64_t, uint64_t> append(const std::vector<const entry_t* >& new_entries) {
			if (new_entries.empty()) {
				return { last_index(),last_index() };
			}
			for (auto it : new_entries) {
				entries_.push_back(*it);
			}
			//LOG_INFO << "after append entries, first_index=" << last_index() - new_entries.size() + 1 << ",last_index=" << last_index();
			return { last_index() - new_entries.size() + 1,last_index() };
		}
		std::pair<uint64_t, uint64_t> append(std::vector<entry_t>& new_entries) {
			auto first_index = start_index_ + entries_.size();
			auto last_index = first_index + new_entries.size() - 1;
			for (auto it : new_entries) {
				entries_.push_back(it);
			}
			//LOG_INFO << "after append entries, first_index=" << first_index << ",last_index=" << last_index;
			return { first_index,last_index };
		}

		uint64_t find_conflict(const std::vector<entry_t>& entries) {
			for (auto& ent : entries) {
				if (ent.term != get_term(ent.index)) {
					if (ent.index <= last_index()) {
						//LOG_INFO << "will cover some entries from:" << ent.index << "\n";
					}
					return ent.index;
				}
			}

			return 0;
		}

		std::vector<entry_t> get_entries(uint64_t next, uint64_t nums = 100) {
			std::vector<entry_t> results;
			if (entries_.empty())
				return results;
			uint64_t offset = entries_.front().index;
			for (int i = 0; i < nums; ++i) {
				uint64_t pos = next - offset + i;
				if (pos >= entries_.size())
					break;
				results.push_back(entries_.at(pos));
			}
			return results;
		}

		const entry_t& get_entry(uint64_t index) const {
			assert(!entries_.empty());
			uint64_t offset = index - start_index();
			return entries_.at(offset);
		}

		uint64_t start_index() const {
			if (entries_.empty())
				return 0;
			return entries_.front().index;
		}

		uint64_t last_index()const {
			if (entries_.empty())
				return 0;
			return start_index() + entries_.size() - 1;
		}

		bool empty() { return entries_.empty(); }

		uint64_t get_term(uint64_t index) {
			if (entries_.empty())
				return 0;
			if (index < entries_.front().index || index > entries_.back().index)
				return 0;
			auto offset = index - entries_.front().index;
			return entries_.at(offset).term;
		}

	private:
		mem_log_t() = default;
		mem_log_t(const mem_log_t&) = delete;
		mem_log_t(mem_log_t&&) = delete;

		uint64_t start_index_ = 1;
		std::deque<entry_t> entries_;
	};
}