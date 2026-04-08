#pragma once

#include "codec.hpp"

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ssilang::net {

namespace detail {

inline void shm_lock(void* ptr) {
    auto* lock_word = static_cast<volatile std::uint32_t*>(ptr);
#ifdef _WIN32
    while (InterlockedExchange(reinterpret_cast<volatile LONG*>(const_cast<std::uint32_t*>(lock_word)), 1) != 0) {
    }
#else
    while (__sync_lock_test_and_set(lock_word, 1) != 0) {
    }
#endif
}

inline void shm_unlock(void* ptr) {
    auto* lock_word = static_cast<volatile std::uint32_t*>(ptr);
#ifdef _WIN32
    InterlockedExchange(reinterpret_cast<volatile LONG*>(const_cast<std::uint32_t*>(lock_word)), 0);
#else
    __sync_lock_release(lock_word);
#endif
}

inline void shm_write_u32_le(char* out, std::uint32_t value) {
    out[0] = static_cast<char>(value & 0xFFu);
    out[1] = static_cast<char>((value >> 8) & 0xFFu);
    out[2] = static_cast<char>((value >> 16) & 0xFFu);
    out[3] = static_cast<char>((value >> 24) & 0xFFu);
}

inline std::uint32_t shm_read_u32_le(const char* in) {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(in[0])) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(in[1])) << 8) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(in[2])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(in[3])) << 24);
}

inline std::string shm_system_error_text() {
#ifdef _WIN32
    return " (err=" + std::to_string(GetLastError()) + ")";
#else
    return " (" + std::string(std::strerror(errno)) + ")";
#endif
}

} // namespace detail

class SharedMemory {
public:
    SharedMemory() = default;
    ~SharedMemory() { close(); }

    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;

    SharedMemory(SharedMemory&& other) noexcept
        : ptr_(other.ptr_), size_(other.size_), name_(std::move(other.name_)), owner_(other.owner_), handle_(other.handle_) {
        other.ptr_ = nullptr;
        other.size_ = 0;
        other.owner_ = false;
        other.handle_ = 0;
    }

    SharedMemory& operator=(SharedMemory&& other) noexcept {
        if (this != &other) {
            close();
            ptr_ = other.ptr_;
            size_ = other.size_;
            name_ = std::move(other.name_);
            owner_ = other.owner_;
            handle_ = other.handle_;
            other.ptr_ = nullptr;
            other.size_ = 0;
            other.owner_ = false;
            other.handle_ = 0;
        }
        return *this;
    }

    static SharedMemory open(const std::string& name, std::size_t size = 4096) {
        if (name.empty()) {
            throw Error("shm name must not be empty");
        }
        if (size < 8) {
            throw Error("shm size must be at least 8 bytes");
        }

        SharedMemory out;
        out.name_ = name;
        out.size_ = size;

#ifdef _WIN32
        const std::string mapping_name = "Local\\sl_shm_" + name;
        HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(size), mapping_name.c_str());
        bool owner = mapping != nullptr && GetLastError() != ERROR_ALREADY_EXISTS;
        if (!mapping) {
            mapping = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, mapping_name.c_str());
            if (!mapping) {
                throw Error("Cannot open shm: " + name + detail::shm_system_error_text());
            }
        }

        void* ptr = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
        if (!ptr) {
            CloseHandle(mapping);
            throw Error("MapViewOfFile failed" + detail::shm_system_error_text());
        }

        out.ptr_ = ptr;
        out.handle_ = reinterpret_cast<std::uintptr_t>(mapping);
        out.owner_ = owner;
#else
        const std::string shm_name = "/sl_shm_" + name;
        bool owner = false;
        int fd = shm_open(shm_name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666);
        if (fd >= 0) {
            owner = true;
            if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
                ::close(fd);
                shm_unlink(shm_name.c_str());
                throw Error("ftruncate failed" + detail::shm_system_error_text());
            }
        } else if (errno == EEXIST) {
            fd = shm_open(shm_name.c_str(), O_RDWR, 0666);
            if (fd < 0) {
                throw Error("shm_open failed" + detail::shm_system_error_text());
            }
        } else {
            throw Error("shm_open failed" + detail::shm_system_error_text());
        }

        void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            ::close(fd);
            throw Error("mmap failed" + detail::shm_system_error_text());
        }

        out.ptr_ = ptr;
        out.handle_ = static_cast<std::uintptr_t>(fd);
        out.owner_ = owner;
#endif
        return out;
    }

    bool is_open() const {
        return ptr_ != nullptr;
    }

    std::size_t size() const {
        return size_;
    }

    const std::string& name() const {
        return name_;
    }

    bool owner() const {
        return owner_;
    }

    void store_payload(std::string_view payload) {
        ensure_open();
        if (payload.size() + 8 > size_) {
            throw Error("shm data too large");
        }
        detail::shm_lock(ptr_);
        detail::shm_write_u32_le(static_cast<char*>(ptr_) + 4, static_cast<std::uint32_t>(payload.size()));
        if (!payload.empty()) {
            std::memcpy(static_cast<char*>(ptr_) + 8, payload.data(), payload.size());
        }
        detail::shm_unlock(ptr_);
    }

    std::optional<std::string> load_payload() const {
        ensure_open();
        detail::shm_lock(ptr_);
        const auto size = detail::shm_read_u32_le(static_cast<const char*>(ptr_) + 4);
        if (size == 0) {
            detail::shm_unlock(ptr_);
            return std::nullopt;
        }
        std::string out(static_cast<const char*>(ptr_) + 8, static_cast<const char*>(ptr_) + 8 + size);
        detail::shm_unlock(ptr_);
        return out;
    }

    void store_net(const Value& value) {
        store_payload(encode_payload(value));
    }

    std::optional<Value> load_net() const {
        const auto payload = load_payload();
        if (!payload.has_value()) {
            return std::nullopt;
        }
        return decode_payload(*payload);
    }

    void clear() {
        ensure_open();
        detail::shm_lock(ptr_);
        detail::shm_write_u32_le(static_cast<char*>(ptr_) + 4, 0);
        detail::shm_unlock(ptr_);
    }

    void close() {
        if (!is_open()) {
            return;
        }
#ifdef _WIN32
        UnmapViewOfFile(ptr_);
        CloseHandle(reinterpret_cast<HANDLE>(handle_));
#else
        munmap(ptr_, size_);
        ::close(static_cast<int>(handle_));
        if (owner_) {
            shm_unlink(("/sl_shm_" + name_).c_str());
        }
#endif
        ptr_ = nullptr;
        size_ = 0;
        owner_ = false;
        handle_ = 0;
        name_.clear();
    }

private:
    void ensure_open() const {
        if (!is_open()) {
            throw Error("Operation on closed shm");
        }
    }

    mutable void* ptr_ = nullptr;
    std::size_t size_ = 0;
    std::string name_;
    bool owner_ = false;
    std::uintptr_t handle_ = 0;
};

} // namespace ssilang::net
