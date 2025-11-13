#pragma once
#include <arpa/inet.h>

#pragma once

#include <string_view>
#include <netinet/in.h>

inline std::atomic RUNNING{true};

template<typename Fn>
std::thread thread_guard(std::string_view label, Fn&& fn) {
    return std::thread([label, fn = std::forward<Fn>(fn)]() mutable {
        try {
            fn();
        } catch (const std::exception& e) {
            std::cerr << "[" << label << "] fatal exception: " << e.what() << std::endl;
            RUNNING.store(false);
        } catch (...) {
            std::cerr << "[" << label << "] fatal unknown exception" << std::endl;
            RUNNING.store(false);
        }
    });
}

class Address {
    std::string host;
    uint16_t port;
    sockaddr_in addr{};

public:
    Address() = default;

    Address(const std::string &host, const unsigned short port) : host(host), port(port) {
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
    }

    [[nodiscard]] std::string_view get_host() const noexcept { return host; }
    [[nodiscard]] uint16_t get_port() const noexcept { return port; }
    [[nodiscard]] const sockaddr_in &get_sockaddr() const noexcept { return addr; }
    [[nodiscard]] sockaddr_in &get_sockaddr_mut() noexcept { return addr; }
};


inline void tune_udp_socket(const int fd, const unsigned int buf_size) {
    if (fd < 0) {
        throw std::invalid_argument("Invalid socket descriptor");
    }

    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buf_size, sizeof(buf_size)) < 0) {
        close(fd);
        throw std::runtime_error("setsockopt(SO_SNDBUF) failed: " + std::string(std::strerror(errno)));
    }

    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buf_size, sizeof(buf_size)) < 0) {
        close(fd);
        throw std::runtime_error("setsockopt(SO_RCVBUF) failed: " + std::string(std::strerror(errno)));
    }

    constexpr int tos = 0x10;
    if (setsockopt(fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0) {
        close(fd);
        throw std::runtime_error("setsockopt(IP_TOS) failed: " + std::string(std::strerror(errno)));
    }

    const int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        close(fd);
        throw std::runtime_error("fcntl(F_GETFL) failed: " + std::string(std::strerror(errno)));
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        close(fd);
        throw std::runtime_error("fcntl(F_SETFL, O_NONBLOCK) failed: " + std::string(std::strerror(errno)));
    }
}

inline int setup_socket(const unsigned int buf_size) {
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) throw std::runtime_error("Failed to create UDP socket");
    tune_udp_socket(fd, buf_size);
    return fd;
}

inline int setup_server_socket(const Address &address, const unsigned int buf_size) {
    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) throw std::runtime_error("Failed to create UDP socket");

    constexpr int flag = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
        close(fd);
        throw std::runtime_error("Failed to set SO_REUSEADDR");
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(flag)) < 0) {
        close(fd);
        throw std::runtime_error("Failed to set SO_REUSEPORT");
    }

    const sockaddr_in &addr = address.get_sockaddr();
    if (bind(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) < 0) {
        close(fd);
        throw std::runtime_error("Failed to bind server socket");
    }

    tune_udp_socket(fd, buf_size);

    return fd;
}
