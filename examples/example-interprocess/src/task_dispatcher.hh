#pragma once
#include <type_traits>
#include <functional>
#include <future>
#include <deque>
#include <tuple>
#include <cxlib/utils/spin_lock.hh>

class task_dispatcher
{
private:
    // Ref: https://stackoverflow.com/a/57694904
    template <typename _Ty>
    struct destructive_copy_constructible {
        using this_type = destructive_copy_constructible<_Ty>;
        using value_type = _Ty;

        mutable value_type value;

        destructive_copy_constructible() {}
        destructive_copy_constructible(value_type&& v) : value{ std::move(v) } {}
        destructive_copy_constructible(const this_type& rhs) : value{ std::move(rhs.value) } {}
        destructive_copy_constructible(this_type&& rhs) = default;
        this_type& operator=(const this_type& rhs) = delete;
        this_type& operator=(this_type&& rhs) = delete;
    };

    template <typename _Ty>
    using dcc_t = destructive_copy_constructible<typename std::remove_reference<_Ty>::type>;

    template <typename _Ty>
    static inline dcc_t<_Ty> move_to_dcc(_Ty&& r) {
        return dcc_t<_Ty>(std::move(r));
    }

public:
    task_dispatcher() = default;
    ~task_dispatcher() = default;

    task_dispatcher(const task_dispatcher&) = delete;
    task_dispatcher& operator=(const task_dispatcher&) = delete;

    // Submit a task to be executed later in the main thread
    template <typename Fn, typename... Args>
    auto submit_task(Fn&& fn, Args&&... args)
    {
        using ReturnType = std::invoke_result_t<Fn, Args...>;

        std::promise<ReturnType> prm;
        auto fut = prm.get_future();

        {
            std::unique_lock lk{ _task_q_lock };
            _task_q.emplace_back(
                [prm_dcc = move_to_dcc(prm),
                task_func = std::forward<Fn>(fn),
                task_args = std::make_tuple(std::forward<Args>(args)...)]() mutable
                {
                    try {
                        if constexpr (std::is_void_v<ReturnType>) {
                            std::apply(task_func, task_args);
                            prm_dcc.value.set_value();
                        } else {
                            ReturnType retval = std::apply(task_func, task_args);
                            prm_dcc.value.set_value(std::move(retval));
                        }
                    } catch (...) {
                        prm_dcc.value.set_exception(std::current_exception());
                    }
                });
        }

        return fut;
    }

    // NOTE: This method should be called in the main thread to process tasks
    void dispatch_pending_tasks()
    {
        for (;;)
        {
            std::unique_lock lk{ _task_q_lock };
            if (_task_q.empty()) { break; }
            auto task = std::move(_task_q.front());
            _task_q.pop_front();
            lk.unlock(); // Unlock the lock before executing the task to avoid deadlock

            if (task) {
                task(); // Execute the task
            }
        }
    }

private:
    std::deque<std::function<void()>> _task_q;
    mutable _CXLIB utils::spin_lock _task_q_lock;
};