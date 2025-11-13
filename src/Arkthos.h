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
    static constexpr const char* ListenerHost = "127.0.0.1";
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
    const unsigned int thread_id
) {
    const Address thread_address = Address(std::string(Config::ListenerHost), Config::ListenerPort + thread_id);
    const int listener_socket = setup_server_socket(thread_address, 1 << 30);

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
    std::array<std::thread, Config::ListenerThreads + Config::ConsensusThreads> workers;

    std::array<Address, Config::ConsensusThreads> multicast_groups;
    for (unsigned int i = 0; i < Config::ConsensusThreads; i++) {
        const std::string ip = std::format(
            "{}.{}.{}.{}",
            Config::McastA,
            Config::McastB,
            Config::McastC,
            Config::McastDBase + i
        );
        multicast_groups[i] = Address(ip, static_cast<unsigned short>(Config::McastPortBase + i));
    }

    using LogBuffer = std::array<char, Config::LogSize * Config::MaxMessageSize>;
    std::unique_ptr<LogBuffer> log = std::make_unique<LogBuffer>();

    for (unsigned int thread_id = 0; thread_id < Config::ListenerThreads; ++thread_id) {
        workers[worker_index++] = thread_guard("Listener Thread " + std::to_string(thread_id), [thread_id] {
            run_arkthos_listener(thread_id);
        });
    }

    for (unsigned int thread_id = 0; thread_id < Config::ConsensusThreads; ++thread_id) {
        workers[worker_index++] = thread_guard("Consensus Thread " + std::to_string(thread_id), [thread_id, &multicast_groups] {
            run_arkthos_consensus(thread_id, multicast_groups);
        });
    }

    for (auto &worker: workers) {
        worker.join();
    }
}
