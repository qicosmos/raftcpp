#pragma once
#include <mutex>
#include <condition_variable>

std::mutex mtx_;
std::condition_variable state_changed_;
std::condition_variable task_changed_;