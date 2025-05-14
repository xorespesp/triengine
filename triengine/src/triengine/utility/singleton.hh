#pragma once
#include <type_traits>
#include <shared_mutex>
#include <mutex>
#include <atomic>
#include <utility>

namespace triengine::utility
{
    /*
    Thread-safe CRTP singleton traits.
    Allows selecting different singleton implementation policies via tag dispatching.

    Policy Types:
    1. Meyer's Singleton: Uses static local variables, thread-safe since C++11.
        - singleton_tag_default: Basic Meyer's Singleton.
        - singleton_tag_default_prevent_construction: Meyer's Singleton + prevents direct construction of the derived class.
    2. DCLP (Double-Checked Locking Pattern) Singleton: Explicit synchronization using std::atomic and mutexes.
        - singleton_tag_dclp: DCLP. Manual lifecycle management with initialize() and deinitialize().
        - singleton_tag_dclp_prevent_construction: DCLP + prevents direct construction of the derived class.

    Usage Example:

    ```cpp
    // 1. Basic Meyer's Singleton (direct construction allowed)
    struct FooMeyer : public singleton_trait<FooMeyer>
    {
        FooMeyer(int id = 0) : _id{ id } { std::cout << name() << "() CALLED" << std::endl; }
        ~FooMeyer() { std::cout << "~" << name() << "() CALLED" << std::endl; }
        void print() const { std::cout << name() << "::print() CALLED" << std::endl; }
        std::string name() const { return "FooMeyer<" + std::to_string(_id) + ">"; }
    private:
        int _id;
    };

    // 2. Meyer's Singleton (direct construction disallowed)
    struct FooMeyerStrict : public singleton_trait<FooMeyerStrict, singleton_tag_meyer_prevent_construction>
    {
        FooMeyerStrict(int id = 0) : _id{id} { std::cout << name() << "() CALLED" << std::endl; }
        ~FooMeyerStrict() { std::cout << "~" << name() << "() CALLED" << std::endl; }
        void print() const { std::cout << name() << "::print() CALLED" << std::endl; }
        std::string name() const { return "FooMeyerStrict<" + std::to_string(_id) + ">"; }
    private:
        int _id;
    };

    // 3. DCLP Singleton (direct construction disallowed)
    struct FooDclpStrict : public singleton_trait<FooDclpStrict, singleton_tag_dclp_prevent_construction>
    {
        FooDclpStrict(int id) : _id{ id } { std::cout << name() << "() CALLED" << std::endl; }
        ~FooDclpStrict() { std::cout << "~" << name() << "() CALLED" << std::endl; }
        void print() const { std::cout << name() << "::print() CALLED" << std::endl; }
        std::string name() const { return "FooDclpStrict<" + std::to_string(_id) + ">"; }
    private:
        int _id;
    };

    int main() {
        FooMeyer x{123}; // OK: FooMeyer can be directly constructed (if constructor is public)
                            // Or controlled with `friend class singleton_trait<FooMeyer>;` and private constructor.
        FooMeyer::instance()->print(); // Internally constructed with id 0 (default constructor)

        //FooMeyerStrict y; // ERROR: 'FooMeyerStrict' is an abstract class. Cannot be directly constructed.
        FooMeyerStrict::instance()->print(); // Internally constructed with id 0

        //FooDclpStrict z{123}; // ERROR: 'FooDclpStrict' is an abstract class. Cannot be directly constructed.
        FooDclpStrict::initialize(456); // Initialize with parameters
        FooDclpStrict::instance()->print();
        FooDclpStrict::deinitialize(); // Manual deinitialization

        // Meyer's singletons are automatically destroyed at program exit.
    }
    ```
    */

    // Define singleton policy tags
    struct singleton_tag_meyer {}; // Meyer's Singleton, default policy
    struct singleton_tag_meyer_prevent_construction {}; // Meyer's Singleton, prevents direct construction of derived class
    struct singleton_tag_dclp {}; // DCLP Singleton, manual initialization/deinitialization
    struct singleton_tag_dclp_prevent_construction {}; // DCLP Singleton, manual init/deinit & prevents direct construction of derived class

    // A basic Meyer's singleton
    // Ref: https://laristra.github.io/flecsi/src/developer-guide/patterns/meyers_singleton.html
    // derived class must be default constructible.
    template <typename _Derived, typename _Tag = singleton_tag_meyer>
    class singleton_trait
    {
    public:
        // Returns the instance. Constructs on first call.
        static _Derived* instance() noexcept(std::is_nothrow_default_constructible<_Derived>::value)
        {
            static _Derived instance_{};
            return &instance_;
        }

    protected:
        singleton_trait() = default;
        singleton_trait(const singleton_trait&) = delete;
        singleton_trait(singleton_trait&&) = delete;
        singleton_trait& operator=(const singleton_trait&) = delete;
        singleton_trait& operator=(singleton_trait&&) = delete;
        ~singleton_trait() = default;
    };

    // Meyer's singleton, prevents direct stack/heap construction of the derived class
    // derived class must be default constructible.
    template <typename _Derived>
    class singleton_trait<_Derived, singleton_tag_meyer_prevent_construction>
    {
    public:
        // Returns the instance. Constructs on first call.
        static _Derived* instance() noexcept(std::is_nothrow_default_constructible<_Derived>::value)
        {
            // Define an internal type that inherits from _Derived to prevent direct construction of _Derived.
            struct DerivedProhibitDirectConstruct final : _Derived {
                using _Derived::_Derived;
                void _prohibit_construct_from_derived() const noexcept override {}
            };

            static DerivedProhibitDirectConstruct instance_{};
            return &instance_;
        }

    protected:
        singleton_trait() = default;
        singleton_trait(const singleton_trait&) = delete;
        singleton_trait(singleton_trait&&) = delete;
        singleton_trait& operator=(const singleton_trait&) = delete;
        singleton_trait& operator=(singleton_trait&&) = delete;
        virtual ~singleton_trait() = default;

    private:
        // This pure virtual function makes singleton_trait an abstract class.
        // If _Derived does not override it, _Derived also becomes abstract, preventing direct construction.
        virtual void _prohibit_construct_from_derived() const noexcept = 0;
    };

    // DCLP(Double-Checked Locking Pattern) Singleton
    // Ref: https://en.wikipedia.org/wiki/Double-checked_locking
    // Requires manual calls to initialize() and deinitialize().
    // derived class must be constructible with arguments passed to initialize.
    template <typename _Derived>
    class singleton_trait<_Derived, singleton_tag_dclp>
    {
    public:
        // Initializes the singleton instance with the given arguments. (thread-safe)
        template <typename... _Args>
        static void initialize(_Args&&... args)
        {
            // First check (without lock)
            if (!_instance.load(std::memory_order_acquire)) {
                std::scoped_lock lk{ _mutex }; // Exclusive lock
                // Second check (inside lock)
                if (!_instance.load(std::memory_order_relaxed)) {
                    _instance.store(new _Derived{ std::forward<_Args>(args)... }, std::memory_order_release);
                }
            }
        }

        // Deinitializes the singleton instance. (thread-safe)
        static void deinitialize()
        {
            // First check (without lock)
            if (_instance.load(std::memory_order_acquire)) {
                std::scoped_lock lk{ _mutex }; // Exclusive lock
                // Second check (inside lock)
                if (auto* instance_ptr = _instance.load(std::memory_order_relaxed);
                    instance_ptr) {
                    delete instance_ptr;
                    _instance.store(nullptr, std::memory_order_release);
                }
            }
        }

        // Returns the current instance pointer. (thread-safe)
        // Note: May return nullptr if initialize() has not been called or after deinitialize()
        static _Derived* instance()
        {
            auto* instance_ptr = _instance.load(std::memory_order_acquire);
            // If instance_ptr is nullptr, and another thread might be initializing/deinitializing,
            // use the mutex to ensure reading the value after that operation completes.
            if (!instance_ptr) {
                std::shared_lock lk{ _mutex }; // Shared lock (for reading purposes)
                instance_ptr = _instance.load(std::memory_order_relaxed);
            }
            return instance_ptr;
        }

    protected:
        singleton_trait() = default;
        singleton_trait(const singleton_trait&) = delete;
        singleton_trait(singleton_trait&&) = delete;
        singleton_trait& operator=(const singleton_trait&) = delete;
        singleton_trait& operator=(singleton_trait&&) = delete;
        ~singleton_trait() = default;

    private:
        inline static std::atomic<_Derived*> _instance{ nullptr };
        inline static std::shared_mutex _mutex;
    };

    // DCLP Singleton, prevents direct stack/heap construction of the derived class
    // Requires manual calls to initialize() and deinitialize().
    // derived class must be constructible with arguments passed to initialize.
    template <typename _Derived>
    class singleton_trait<_Derived, singleton_tag_dclp_prevent_construction>
    {
    public:
        // Initializes the singleton instance with the given arguments. (thread-safe)
        template <typename... _Args>
        static void initialize(_Args&&... args)
        {
            // Define an internal type that inherits from _Derived to prevent direct construction of _Derived.
            struct DerivedProhibitDirectConstruct final : _Derived {
                using _Derived::_Derived;
                void _prohibit_construct_from_derived() const noexcept override {}
            };

            // First check (without lock)
            if (!_instance.load(std::memory_order_acquire)) {
                std::scoped_lock lk{ _mutex }; // Exclusive lock
                // Second check (inside lock)
                if (!_instance.load(std::memory_order_relaxed)) {
                    _instance.store(new DerivedProhibitDirectConstruct{ std::forward<_Args>(args)... }, std::memory_order_release);
                }
            }
        }

        // Deinitializes the singleton instance. (thread-safe)
        static void deinitialize()
        {
            // First check (without lock)
            if (_instance.load(std::memory_order_acquire)) {
                std::scoped_lock lk{ _mutex }; // Exclusive lock
                // Second check (inside lock)
                if (auto* instance_ptr = _instance.load(std::memory_order_relaxed);
                    instance_ptr) {
                    delete instance_ptr;
                    _instance.store(nullptr, std::memory_order_release);
                }
            }
        }

        // Returns the current instance pointer. (thread-safe)
        // Note: May return nullptr if initialize() has not been called or after deinitialize()
        static _Derived* instance()
        {
            auto* instance_ptr = _instance.load(std::memory_order_acquire);
            // If instance_ptr is nullptr, and another thread might be initializing/deinitializing,
            // use the mutex to ensure reading the value after that operation completes.
            if (!instance_ptr) {
                std::shared_lock lk{ _mutex }; // Shared lock (for reading purposes)
                instance_ptr = _instance.load(std::memory_order_relaxed);
            }
            return instance_ptr;
        }

    protected:
        singleton_trait() = default;
        singleton_trait(const singleton_trait&) = delete;
        singleton_trait(singleton_trait&&) = delete;
        singleton_trait& operator=(const singleton_trait&) = delete;
        singleton_trait& operator=(singleton_trait&&) = delete;
        virtual ~singleton_trait() = default;

    private:
        // This pure virtual function makes singleton_trait an abstract class.
        // If _Derived does not override it, _Derived also becomes abstract, preventing direct construction.
        virtual void _prohibit_construct_from_derived() const noexcept = 0;

    private:
        inline static std::atomic<_Derived*> _instance{ nullptr }; // NOTE: actually stores a pointer to the DerivedProhibitDirectConstruct
        inline static std::shared_mutex _mutex;
    };

} // namespace