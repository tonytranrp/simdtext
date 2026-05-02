#pragma once
#include <type_traits>
#include <new>
#include <stdexcept>

// Use std::expected if available (C++23), otherwise polyfill
#if defined(__has_include)
#  if __has_include(<expected>)
#    include <expected>
#    if __cpp_lib_expected >= 202202L
#      define SIMDTEXT_HAS_STD_EXPECTED 1
#    endif
#  endif
#endif

#if SIMDTEXT_HAS_STD_EXPECTED
namespace simdtext {
    template<typename T, typename E>
    using expected = std::expected<T, E>;
    using unexpect_t = std::unexpect_t;
    inline constexpr unexpect_t unexpect{};
}
#else
namespace simdtext {

template<typename E>
class unexpected {
public:
    explicit unexpected(E e) noexcept(std::is_nothrow_move_constructible_v<E>) : err_(std::move(e)) {}
    [[nodiscard]] const E& error() const& noexcept { return err_; }
    [[nodiscard]] E& error() & noexcept { return err_; }
    [[nodiscard]] E&& error() && noexcept { return std::move(err_); }
private:
    E err_;
};

template<typename T, typename E>
class expected {
    static_assert(std::is_nothrow_destructible_v<T>, "T must be nothrow destructible");
    static_assert(std::is_nothrow_destructible_v<E>, "E must be nothrow destructible");

public:
    // Value constructor
    template<typename U = T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, expected> && !std::is_same_v<std::decay_t<U>, unexpected<E>>>>
    expected(U&& val) noexcept(std::is_nothrow_constructible_v<T, U&&>)
        : has_val_(true) { ::new(static_cast<void*>(&storage_)) T(std::forward<U>(val)); }

    // Error constructor
    expected(unexpected<E> err) noexcept(std::is_nothrow_move_constructible_v<E>)
        : has_val_(false) { ::new(static_cast<void*>(&storage_)) E(std::move(err.error())); }

    expected(const expected& other) noexcept(std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_copy_constructible_v<E>)
        : has_val_(other.has_val_) {
        if (has_val_) ::new(static_cast<void*>(&storage_)) T(*other);
        else ::new(static_cast<void*>(&storage_)) E(other.error());
    }

    expected(expected&& other) noexcept(std::is_nothrow_move_constructible_v<T> && std::is_nothrow_move_constructible_v<E>)
        : has_val_(other.has_val_) {
        if (has_val_) ::new(static_cast<void*>(&storage_)) T(std::move(*other));
        else ::new(static_cast<void*>(&storage_)) E(std::move(other.error()));
    }

    expected& operator=(const expected&) = delete;
    expected& operator=(expected&&) = delete;

    ~expected() noexcept {
        if (has_val_) (**this).~T();
        else error().~E();
    }

    [[nodiscard]] bool has_value() const noexcept { return has_val_; }
    [[nodiscard]] explicit operator bool() const noexcept { return has_val_; }

    [[nodiscard]] T& value() & { if (!has_val_) throw std::runtime_error("bad expected access"); return **this; }
    [[nodiscard]] const T& value() const& { if (!has_val_) throw std::runtime_error("bad expected access"); return **this; }
    [[nodiscard]] T&& value() && { if (!has_val_) throw std::runtime_error("bad expected access"); return std::move(**this); }

    [[nodiscard]] T& operator*() & noexcept { return *reinterpret_cast<T*>(&storage_); }
    [[nodiscard]] const T& operator*() const& noexcept { return *reinterpret_cast<const T*>(&storage_); }
    [[nodiscard]] T* operator->() noexcept { return reinterpret_cast<T*>(&storage_); }
    [[nodiscard]] const T* operator->() const noexcept { return reinterpret_cast<const T*>(&storage_); }

    [[nodiscard]] const E& error() const& noexcept { return *reinterpret_cast<const E*>(&storage_); }
    [[nodiscard]] E& error() & noexcept { return *reinterpret_cast<E*>(&storage_); }

    [[nodiscard]] T value_or(T&& default_val) const { return has_val_ ? **this : std::move(default_val); }

private:
    alignas(alignof(T) > alignof(E) ? alignof(T) : alignof(E))
    alignas(sizeof(T) > sizeof(E) ? sizeof(T) : sizeof(E))
    unsigned char storage_[sizeof(T) > sizeof(E) ? sizeof(T) : sizeof(E)];
    bool has_val_;
};

// Void specialization
template<typename E>
class expected<void, E> {
public:
    expected() noexcept : has_val_(true) {}
    expected(unexpected<E> err) noexcept(std::is_nothrow_move_constructible_v<E>)
        : has_val_(false) { ::new(static_cast<void*>(&storage_)) E(std::move(err.error())); }

    expected(const expected& other) noexcept(std::is_nothrow_copy_constructible_v<E>)
        : has_val_(other.has_val_) {
        if (!has_val_) ::new(static_cast<void*>(&storage_)) E(other.error());
    }

    expected(expected&& other) noexcept(std::is_nothrow_move_constructible_v<E>)
        : has_val_(other.has_val_) {
        if (!has_val_) ::new(static_cast<void*>(&storage_)) E(std::move(other.error()));
    }

    expected& operator=(const expected&) = delete;
    expected& operator=(expected&&) = delete;

    ~expected() noexcept { if (!has_val_) error().~E(); }

    [[nodiscard]] bool has_value() const noexcept { return has_val_; }
    [[nodiscard]] explicit operator bool() const noexcept { return has_val_; }

    [[nodiscard]] const E& error() const& noexcept { return *reinterpret_cast<const E*>(&storage_); }
    [[nodiscard]] E& error() & noexcept { return *reinterpret_cast<E*>(&storage_); }

private:
    alignas(alignof(E)) unsigned char storage_[sizeof(E)];
    bool has_val_;
};

struct unexpect_t {};
inline constexpr unexpect_t unexpect{};

} // namespace simdtext
#endif
