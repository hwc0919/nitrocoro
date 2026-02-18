/**
 * @file Generator.h
 * @brief Synchronous generator using co_yield
 */
#pragma once

#include <coroutine>
#include <exception>
#include <iterator>

namespace nitro_coro
{

template <typename T>
class [[nodiscard]] Generator
{
public:
    struct promise_type
    {
        T * value_ = nullptr;
        std::exception_ptr exception_;

        Generator get_return_object() noexcept
        {
            return Generator{ std::coroutine_handle<promise_type>::from_promise(*this) };
        }

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(T & value) noexcept
        {
            value_ = std::addressof(value);
            return {};
        }

        std::suspend_always yield_value(T && value) noexcept
        {
            value_ = std::addressof(value);
            return {};
        }

        void return_void() noexcept {}

        void unhandled_exception() noexcept { exception_ = std::current_exception(); }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    class iterator
    {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T *;
        using reference = T &;

        iterator() noexcept = default;

        explicit iterator(handle_type handle) noexcept
            : handle_(handle) {}

        iterator & operator++()
        {
            handle_.resume();
            if (handle_.done())
                handle_ = nullptr;
            return *this;
        }

        void operator++(int) { ++(*this); }

        reference operator*() const noexcept { return *handle_.promise().value_; }

        pointer operator->() const noexcept { return handle_.promise().value_; }

        bool operator==(const iterator & other) const noexcept { return handle_ == other.handle_; }

        bool operator!=(const iterator & other) const noexcept { return !(*this == other); }

    private:
        handle_type handle_;
    };

    Generator() noexcept = default;

    explicit Generator(handle_type handle) noexcept
        : handle_(handle) {}

    Generator(Generator && other) noexcept
        : handle_(other.handle_)
    {
        other.handle_ = nullptr;
    }

    Generator & operator=(Generator && other) noexcept
    {
        if (this != &other)
        {
            if (handle_)
                handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    Generator(const Generator &) = delete;
    Generator & operator=(const Generator &) = delete;

    ~Generator()
    {
        if (handle_)
            handle_.destroy();
    }

    iterator begin()
    {
        if (handle_)
        {
            handle_.resume();
            if (handle_.done())
                return iterator{};

            if (handle_.promise().exception_)
                std::rethrow_exception(handle_.promise().exception_);
        }
        return iterator{ handle_ };
    }

    iterator end() noexcept { return iterator{}; }

private:
    handle_type handle_;
};

} // namespace nitro_coro
