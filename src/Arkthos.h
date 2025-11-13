#pragma once
#include <fcntl.h>
#include <netinet/in.h>
#include "Temp.h"

struct Config {
    static constexpr unsigned int LogSize = 1024;
    static constexpr unsigned int MaxMessageSize = 1400;
    static constexpr unsigned int ConsensusThreads = 1;
    static constexpr unsigned int Pipes = 1;
    static constexpr unsigned int ListenerThreads = 1;
    static constexpr unsigned char NodeId = 0;
    static constexpr Address ListenerAddress = Address(127, 0, 0, 1, 6969);

    static constexpr unsigned char ListenerA = 127;
    static constexpr unsigned char ListenerB = 0;
    static constexpr unsigned char ListenerC = 0;
    static constexpr unsigned char ListenerD = 1;

    // Base port for client listener addresses
    static constexpr unsigned short ListenerPort = 6969;

    // Multicast base IP: 239.10.0.100
    static constexpr unsigned char McastA = 239;
    static constexpr unsigned char McastB = 10;
    static constexpr unsigned char McastC = 0;
    static constexpr unsigned char McastDBase = 100;
    // Base port for multicast groups
    static constexpr unsigned short McastPortBase = 9000;
};

template<typename T>
concept ArkthosConfig =
        requires
        {
            { T::LogSize } -> std::convertible_to<unsigned int>;
            { T::MaxMessageSize } -> std::convertible_to<unsigned int>;
            { T::ConsensusThreads } -> std::convertible_to<unsigned int>;
            { T::Pipes } -> std::convertible_to<unsigned int>;
            { T::ListenerThreads } -> std::convertible_to<unsigned int>;
            { T::NodeId } -> std::convertible_to<unsigned char>;
            { T::ListenerPort } -> std::convertible_to<unsigned short>;
            { T::ListenerHost } -> std::convertible_to<const char*>;

            { T::McastA }            -> std::convertible_to<unsigned char>;
            { T::McastB }            -> std::convertible_to<unsigned char>;
            { T::McastC }            -> std::convertible_to<unsigned char>;
            { T::McastDBase }        -> std::convertible_to<unsigned char>;
            { T::McastPortBase }     -> std::convertible_to<unsigned short>;
        };

inline void run_arkthos_listener(
    const unsigned int thread_id,
    const std::array<Address, Config::ListenerThreads>& listener_groups
) {
    const int listener_socket = setup_server_socket(listener_groups[thread_id], 1 << 30);

    sockaddr_in src_addr{};
    socklen_t src_len = sizeof(src_addr);

    using RecvBuffer = std::array<char, Config::MaxMessageSize>;
    std::unique_ptr<RecvBuffer> recv_buffer = std::make_unique<RecvBuffer>();
    iovec iov{
        .iov_base = recv_buffer->data(),
        .iov_len = recv_buffer->size()
    };

    std::array<char, 256> control_buf{};
    msghdr msg{
        .msg_name = &src_addr,
        .msg_namelen = src_len,
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = control_buf.data(),
        .msg_controllen = control_buf.size(),
        .msg_flags = 0
    };

    while (RUNNING.load(std::memory_order_relaxed)) {
        const ssize_t bytes_read = recvmsg(listener_socket, &msg, 0);

        if (bytes_read <= 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            throw std::runtime_error("Failed to receive message");
        }
    }
}



inline void run_arkthos_consensus(
    const unsigned int thread_id,
    const std::array<Address, Config::ConsensusThreads>& multicast_groups
) {
    std::array<int, Config::ConsensusThreads> sockets {};
    for (unsigned int socket_id = 0; socket_id < Config::ConsensusThreads; ++socket_id) {
        if (socket_id == thread_id) {
            // setup multicast server socket
        } else {
            // setup socket connected to multicast group
        }
    }

    using RecvBuffer = std::array<char, Config::MaxMessageSize>;
    std::unique_ptr<RecvBuffer> recv_buffer = std::make_unique<RecvBuffer>();

    const auto server_socket = sockets[thread_id];
    while (RUNNING.load(std::memory_order_relaxed)) {
        const auto bytes_read = recv(server_socket, recv_buffer->data(), recv_buffer->size(), 0);

        if (bytes_read <= 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
            throw std::runtime_error("Failed to receive message");
        }
    }
}

template<ArkthosConfig Config>
void run_arkthos_udp() {
    unsigned int worker_index = 0;
    constexpr unsigned int TotalWorkers = Config::ListenerThreads + Config::ConsensusThreads;
    std::array<std::thread, TotalWorkers> workers;

    constexpr std::array<Address, Config::ConsensusThreads> multicast_groups = [] consteval {
        std::array<Address, Config::ConsensusThreads> arr{};
        for (unsigned int i = 0; i < Config::ConsensusThreads; ++i)
            arr[i] = Address(
                Config::McastA,
                Config::McastB,
                Config::McastC,
                Config::McastDBase + i,
                Config::McastPortBase + i
            );
        return arr;
    }();

    constexpr auto listener_groups = []() consteval {
        std::array<Address, Config::ListenerThreads> arr{};
        for (unsigned int i = 0; i < Config::ListenerThreads; ++i) {
            arr[i] = Address(
                Config::ListenerA,
                Config::ListenerB,
                Config::ListenerC,
                Config::ListenerD,
                static_cast<unsigned short>(Config::ListenerPort + i)
            );
        }
        return arr;
    }();

    using LogBuffer = std::array<char, Config::LogSize * Config::MaxMessageSize>;
    std::unique_ptr<LogBuffer> log = std::make_unique<LogBuffer>();

    for (unsigned int i = 0; i < TotalWorkers; ++i) {
        workers[worker_index++] = i < Config::ListenerThreads
            ? thread_guard(
                  "Listener Thread " + std::to_string(i),
                  [i, &listener_groups] { run_arkthos_listener(i, listener_groups); })
            : thread_guard(
                  "Consensus Thread " + std::to_string(i - Config::ListenerThreads),
                  [thread_id = i - Config::ListenerThreads, &multicast_groups] {
                      run_arkthos_consensus(thread_id, multicast_groups);
                  });
    }


    for (auto &worker: workers) {
        worker.join();
    }
}
