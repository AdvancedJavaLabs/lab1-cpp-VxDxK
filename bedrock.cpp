#include "bedrock.h"


namespace br {
ThreadPool::ThreadPool(std::size_t num_threads)
{
    threads_.reserve(num_threads);
    for (std::size_t i = 0; i < num_threads; ++i) {
        threads_.emplace_back([this]() {
            while (auto task = tasks_.Pop()) {
                (*task)();
            }
        });
    }
}
ThreadPool::~ThreadPool()
{
    tasks_.Stop();
    for (auto &&thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}
void ThreadPool::Push(Task &&task)
{
    tasks_.Push(std::move(task));
}

void WaitGroup::Add(size_t count)
{
    std::unique_lock l(mutex_);
    count_ += count;
}
void WaitGroup::Done()
{
    std::unique_lock l(mutex_);
    count_ -= 1;
    if (count_ == 0 && waiters_ > 0) {
        cv_.notify_all();
    }
}
void WaitGroup::Wait()
{
    std::unique_lock l(mutex_);
    ++waiters_;
    cv_.wait(l, [&]() { return count_ == 0; });
    --waiters_;
}

}