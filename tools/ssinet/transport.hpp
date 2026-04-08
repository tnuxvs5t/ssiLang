#pragma once

#include "codec.hpp"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace ssilang::net {

namespace detail {

struct Endpoint {
    std::string host;
    int port = 0;
};

inline Endpoint parse_endpoint(const std::string& endpoint) {
    const auto colon = endpoint.rfind(':');
    if (colon == std::string::npos) {
        throw Error("Invalid endpoint: " + endpoint);
    }

    Endpoint out;
    out.host = endpoint.substr(0, colon);
    try {
        out.port = std::stoi(endpoint.substr(colon + 1));
    } catch (const std::exception&) {
        throw Error("Invalid endpoint: " + endpoint);
    }

    if (out.port < 1 || out.port > 65535) {
        throw Error("Port out of range: " + std::to_string(out.port));
    }
    return out;
}

inline void write_u32_le(char* out, std::uint32_t value) {
    out[0] = static_cast<char>(value & 0xFFu);
    out[1] = static_cast<char>((value >> 8) & 0xFFu);
    out[2] = static_cast<char>((value >> 16) & 0xFFu);
    out[3] = static_cast<char>((value >> 24) & 0xFFu);
}

inline std::uint32_t read_u32_le(const char* in) {
    return static_cast<std::uint32_t>(static_cast<unsigned char>(in[0])) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(in[1])) << 8) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(in[2])) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(in[3])) << 24);
}

inline std::string system_error_text() {
#ifdef _WIN32
    return " (err=" + std::to_string(WSAGetLastError()) + ")";
#else
    return " (" + std::string(std::strerror(errno)) + ")";
#endif
}

class SocketSystem {
public:
    SocketSystem() {
#ifdef _WIN32
        static bool started = false;
        if (!started) {
            WSADATA data{};
            const int rc = WSAStartup(MAKEWORD(2, 2), &data);
            if (rc != 0) {
                throw Error("WSAStartup failed (err=" + std::to_string(rc) + ")");
            }
            started = true;
        }
#endif
    }
};

#ifdef _WIN32
using socket_handle_t = SOCKET;
using pipe_handle_t = HANDLE;

inline socket_handle_t invalid_socket_handle() {
    return INVALID_SOCKET;
}

inline bool is_invalid_pipe_handle(pipe_handle_t handle) {
    return handle == nullptr || handle == INVALID_HANDLE_VALUE;
}

inline void close_socket(socket_handle_t handle) {
    if (handle != INVALID_SOCKET) {
        closesocket(handle);
    }
}

inline void close_pipe(pipe_handle_t handle) {
    if (!is_invalid_pipe_handle(handle)) {
        CloseHandle(handle);
    }
}
#else
using socket_handle_t = int;
using pipe_handle_t = int;

inline socket_handle_t invalid_socket_handle() {
    return -1;
}

inline bool is_invalid_pipe_handle(pipe_handle_t handle) {
    return handle < 0;
}

inline void close_socket(socket_handle_t handle) {
    if (handle >= 0) {
        ::close(handle);
    }
}

inline void close_pipe(pipe_handle_t handle) {
    if (handle >= 0) {
        ::close(handle);
    }
}
#endif

inline void send_all_socket(socket_handle_t handle, const char* data, std::size_t size) {
    std::size_t sent = 0;
    while (sent < size) {
#ifdef _WIN32
        const int n = ::send(handle, data + sent, static_cast<int>(size - sent), 0);
#else
        const int n = ::send(handle, data + sent, size - sent, 0);
#endif
        if (n <= 0) {
            throw Error("socket send failed" + system_error_text());
        }
        sent += static_cast<std::size_t>(n);
    }
}

inline void recv_all_socket(socket_handle_t handle, char* data, std::size_t size) {
    std::size_t got = 0;
    while (got < size) {
#ifdef _WIN32
        const int n = ::recv(handle, data + got, static_cast<int>(size - got), 0);
#else
        const int n = ::recv(handle, data + got, size - got, 0);
#endif
        if (n <= 0) {
            throw Error("socket recv failed" + system_error_text());
        }
        got += static_cast<std::size_t>(n);
    }
}

inline void write_all_pipe(pipe_handle_t handle, const char* data, std::size_t size) {
    std::size_t sent = 0;
    while (sent < size) {
#ifdef _WIN32
        DWORD n = 0;
        if (!WriteFile(handle, data + sent, static_cast<DWORD>(size - sent), &n, nullptr) || n == 0) {
            throw Error("pipe write failed" + system_error_text());
        }
        sent += static_cast<std::size_t>(n);
#else
        const int n = ::write(handle, data + sent, size - sent);
        if (n <= 0) {
            throw Error("pipe write failed" + system_error_text());
        }
        sent += static_cast<std::size_t>(n);
#endif
    }
}

inline void read_all_pipe(pipe_handle_t handle, char* data, std::size_t size) {
    std::size_t got = 0;
    while (got < size) {
#ifdef _WIN32
        DWORD n = 0;
        if (!ReadFile(handle, data + got, static_cast<DWORD>(size - got), &n, nullptr) || n == 0) {
            throw Error("pipe read failed" + system_error_text());
        }
        got += static_cast<std::size_t>(n);
#else
        const int n = ::read(handle, data + got, size - got);
        if (n <= 0) {
            throw Error("pipe read failed" + system_error_text());
        }
        got += static_cast<std::size_t>(n);
#endif
    }
}

inline std::string sockaddr_to_host_port(const sockaddr_in& addr) {
    char ip[INET_ADDRSTRLEN] = {};
    if (!inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip))) {
        return {};
    }
    return std::string(ip) + ":" + std::to_string(ntohs(addr.sin_port));
}

} // namespace detail

class TcpSocket {
public:
    TcpSocket() = default;
    explicit TcpSocket(detail::socket_handle_t handle) : handle_(handle) {}
    ~TcpSocket() { close(); }

    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    TcpSocket(TcpSocket&& other) noexcept : handle_(other.release()) {}
    TcpSocket& operator=(TcpSocket&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = other.release();
        }
        return *this;
    }

    static TcpSocket connect(const std::string& endpoint, int timeout_seconds = 3) {
        detail::SocketSystem sockets;
        const auto target = detail::parse_endpoint(endpoint);
        if (timeout_seconds < 0) {
            throw Error("timeout_seconds must be non-negative");
        }

#ifdef _WIN32
        SOCKET handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (handle == INVALID_SOCKET) {
            throw Error("socket() failed" + detail::system_error_text());
        }
        u_long non_blocking = 1;
        ioctlsocket(handle, FIONBIO, &non_blocking);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<u_short>(target.port));
        inet_pton(AF_INET, target.host.c_str(), &addr.sin_addr);

        const int rc = ::connect(handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (rc != 0 && WSAGetLastError() == WSAEWOULDBLOCK) {
            fd_set write_set;
            FD_ZERO(&write_set);
            FD_SET(handle, &write_set);
            timeval tv{timeout_seconds, 0};
            const int sr = select(0, nullptr, &write_set, nullptr, &tv);
            if (sr <= 0) {
                closesocket(handle);
                throw Error("connect timeout: " + endpoint + detail::system_error_text());
            }
        } else if (rc != 0) {
            closesocket(handle);
            throw Error("connect failed: " + endpoint + detail::system_error_text());
        }

        non_blocking = 0;
        ioctlsocket(handle, FIONBIO, &non_blocking);
        return TcpSocket(handle);
#else
        int handle = socket(AF_INET, SOCK_STREAM, 0);
        if (handle < 0) {
            throw Error("socket() failed" + detail::system_error_text());
        }

        const int flags = fcntl(handle, F_GETFL, 0);
        fcntl(handle, F_SETFL, flags | O_NONBLOCK);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(target.port);
        inet_pton(AF_INET, target.host.c_str(), &addr.sin_addr);

        const int rc = ::connect(handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        if (rc < 0 && errno == EINPROGRESS) {
            fd_set write_set;
            FD_ZERO(&write_set);
            FD_SET(handle, &write_set);
            timeval tv{timeout_seconds, 0};
            const int sr = select(handle + 1, nullptr, &write_set, nullptr, &tv);
            if (sr <= 0) {
                ::close(handle);
                throw Error("connect timeout: " + endpoint + detail::system_error_text());
            }
            int err = 0;
            socklen_t err_len = sizeof(err);
            getsockopt(handle, SOL_SOCKET, SO_ERROR, &err, &err_len);
            if (err != 0) {
                ::close(handle);
                errno = err;
                throw Error("connect failed: " + endpoint + detail::system_error_text());
            }
        } else if (rc < 0) {
            ::close(handle);
            throw Error("connect failed: " + endpoint + detail::system_error_text());
        }

        fcntl(handle, F_SETFL, flags);
        return TcpSocket(handle);
#endif
    }

    bool is_open() const {
        return handle_ != detail::invalid_socket_handle();
    }

    void send_raw(std::string_view bytes) {
        ensure_open();
        detail::send_all_socket(handle_, bytes.data(), bytes.size());
    }

    std::string recv_raw_exact(std::size_t size) {
        ensure_open();
        std::string out(size, '\0');
        if (!out.empty()) {
            detail::recv_all_socket(handle_, out.data(), out.size());
        }
        return out;
    }

    void send_frame(std::string_view payload) {
        ensure_open();
        if (payload.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            throw Error("frame too large for 32-bit length prefix");
        }
        std::array<char, 4> header{};
        detail::write_u32_le(header.data(), static_cast<std::uint32_t>(payload.size()));
        detail::send_all_socket(handle_, header.data(), header.size());
        if (!payload.empty()) {
            detail::send_all_socket(handle_, payload.data(), payload.size());
        }
    }

    std::string recv_frame() {
        ensure_open();
        std::array<char, 4> header{};
        detail::recv_all_socket(handle_, header.data(), header.size());
        const auto size = detail::read_u32_le(header.data());
        std::string payload(size, '\0');
        if (size != 0) {
            detail::recv_all_socket(handle_, payload.data(), payload.size());
        }
        return payload;
    }

    void send_net(const Value& value) {
        send_frame(encode_payload(value));
    }

    Value recv_net() {
        return decode_payload(recv_frame());
    }

    std::string peer_addr() const {
        ensure_open();
        sockaddr_in addr{};
#ifdef _WIN32
        int len = sizeof(addr);
#else
        socklen_t len = sizeof(addr);
#endif
        if (getpeername(handle_, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
            return {};
        }
        return detail::sockaddr_to_host_port(addr);
    }

    void set_timeout_seconds(int seconds) {
        ensure_open();
        if (seconds < 0) {
            throw Error("tcp.set_timeout requires non-negative seconds");
        }
#ifdef _WIN32
        const DWORD timeout_ms = static_cast<DWORD>(seconds * 1000);
        setsockopt(handle_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
        setsockopt(handle_, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
#else
        timeval tv{seconds, 0};
        setsockopt(handle_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(handle_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
    }

    void close() {
        if (is_open()) {
            detail::close_socket(handle_);
            handle_ = detail::invalid_socket_handle();
        }
    }

    detail::socket_handle_t release() noexcept {
        const auto out = handle_;
        handle_ = detail::invalid_socket_handle();
        return out;
    }

private:
    void ensure_open() const {
        if (!is_open()) {
            throw Error("Operation on closed tcp socket");
        }
    }

    detail::socket_handle_t handle_ = detail::invalid_socket_handle();
};

class TcpServer {
public:
    TcpServer() = default;
    explicit TcpServer(detail::socket_handle_t handle) : handle_(handle) {}
    ~TcpServer() { close(); }

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    TcpServer(TcpServer&& other) noexcept : handle_(other.release()) {}
    TcpServer& operator=(TcpServer&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = other.release();
        }
        return *this;
    }

    static TcpServer listen(const std::string& endpoint, int backlog = 16) {
        detail::SocketSystem sockets;
        const auto target = detail::parse_endpoint(endpoint);
        if (backlog < 0) {
            throw Error("tcp_listen backlog must be non-negative");
        }

#ifdef _WIN32
        SOCKET handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (handle == INVALID_SOCKET) {
            throw Error("socket() failed" + detail::system_error_text());
        }
        int yes = 1;
        setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
#else
        int handle = socket(AF_INET, SOCK_STREAM, 0);
        if (handle < 0) {
            throw Error("socket() failed" + detail::system_error_text());
        }
        int yes = 1;
        setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<std::uint16_t>(target.port));
        if (target.host.empty() || target.host == "0.0.0.0") {
            addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            inet_pton(AF_INET, target.host.c_str(), &addr.sin_addr);
        }

        if (::bind(handle, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
            detail::close_socket(handle);
            throw Error("bind failed: " + endpoint + detail::system_error_text());
        }
        if (::listen(handle, backlog) != 0) {
            detail::close_socket(handle);
            throw Error("listen failed: " + endpoint + detail::system_error_text());
        }
        return TcpServer(handle);
    }

    TcpSocket accept() const {
        ensure_open();
        sockaddr_in addr{};
#ifdef _WIN32
        int len = sizeof(addr);
        SOCKET client = ::accept(handle_, reinterpret_cast<sockaddr*>(&addr), &len);
        if (client == INVALID_SOCKET) {
#else
        socklen_t len = sizeof(addr);
        int client = ::accept(handle_, reinterpret_cast<sockaddr*>(&addr), &len);
        if (client < 0) {
#endif
            throw Error("accept failed" + detail::system_error_text());
        }
        return TcpSocket(client);
    }

    std::string addr() const {
        ensure_open();
        sockaddr_in addr{};
#ifdef _WIN32
        int len = sizeof(addr);
#else
        socklen_t len = sizeof(addr);
#endif
        if (getsockname(handle_, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
            return {};
        }
        return detail::sockaddr_to_host_port(addr);
    }

    bool is_open() const {
        return handle_ != detail::invalid_socket_handle();
    }

    void close() {
        if (is_open()) {
            detail::close_socket(handle_);
            handle_ = detail::invalid_socket_handle();
        }
    }

    detail::socket_handle_t release() noexcept {
        const auto out = handle_;
        handle_ = detail::invalid_socket_handle();
        return out;
    }

private:
    void ensure_open() const {
        if (!is_open()) {
            throw Error("Operation on closed tcp server");
        }
    }

    detail::socket_handle_t handle_ = detail::invalid_socket_handle();
};

class PipeChannel {
public:
    PipeChannel() = default;
    PipeChannel(detail::pipe_handle_t read_handle, detail::pipe_handle_t write_handle, std::string name, bool named)
        : read_handle_(read_handle), write_handle_(write_handle), name_(std::move(name)), named_(named) {}
    ~PipeChannel() { close(); }

    PipeChannel(const PipeChannel&) = delete;
    PipeChannel& operator=(const PipeChannel&) = delete;

    PipeChannel(PipeChannel&& other) noexcept
        : read_handle_(other.read_handle_),
          write_handle_(other.write_handle_),
          name_(std::move(other.name_)),
          named_(other.named_) {
        other.read_handle_ = invalid_pipe();
        other.write_handle_ = invalid_pipe();
        other.named_ = false;
    }

    PipeChannel& operator=(PipeChannel&& other) noexcept {
        if (this != &other) {
            close();
            read_handle_ = other.read_handle_;
            write_handle_ = other.write_handle_;
            name_ = std::move(other.name_);
            named_ = other.named_;
            other.read_handle_ = invalid_pipe();
            other.write_handle_ = invalid_pipe();
            other.named_ = false;
        }
        return *this;
    }

    static PipeChannel anonymous() {
#ifdef _WIN32
        HANDLE read_handle = nullptr;
        HANDLE write_handle = nullptr;
        SECURITY_ATTRIBUTES attrs{};
        attrs.nLength = sizeof(attrs);
        attrs.bInheritHandle = TRUE;
        attrs.lpSecurityDescriptor = nullptr;
        if (!CreatePipe(&read_handle, &write_handle, &attrs, 0)) {
            throw Error("CreatePipe failed" + detail::system_error_text());
        }
        return PipeChannel(read_handle, write_handle, "", false);
#else
        int fds[2] = {-1, -1};
        if (::pipe(fds) != 0) {
            throw Error("pipe() failed" + detail::system_error_text());
        }
        return PipeChannel(fds[0], fds[1], "", false);
#endif
    }

    static PipeChannel open_named(const std::string& name) {
#ifdef _WIN32
        const std::string pipe_name = "\\\\.\\pipe\\" + name;
        HANDLE handle = CreateNamedPipeA(
            pipe_name.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,
            4096,
            4096,
            0,
            nullptr
        );
        if (handle == INVALID_HANDLE_VALUE) {
            handle = CreateFileA(
                pipe_name.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr
            );
            if (handle == INVALID_HANDLE_VALUE) {
                throw Error("Cannot open pipe: " + name + detail::system_error_text());
            }
        } else {
            const BOOL ok = ConnectNamedPipe(handle, nullptr);
            if (!ok && GetLastError() != ERROR_PIPE_CONNECTED) {
                CloseHandle(handle);
                throw Error("ConnectNamedPipe failed" + detail::system_error_text());
            }
        }
        return PipeChannel(handle, handle, name, true);
#else
        const std::string pipe_name = "/tmp/sl_pipe_" + name;
        ::mkfifo(pipe_name.c_str(), 0666);
        const int handle = ::open(pipe_name.c_str(), O_RDWR);
        if (handle < 0) {
            throw Error("Cannot open pipe: " + name + detail::system_error_text());
        }
        return PipeChannel(handle, handle, name, true);
#endif
    }

    bool is_open() const {
        return !detail::is_invalid_pipe_handle(read_handle_) && !detail::is_invalid_pipe_handle(write_handle_);
    }

    bool is_named() const {
        return named_;
    }

    const std::string& name() const {
        return name_;
    }

    void send_frame(std::string_view payload) {
        ensure_open();
        if (payload.size() > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
            throw Error("frame too large for 32-bit length prefix");
        }
        std::array<char, 4> header{};
        detail::write_u32_le(header.data(), static_cast<std::uint32_t>(payload.size()));
        detail::write_all_pipe(write_handle_, header.data(), header.size());
        if (!payload.empty()) {
            detail::write_all_pipe(write_handle_, payload.data(), payload.size());
        }
    }

    std::string recv_frame() {
        ensure_open();
        std::array<char, 4> header{};
        detail::read_all_pipe(read_handle_, header.data(), header.size());
        const auto size = detail::read_u32_le(header.data());
        std::string payload(size, '\0');
        if (size != 0) {
            detail::read_all_pipe(read_handle_, payload.data(), payload.size());
        }
        return payload;
    }

    void send_net(const Value& value) {
        send_frame(encode_payload(value));
    }

    Value recv_net() {
        return decode_payload(recv_frame());
    }

    void flush() {
        ensure_open();
#ifdef _WIN32
        FlushFileBuffers(write_handle_);
#else
        fsync(write_handle_);
#endif
    }

    void close() {
        if (detail::is_invalid_pipe_handle(read_handle_) && detail::is_invalid_pipe_handle(write_handle_)) {
            return;
        }
        detail::close_pipe(read_handle_);
        if (write_handle_ != read_handle_) {
            detail::close_pipe(write_handle_);
        }
#ifndef _WIN32
        if (named_) {
            ::unlink(("/tmp/sl_pipe_" + name_).c_str());
        }
#endif
        read_handle_ = invalid_pipe();
        write_handle_ = invalid_pipe();
        name_.clear();
        named_ = false;
    }

private:
    static detail::pipe_handle_t invalid_pipe() {
#ifdef _WIN32
        return nullptr;
#else
        return -1;
#endif
    }

    void ensure_open() const {
        if (!is_open()) {
            throw Error("Operation on closed pipe");
        }
    }

    detail::pipe_handle_t read_handle_ = invalid_pipe();
    detail::pipe_handle_t write_handle_ = invalid_pipe();
    std::string name_;
    bool named_ = false;
};

} // namespace ssilang::net
