#pragma once
#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

/// <summary>
/// A minimal async task providing a simplified Promise-like API.
/// Supports: resolve(), wait(), then(), hasError(), getError().
/// Does NOT handle cancellation, timeouts, or awaiting.
/// </summary>
/// <typeparam name="T">Result type (may be void)</typeparam>
template<typename T>
class LoadTask {
    /// <summary>
    /// Helper trait to detect if a type is a LoadTask.
    /// </summary>
    template<typename T_>
    struct is_load_task : std::false_type {};

    template<typename T_>
    struct is_load_task<LoadTask<T_>> : std::true_type {};

    template<typename T_>
    static constexpr bool is_load_task_v = is_load_task<T>::value;

    struct State {
        mutable std::mutex mutex;
        std::condition_variable cv;
        bool completed = false;
        std::exception_ptr error;
        // For non-void: store result
        std::conditional_t<std::is_void_v<T>, char, std::optional<T>> value;
        // JS Promise semantics: only one continuation stored
        std::move_only_function<void()> continuation;
        /// <summary>
        /// Marks the task as completed with a value.
        /// </summary>
        void resolveValue(auto &&result) {
            std::unique_lock lock(mutex);
            if constexpr (!std::is_void_v<T>) {
                value.emplace(std::forward<decltype(result)>(result));
            }
            completed = true;
            if (continuation) {
                auto cb = std::move(continuation);
                lock.unlock();
                cb();
            } else {
                cv.notify_all();
            }
        }
        /// <summary>
        /// Marks the task as completed with an exception.,
        /// </summary>
        void resolveError(const std::exception_ptr &e) {
            std::unique_lock lock(mutex);
            error = e;
            completed = true;
            if (continuation) {
                auto cb = std::move(continuation);
                lock.unlock();
                cb();
            } else {
                cv.notify_all();
            }
        }
    };
    std::shared_ptr<State> state;

public:
    using value_type = T;

    LoadTask() : state(std::make_shared<State>()) {}
    /// <summary>
    /// Returns true if the task still has a state (not moved-from).
    /// </summary>
    [[nodiscard]] bool valid() const noexcept { return state != nullptr; }
    /// <summary>
    /// Completes the task with a result.
    /// </summary>
    void resolve(auto &&result)
        requires(!std::is_void_v<T>)
    {
        state->resolveValue(std::forward<decltype(result)>(result));
    }
    /// <summary>
    /// Completes void tasks.
    /// </summary>
    void resolve()
        requires std::is_void_v<T>
    {
        state->resolveValue(0); // Dummy
    }
    /// <summary>
    /// Completes the task with an exception.
    /// </summary>
    void resolveError(std::exception_ptr e) { state->resolveError(e); }
    /// <summary>
    /// Returns true if the task completed with an exception.
    /// </summary>
    [[nodiscard]] bool hasError() const {
        std::unique_lock lock(state->mutex);
        return state->completed && state->error != nullptr;
    }
    /// <summary>
    /// Returns the stored exception (if any).
    /// </summary>
    [[nodiscard]] std::exception_ptr getError() const {
        std::unique_lock lock(state->mutex);
        return state->error;
    }
    /// <summary>
    /// Blocks until task completes.
    /// Returns a const pointer to the result or nullptr for void.
    /// </summary>
    [[nodiscard]] const T *wait() const {
        std::unique_lock lock(state->mutex);
        state->cv.wait(lock, [&] { return state->completed; });
        if constexpr (std::is_void_v<T>) {
            return nullptr;
        } else {
            if (!state->value.has_value())
                return nullptr;
            return std::addressof(*state->value);
        }
    }
    /// <summary>
    /// Adds a continuation that transforms the result into a new task.
    /// Non-flattening version for non-LoadTask returns.
    /// </summary>
    template<typename Func>
        requires(!is_load_task_v<std::invoke_result_t<Func, T>>)
    auto then(Func &&f) const {
        using R = std::invoke_result_t<Func, T>;
        LoadTask<R> next;
        auto st = state;
        st->continuation = [st, f = std::forward<Func>(f), next]() mutable {
            if (st->error) {
                next.resolveError(st->error);
                return;
            }
            if constexpr (std::is_void_v<T>) {
                if constexpr (std::is_void_v<R>) {
                    f();
                    next.resolve();
                } else {
                    next.resolve(f());
                }
            } else {
                const T *v = nullptr;
                {
                    std::unique_lock lock(st->mutex);
                    v = std::addressof(*st->value);
                }
                if constexpr (std::is_void_v<R>) {
                    f(*v);
                    next.resolve();
                } else {
                    next.resolve(f(*v));
                }
            }
        };
        // If already done, invoke immediately
        {
            std::unique_lock lock(st->mutex);
            if (st->completed && st->continuation) {
                auto cb = std::move(st->continuation);
                lock.unlock();
                cb();
            }
        }
        return next;
    }

    /// <summary>
    /// Flattening then: when callback returns LoadTask<U>, automatically unwrap it.
    /// </summary>
    template<typename Func>
        requires is_load_task_v<std::invoke_result_t<Func, T>>
    auto then(Func &&f) const {
        using Inner = std::invoke_result_t<Func, T>;
        using R = typename Inner::value_type;

        LoadTask<R> next;
        auto st = state;
        st->continuation = [st, f = std::forward<Func>(f), next]() mutable {
            if (st->error) {
                next.resolveError(st->error);
                return;
            }

            try {
                LoadTask<R> inner;
                if constexpr (std::is_void_v<T>) {
                    inner = f();
                } else {
                    const T *v = nullptr;
                    {
                        std::unique_lock lock(st->mutex);
                        v = std::addressof(*st->value);
                    }
                    inner = f(*v);
                }

                // Chain the inner task to next
                if constexpr (std::is_void_v<R>) {
                    inner.then([next]() mutable { next.resolve(); });
                } else {
                    inner.then([next](const R &result) mutable { next.resolve(result); });
                }
            } catch (...) {
                next.resolveError(std::current_exception());
            }
        };

        // If already done, invoke immediately
        {
            std::unique_lock lock(st->mutex);
            if (st->completed && st->continuation) {
                auto cb = std::move(st->continuation);
                lock.unlock();
                cb();
            }
        }
        return next;
    }

    /// <summary>
    /// Adds a continuation that runs on the specified executor.
    /// Executor must have a post(callable) or execute(callable) method.
    /// </summary>
    template<typename Executor, typename Func>
    auto then_on(Executor &&executor, Func &&f) const {
        using R = std::invoke_result_t<Func, T>;
        LoadTask<R> next;
        auto st = state;
        auto exec = std::forward<Executor>(executor);

        st->continuation = [st, exec = std::move(exec), f = std::forward<Func>(f), next]() mutable {
            // Post the actual work to the executor
            if constexpr (requires { exec.post(std::declval<std::function<void()>>()); }) {
                exec.post([st, f = std::move(f), next]() mutable {
                    if (st->error) {
                        next.resolveError(st->error);
                        return;
                    }
                    if constexpr (std::is_void_v<T>) {
                        if constexpr (std::is_void_v<R>) {
                            f();
                            next.resolve();
                        } else {
                            next.resolve(f());
                        }
                    } else {
                        const T *v = nullptr;
                        {
                            std::unique_lock lock(st->mutex);
                            v = std::addressof(*st->value);
                        }
                        if constexpr (std::is_void_v<R>) {
                            f(*v);
                            next.resolve();
                        } else {
                            next.resolve(f(*v));
                        }
                    }
                });
            } else {
                exec.execute([st, f = std::move(f), next]() mutable {
                    if (st->error) {
                        next.resolveError(st->error);
                        return;
                    }
                    if constexpr (std::is_void_v<T>) {
                        if constexpr (std::is_void_v<R>) {
                            f();
                            next.resolve();
                        } else {
                            next.resolve(f());
                        }
                    } else {
                        const T *v = nullptr;
                        {
                            std::unique_lock lock(st->mutex);
                            v = std::addressof(*st->value);
                        }
                        if constexpr (std::is_void_v<R>) {
                            f(*v);
                            next.resolve();
                        } else {
                            next.resolve(f(*v));
                        }
                    }
                });
            }
        };

        // If already done, invoke immediately
        {
            std::unique_lock lock(st->mutex);
            if (st->completed && st->continuation) {
                auto cb = std::move(st->continuation);
                lock.unlock();
                cb();
            }
        }
        return next;
    }


    /// <summary>
    /// Waits for all tasks to complete. Returns a task that resolves with pointers to all results
    /// when all input tasks resolve, or rejects with the first error encountered.
    /// </summary>
    static LoadTask<std::vector<const T *>> when_all(std::initializer_list<LoadTask<T>> tasks) {
        LoadTask<std::vector<const T *>> result;
        auto taskVec = std::make_shared<std::vector<LoadTask<T>>>(tasks);

        if (taskVec->empty()) {
            result.resolve(std::vector<const T *>{});
            return result;
        }

        auto remaining = std::make_shared<std::atomic<size_t>>(taskVec->size());
        auto errorMutex = std::make_shared<std::mutex>();
        auto firstError = std::make_shared<std::exception_ptr>();

        for (auto &task: *taskVec) {
            auto state = task.state;
            auto cont = [state, result, remaining, errorMutex, firstError, taskVec]() mutable {
                // Capture any error from this task
                if (state->error) {
                    std::lock_guard lock(*errorMutex);
                    if (!*firstError) {
                        *firstError = state->error;
                    }
                }

                // Check if all tasks completed
                if (remaining->fetch_sub(1) == 1) {
                    std::lock_guard lock(*errorMutex);
                    if (*firstError) {
                        result.resolveError(*firstError);
                    } else {
                        // Collect all results as pointers
                        std::vector<const T *> results;
                        results.reserve(taskVec->size());
                        for (const auto &t: *taskVec) {
                            results.push_back(t.wait());
                        }
                        result.resolve(std::move(results));
                    }
                }
            };

            std::unique_lock lock(state->mutex);
            state->continuation = std::move(cont);
            if (state->completed && state->continuation) {
                auto cb = std::move(state->continuation);
                lock.unlock();
                cb();
            }
        }

        return result;
    }

    // /// <summary>
    // /// Waits for all void tasks to complete. Returns a task that resolves when all input tasks
    // /// resolve, or rejects with the first error encountered.
    // /// </summary>
    // static LoadTask<void> when_all(std::initializer_list<LoadTask<void>> tasks) {
    //     LoadTask<void> result;
    //     auto taskVec = std::make_shared<std::vector<LoadTask<void>>>(tasks);
    //
    //     if (taskVec->empty()) {
    //         result.resolve();
    //         return result;
    //     }
    //
    //     auto remaining = std::make_shared<std::atomic<size_t>>(taskVec->size());
    //     auto errorMutex = std::make_shared<std::mutex>();
    //     auto firstError = std::make_shared<std::exception_ptr>();
    //
    //     for (auto &task: *taskVec) {
    //         auto state = task.state;
    //         auto cont = [state, result, remaining, errorMutex, firstError]() mutable {
    //             // Capture any error from this task
    //             if (state->error) {
    //                 std::lock_guard lock(*errorMutex);
    //                 if (!*firstError) {
    //                     *firstError = state->error;
    //                 }
    //             }
    //
    //             // Check if all tasks completed
    //             if (remaining->fetch_sub(1) == 1) {
    //                 std::lock_guard lock(*errorMutex);
    //                 if (*firstError) {
    //                     result.resolveError(*firstError);
    //                 } else {
    //                     result.resolve();
    //                 }
    //             }
    //         };
    //
    //         std::unique_lock lock(state->mutex);
    //         state->continuation = std::move(cont);
    //         if (state->completed && state->continuation) {
    //             auto cb = std::move(state->continuation);
    //             lock.unlock();
    //             cb();
    //         }
    //     }
    //
    //     return result;
    // }
};
