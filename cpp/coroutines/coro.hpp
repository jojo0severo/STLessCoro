#pragma once

#include <coroutine>
#include <memory>
#include <vector>

#ifdef __linux__
#define ALWAYS_INLINE __attribute__((always_inline))
#else
#define ALWAYS_INLINE __forceinline
#endif


class Result {
    /**
     * Helper class to reduce code duplication when casting from void* to T
     *
     * */
public:
    /**
     * Get the result casted to the expected type
     * */
    template<typename T>
    ALWAYS_INLINE T result() {
        return (T) *((T *) m_data.get());
    }

    Result(std::shared_ptr<void> &data) noexcept: m_data{data} {}

    ~Result() {
        m_data.reset();
    }

private:
    std::shared_ptr<void> m_data;  /// Void pointer with the returned value from coroutine
};


class Task {
    /**
     * Helper class that will be created when calling the coroutine
     * */
public:
    struct Promise;

    using PromiseHandle = std::coroutine_handle<Promise>;  /// Reduce code writing
    using promise_type = Promise;                          /// Used by compiler boilerplate

    struct Promise {
        /**
         * Task promise used to handle the object from within and outside the coroutine
         * */
        inline Task get_return_object() & noexcept {
            m_done = 0;
            return Task(PromiseHandle::from_promise(*this));
        }

        /**
         * Called immediately after getting the Task (i.e. return object)
         * */
        ALWAYS_INLINE auto initial_suspend() const & noexcept {
            return std::suspend_always{};
        }

        /**
         * Called on co_yield exp; where exp can be of any type
         * but will be ignored
         * */
        ALWAYS_INLINE auto yield_value(const void *&&) &{
            m_next = PromiseHandle::from_promise(*this);
            return std::suspend_always{};
        }

        /**
         * Called on co_return exp; where exp can be of any type
         *
         * It will be stored as a void pointer to be used later
         * */
        template<typename T = void *>
        inline void return_value(T &value) &{
            m_data = std::make_shared<T>(value);
        }

        template<typename T = void *>
        inline void return_value(T &&value) &{
            m_data = std::make_shared<T>(value);
        }

        /**
         * Called after the coroutine executed every statement in its body (i.e. after co_return and return_void)
         * */
        ALWAYS_INLINE auto final_suspend() & noexcept {
            m_done = 1;
            m_next = m_previous;
            return std::suspend_always{};
        }

        /**
         * Called in case the coroutine raise an unhandled exception
         * */
        inline void unhandled_exception() & noexcept {
            m_done = 1;
            std::terminate();
        }

        Promise() noexcept {}

        ~Promise() {
            m_data.reset();
            m_next = nullptr;
            m_previous = nullptr;
        }

        char8_t m_done: 2;             /// Promise state, either finished or in suspended state
        std::shared_ptr<void> m_data;  /// Returned value
        PromiseHandle m_next;             /// Child coroutine (i.e. coroutine from co_await)
        PromiseHandle m_previous;         /// Parent coroutine (i.e. coroutine that called this)
    };

private:
    struct Awaiter {
        /**
         * Called immediately after getting the awaiter
         * */
        ALWAYS_INLINE bool await_ready() const & noexcept {
            return false;
        }

        /**
         * Called if await_ready is false
         * Receives the caller handle
         * */
        inline void await_suspend(PromiseHandle &parent) const & noexcept {
            parent.promise().m_next = m_handle;
            m_handle.promise().m_previous = parent;
        }

        /**
         * Called when the handle is resumed (i.e. handle.resume())
         * Returns the expression saved while executing "co_return expr;"
         * */
        inline Result await_resume() const & noexcept {
            return Result{m_handle.promise().m_data};
        };

        Awaiter(PromiseHandle &h) : m_handle{h} {}

        ~Awaiter() noexcept {
            m_handle = nullptr;
        }

        PromiseHandle &m_handle;  /// Child handle from co_await (i.e. exp.m_handle of "co_await exp;")
    };

public:
    /**
     * Called on every co_await expression inside the coroutine
     * It is kind tricky this one because it will actually be the callee (i.e. child) handle that will
     * invoke this method passing the caller (i.e. parent) handle
     * */
    inline auto operator
    co_await() && noexcept {
        return Awaiter{m_handle};
    }

    Task(PromiseHandle &&handle) noexcept: m_handle{handle} {
        handle = {};
    }

    ~Task() noexcept {
        if (m_handle)
            m_handle.destroy();

        m_handle = nullptr;
    }

    PromiseHandle m_handle;  /// Coroutine handle created from the Promise
};


class Scheduler {
    /**
     * Scheduler for the coroutines
     *
     * It holds the current state of each coroutine to resume when called
     * */
public:
    /**
     * Add a handle to the coroutines vector
     * */
    inline void schedule(Task &task) {
        m_coroutines.push_back(task.m_handle);
        task.m_handle = nullptr;
        ++m_size;
    }

    inline void schedule(Task &&task) {
        m_coroutines.push_back(task.m_handle);
        task.m_handle = nullptr;
        ++m_size;
    }

    inline bool resume() {
        /**
         * Resume each coroutine sequentially, starting from the last suspended point
         *
         * A suspended point is either a co_await expression (will add a child to the coroutine promise)
         * or a co_return (will point back to the parent handle)
         *
         * If the coroutine is done, it is removed from the vector
         * */

        for (size_t i = 0; i < m_size; ++i) {
            Task::PromiseHandle &handle = m_coroutines[i];
            handle.resume();

            Task::Promise &promise = handle.promise();
            if (promise.m_next) {
                m_coroutines[i] = promise.m_next;
                if (promise.m_done)
                    promise.~Promise();
            } else {
                promise.~Promise();
                m_coroutines[i] = std::move(m_coroutines.back());
                m_coroutines.pop_back();
                --m_size;
            }
        }

        return m_size == 0;
    }

private:
    std::vector<Task::PromiseHandle> m_coroutines;  /// Vector with the current root coroutines
    size_t m_size{0};                         /// Current size of the vector (avoid call to vector.size())
};

