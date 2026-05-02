#pragma once
#include <variant>
#include <string>
#include <stdexcept>

// Use std::expected if available (C++23), otherwise polyfill
#if __cpp_lib_expected >= 202202L
#include <expected>
namespace simdtext {
    template<typename T, typename E>
    using expected = std::expected<T, E>;
    using unexpect_t = std::unexpect_t;
    inline constexpr unexpect_t unexpect{};
}
#else
namespace simdtext {
// Minimal expected polyfill
template<typename E>
class unexpected {
public:
    explicit unexpected(E e) : err_(std::move(e)) {}
    const E& error() const& { return err_; }
    E& error() & { return err_; }
    E&& error() && { return std::move(err_); }
private:
    E err_;
};

template<typename T, typename E>
class expected {
public:
    // Value constructor
    template<typename U = T, typename = std::enable_if_t<!std::is_same_v<std::decay_t<U>, expected> && !std::is_same_v<std::decay_t<U>, unexpected<E>>>>
    expected(U&& val) : data_(std::in_place_index<0>, std::forward<U>(val)) {}
    
    // Error constructor
    expected(unexpected<E> err) : data_(std::in_place_index<1>, std::move(err)) {}
    
    bool has_value() const { return data_.index() == 0; }
    explicit operator bool() const { return has_value(); }
    
    T& value() & { if (!has_value()) throw std::runtime_error("bad expected access"); return std::get<0>(data_); }
    const T& value() const& { if (!has_value()) throw std::runtime_error("bad expected access"); return std::get<0>(data_); }
    T&& value() && { if (!has_value()) throw std::runtime_error("bad expected access"); return std::get<0>(std::move(data_)); }
    
    T& operator*() & { return std::get<0>(data_); }
    const T& operator*() const& { return std::get<0>(data_); }
    
    const E& error() const& { return std::get<1>(data_).error(); }
    E& error() & { return std::get<1>(data_).error(); }
    
    T value_or(T&& default_val) const { return has_value() ? **this : std::move(default_val); }
    
private:
    std::variant<T, unexpected<E>> data_;
};

// Void specialization
template<typename E>
class expected<void, E> {
public:
    expected() : data_(std::in_place_index<0>) {}
    expected(unexpected<E> err) : data_(std::in_place_index<1>, std::move(err)) {}
    
    bool has_value() const { return data_.index() == 0; }
    explicit operator bool() const { return has_value(); }
    
    const E& error() const& { return std::get<1>(data_).error(); }
    
private:
    std::variant<std::monostate, unexpected<E>> data_;
};

struct unexpect_t {};
inline constexpr unexpect_t unexpect{};

} // namespace simdtext
#endif
