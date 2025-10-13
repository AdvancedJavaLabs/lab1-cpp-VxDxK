#ifndef BEDROCK_H
#define BEDROCK_H

#include <optional>
#include <queue>
#include <condition_variable>
#include <type_traits>
#include <mutex>
#include <functional>

namespace br {
template <typename T, typename MutexT = std::mutex>
class Mutex final {
public:
    using ValueType = T;
    using MutexType = MutexT;

    class Guard final {
        friend class Mutex;

    public:
        using ValueType = T;
        using MutexType = MutexT;
        using LockType = std::unique_lock<MutexType>;

        Guard(Guard &&guard) noexcept : value_(guard.value_), lock_(std::move(guard.lock_))
        {
            guard.value_ = nullptr;
        }

        Guard &operator=(Guard &&guard) noexcept
        {
            if (this != &guard) {
                Guard tmp(std::move(guard));
                value_ = tmp.value_;
                lock_ = std::move(tmp.lock_);
            }
            return *this;
        }

        ValueType &operator*()
        {
            return *value_;
        }

        const ValueType &operator*() const
        {
            return *value_;
        }

        ValueType *operator->()
        {
            return value_;
        }

        const ValueType *operator->() const
        {
            return value_;
        }

        LockType &AsLock() & noexcept
        {
            return lock_;
        }

        const LockType &AsLock() const & noexcept
        {
            return lock_;
        }

    private:
        Guard(ValueType *value, LockType &&lock) : value_(value), lock_(std::move(lock)) {}

        ValueType *value_;
        LockType lock_;
    };

    Guard Lock()
    {
        return Guard(&value_, std::unique_lock(mutex_));
    }

    template <typename... Args>
    explicit Mutex(Args &&...args) : value_(std::forward<Args>(args)...)
    {
    }

    Mutex(Mutex &&mutex) noexcept(false) : value_(std::move(*mutex.Lock())) {}

    Mutex &operator=(Mutex &&mutex) noexcept(false)
    {
        if (this != &mutex) {
            std::scoped_lock lock(mutex_, mutex.mutex_);
            std::swap(value_, mutex.value_);
        }
        return *this;
    }

private:
    mutable MutexType mutex_;
    T value_;
};

template <typename T>
Mutex(T) -> Mutex<T>;

template <typename T, typename Container = std::queue<T>>
class UnboundedBlockingQueue final {
public:
    enum class State : uint8_t { RUNNING = 0, STOPPED, FORCE_STOPPED };

    using ValueType = T;

    UnboundedBlockingQueue() : queue_(std::make_pair(State::RUNNING, Container{})) {}
    UnboundedBlockingQueue &operator=(UnboundedBlockingQueue &&) noexcept = delete;
    UnboundedBlockingQueue(UnboundedBlockingQueue &&) noexcept = delete;

    bool Push(ValueType value)
    {
        return Emplace(std::move(value));
    }

    template <typename... Args>
    bool Emplace(Args &&...args)
    {
        auto guard = queue_.Lock();
        if (guard->first != State::RUNNING) {
            return false;
        }
        guard->second.emplace(std::forward<Args>(args)...);
        waiter_.notify_one();
        return true;
    }

    void Stop(State state = State::STOPPED)
    {
        auto guard = queue_.Lock();
        guard->first = state;
        waiter_.notify_all();
    }

    std::optional<ValueType> Pop()
    {
        auto queue = queue_.Lock();
        waiter_.wait(queue.AsLock(), [&] { return queue->first != State::RUNNING || !queue->second.empty(); });

        if (queue->first == State::FORCE_STOPPED) {
            return std::nullopt;
        }

        if (!queue->second.empty()) {
            auto value = std::move(queue->second.front());
            queue->second.pop();
            return value;
        }

        return std::nullopt;
    }

private:
    Mutex<std::pair<State, Container>> queue_;
    std::condition_variable waiter_;
};

class ThreadPool final {
public:
    using Task = std::move_only_function<void()>;

    explicit ThreadPool(std::size_t num_threads = std::thread::hardware_concurrency());
    ThreadPool(ThreadPool &&) noexcept = delete;
    ThreadPool &operator=(ThreadPool &&) noexcept = delete;
    ~ThreadPool();

    void Push(Task &&task);

    template <typename Callable, typename... Args>
    void Push(Callable &&callable, Args &&...args)
    {
        auto args_tuple = std::make_tuple(std::forward<Args>(args)...);
        Task task([callable(std::forward<Callable>(callable)), args(std::move(args_tuple))] mutable {
            std::apply(std::move(callable), std::move(args));
        });
        Push(std::move(task));
    }

private:
    UnboundedBlockingQueue<Task> tasks_;
    std::vector<std::thread> threads_;
};

class WaitGroup final {
public:
    WaitGroup() = default;
    explicit WaitGroup(std::size_t count) : count_(count) {}
    void Add(size_t count);
    void Done();
    void Wait();

private:
    std::size_t count_{0};
    std::size_t waiters_{0};
    std::mutex mutex_;
    std::condition_variable cv_;
};

} // namespace br
#endif // BEDROCK_H
