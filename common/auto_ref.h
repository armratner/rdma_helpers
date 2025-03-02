#pragma once

#include <utility>
#include <cstdlib>
#include <stdexcept>

template <typename T>
class auto_ref {
public:
    auto_ref() noexcept : m_ptr{new T}, m_owns{true} 
    {}

    auto_ref(T& value) noexcept : m_ptr{&value}, m_owns{false}
    {}

    auto_ref(const auto_ref& other) 
        : m_ptr{other.m_owns ? new T(*other.m_ptr) : other.m_ptr}
        , m_owns{other.m_owns}
    {}

    auto_ref(auto_ref&& other) noexcept 
        : m_ptr{std::exchange(other.m_ptr, nullptr)}
        , m_owns{std::exchange(other.m_owns, false)}
    {}

    template <typename U>
    auto_ref(auto_ref<U>&& other) noexcept 
        : m_ptr{std::exchange(other.m_ptr, nullptr)}
        , m_owns{std::exchange(other.m_owns, false)}
    {}

    ~auto_ref() noexcept {
        if (m_owns && m_ptr) {
            delete m_ptr;
        }
    }

    auto_ref& operator=(const auto_ref& other) {
        if (this != &other) {
            if (m_owns && m_ptr) {
                delete m_ptr;
            }
            m_ptr = other.m_owns ? new T(*other.m_ptr) : other.m_ptr;
            m_owns = other.m_owns;
        }
        return *this;
    }

    auto_ref& operator=(auto_ref&& other) noexcept {
        if (this != &other) {
            if (m_owns && m_ptr) {
                delete m_ptr;
            }
            m_ptr = std::exchange(other.m_ptr, nullptr);
            m_owns = std::exchange(other.m_owns, false);
        }
        return *this;
    }

    template <typename U>
    auto_ref& operator=(auto_ref<U>&& other) noexcept {
        if (m_owns && m_ptr) {
            delete m_ptr;
        }
        m_ptr = std::exchange(other.m_ptr, nullptr);
        m_owns = std::exchange(other.m_owns, false);
        return *this;
    }

    operator T*() const noexcept {
        return m_ptr;
    }

    T& operator*() const {
        if (!m_ptr) {
            throw std::runtime_error("Null pointer dereference");
        }
        return *m_ptr;
    }

    T* operator->() const {
        if (!m_ptr) {
            throw std::runtime_error("Null pointer dereference");
        }
        return m_ptr;
    }

    T* release() noexcept {
        m_owns = false;
        return std::exchange(m_ptr, nullptr);
    }

    void reset(T* ptr = nullptr) noexcept {
        if (m_owns && m_ptr) {
            delete m_ptr;
        }
        m_ptr = ptr;
        m_owns = (ptr != nullptr);
    }

    void swap(auto_ref& other) noexcept {
        std::swap(m_ptr, other.m_ptr);
        std::swap(m_owns, other.m_owns);
    }

    T* get() const noexcept {
        return m_ptr;
    }

    bool owns_pointer() const noexcept {
        return m_owns;
    }

    T* clone() const {
        return m_ptr ? new T(*m_ptr) : nullptr;
    }

    bool operator==(const T* ptr) const noexcept { return m_ptr == ptr; }
    bool operator!=(const T* ptr) const noexcept { return m_ptr != ptr; }
    bool operator<(const T* ptr) const noexcept { return m_ptr < ptr; }
    bool operator>(const T* ptr) const noexcept { return m_ptr > ptr; }
    bool operator<=(const T* ptr) const noexcept { return m_ptr <= ptr; }
    bool operator>=(const T* ptr) const noexcept { return m_ptr >= ptr; }

private:
    T* m_ptr;
    bool m_owns;
};