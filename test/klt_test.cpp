#include <gtest/gtest.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <thread>
#include <vector>

#include "klt.h"
#include "scheduler.h"
#include "ult.h"

namespace {
bool wait_for(const std::atomic<int>& value, int target) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (value.load(std::memory_order_acquire) != target &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return value.load(std::memory_order_acquire) == target;
}

struct YieldState {
    std::atomic<int> other_ran{0};
    std::atomic<int> saw_other{0};
    std::atomic<int> completed{0};
};

void* yielding_entry(void* arg) {
    auto* state = static_cast<YieldState*>(arg);
    sched();
    state->saw_other.store(state->other_ran.load());
    state->completed.fetch_add(1, std::memory_order_release);
    return nullptr;
}

void* other_entry(void* arg) {
    auto* state = static_cast<YieldState*>(arg);
    state->other_ran.store(1);
    state->completed.fetch_add(1, std::memory_order_release);
    return nullptr;
}

void* counting_entry(void* arg) {
    auto* completed = static_cast<std::atomic<int>*>(arg);
    for (int i = 0; i < 4; ++i) sched();
    completed->fetch_add(1, std::memory_order_release);
    return nullptr;
}
}

TEST(UltTest, RejectsInvalidEntryAndStack) {
    EXPECT_EQ(ult_create(16384, nullptr, nullptr), nullptr);
    EXPECT_EQ(ult_create(1, counting_entry, nullptr), nullptr);
    EXPECT_EQ(ult_destroy(nullptr), EINVAL);
    auto* task = ult_create(16384, counting_entry, nullptr);
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(ult_destroy(task), 0);
    EXPECT_EQ(klt_destroy(nullptr), EINVAL);
}

TEST(SchedulerTest, YieldRunsAnotherThreadBeforeResuming) {
    YieldState state;
    auto* first = ult_create(16384, yielding_entry, &state);
    auto* second = ult_create(16384, other_entry, &state);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_EQ(ult_put(first), 0);
    EXPECT_EQ(ult_put(first), EALREADY);
    EXPECT_EQ(ult_destroy(first), EBUSY);
    ASSERT_EQ(ult_put(second), 0);
    klt_t* worker = klt_create();
    ASSERT_NE(worker, nullptr);
    bool completed = wait_for(state.completed, 2);
    EXPECT_TRUE(completed);
    EXPECT_EQ(klt_destroy(worker), 0);
    EXPECT_EQ(state.saw_other.load(), 1);
}

TEST(SchedulerTest, MultipleWorkersCompleteYieldingThreads) {
    constexpr int count = 64;
    std::atomic<int> completed{0};
    std::vector<ult_t*> tasks;
    for (int i = 0; i < count; ++i) {
        auto* task = ult_create(16384, counting_entry, &completed);
        ASSERT_NE(task, nullptr);
        tasks.push_back(task);
    }
    for (auto* task : tasks) ASSERT_EQ(ult_put(task), 0);
    klt_t* first = klt_create();
    klt_t* second = klt_create();
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    bool done = wait_for(completed, count);
    EXPECT_TRUE(done);
    EXPECT_EQ(klt_destroy(first), 0);
    EXPECT_EQ(klt_destroy(second), 0);
}
