#pragma once
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <utility>

namespace triengine::utility
{
    /*
    Thread-safe CRTP singleton template
    Implement based on [Double-Checked Locking Pattern(DCLP)](https://en.wikipedia.org/wiki/Double-checked_locking)
    You need to initialize with parameters or control the destruction order manually
    ```cpp
    class Foo : public atomic_singleton<Foo> {
    public:
        Foo(int n) : n_ { n } {}
        void Bar() {}

    private:
        int n_;
    };

    int main()
    {
        Foo::ConstructInstance(17);
        Foo::GetInstance()->Bar();
        Foo::DestructInstance();
    }
    ```
    */
    template <typename _Derived>
    class atomic_singleton {
    public:
        template <typename... _Args>
        static void ConstructInstance(_Args&&... args)
        {
            struct DerivedDummy final : _Derived {
                using _Derived::_Derived;
                void ProhibitConstructFromDerived() const noexcept override { }
            };

            using InstanceType = DerivedDummy;
    
            if (!instance_.load(std::memory_order_acquire))
            {
                std::lock_guard lk{ mutex_ };

                if (!instance_.load(std::memory_order_relaxed)) {
                    instance_.store(new InstanceType{ std::forward<_Args>(args)... }, std::memory_order_release);
                }
            }
        }
    
        static void DestructInstance()
        {
            if (instance_.load(std::memory_order_acquire))
            {
                std::lock_guard lk{ mutex_ };

                if (auto* instance = instance_.load(std::memory_order_relaxed); instance) {
                    delete instance;
                    instance_.store(nullptr, std::memory_order_release);
                }
            }
        }
    
        static _Derived* GetInstance()
        {
            auto* instance = instance_.load(std::memory_order_acquire);
    
            if (!instance) {
                std::shared_lock lk{ mutex_ };
                instance = instance_.load(std::memory_order_relaxed);
            }
    
            return instance;
        }
    
    protected:
        atomic_singleton() = default;
        atomic_singleton(const atomic_singleton&) = delete;
        atomic_singleton(atomic_singleton&&) = delete;
        atomic_singleton& operator=(const atomic_singleton&) = delete;
        atomic_singleton& operator=(atomic_singleton&&) = delete;
        virtual ~atomic_singleton() = default;
    
    private:
        virtual void ProhibitConstructFromDerived() const noexcept = 0;
    
    private:
        inline static std::atomic<_Derived*> instance_{ nullptr };
        inline static std::shared_mutex mutex_;
    };

} // namespace