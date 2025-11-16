#pragma once
#include <fcntl.h>
#include <netinet/in.h>
#include "Temp.h"

struct ConfigTcp {
    static constexpr unsigned int LogSize = 1024;
    static constexpr unsigned int MaxMessageSize = 1400;
    static constexpr unsigned int ConsensusThreads = 2;
    static constexpr unsigned int Pipes = 1;
    static constexpr unsigned int ListenerThreads = 1;
    static constexpr unsigned char NodeId = 1;

    static constexpr unsigned char NodeIPs[][4] = {
        {127, 0, 0, 1},     // node 0
        {127, 0, 0, 1}     // node 1
    };
    static constexpr unsigned short BasePort = 9000;

    static constexpr unsigned int NumNodes = std::size(NodeIPs);

    static constexpr unsigned char ListenerA = 127;
    static constexpr unsigned char ListenerB = 0;
    static constexpr unsigned char ListenerC = 0;
    static constexpr unsigned char ListenerD = 1;

    // Base port for client listener addresses
    static constexpr unsigned short ListenerPort = 6969;

    static constexpr unsigned int RecvSendBufferSize = 1 << 30;
};

template<typename T>
concept ArkthosConfigTcp = requires {

    // Log + message sizing
    { T::LogSize }            -> std::convertible_to<unsigned int>;
    { T::MaxMessageSize }     -> std::convertible_to<unsigned int>;

    // Thread topology
    { T::ConsensusThreads }   -> std::convertible_to<unsigned int>;
    { T::Pipes }              -> std::convertible_to<unsigned int>;
    { T::ListenerThreads }    -> std::convertible_to<unsigned int>;
    { T::RecvSendBufferSize } -> std::convertible_to<unsigned int>;

    // Node identity
    { T::NodeId }             -> std::convertible_to<unsigned char>;

    // Listener IP (4 octets)
    { T::ListenerA }          -> std::convertible_to<unsigned char>;
    { T::ListenerB }          -> std::convertible_to<unsigned char>;
    { T::ListenerC }          -> std::convertible_to<unsigned char>;
    { T::ListenerD }          -> std::convertible_to<unsigned char>;
    { T::ListenerPort }       -> std::convertible_to<unsigned short>;


    // requires std::is_array_v<decltype(T::NodeIPs)>;  // outer array
    // requires std::is_array_v<std::remove_extent_t<decltype(T::NodeIPs)>>; // inner array
    // requires std::same_as<std::remove_all_extents_t<decltype(T::NodeIPs)>, unsigned char>;
    { T::NumNodes }           -> std::convertible_to<unsigned int>;
    { T::BasePort }           -> std::convertible_to<unsigned short>;
};


inline void run_arkthos_tcp_listener(
    const unsigned int thread_id,
    const std::array<Address, ConfigTcp::ListenerThreads>& listener_groups
) {
    // const int listener_socket = setup_server_socket(listener_groups[thread_id], Config::RecvSendBufferSize);

    // sockaddr_in src_addr{};
    // socklen_t src_len = sizeof(src_addr);
    //
    // using RecvBuffer = std::array<char, Config::MaxMessageSize>;
    // const std::unique_ptr<RecvBuffer> recv_buffer = std::make_unique<RecvBuffer>();
    // iovec iov{
    //     .iov_base = recv_buffer->data(),
    //     .iov_len = recv_buffer->size()
    // };
    //
    // std::array<char, 256> control_buf{};
    // msghdr msg{
    //     .msg_name = &src_addr,
    //     .msg_namelen = src_len,
    //     .msg_iov = &iov,
    //     .msg_iovlen = 1,
    //     .msg_control = control_buf.data(),
    //     .msg_controllen = control_buf.size(),
    //     .msg_flags = 0
    // };
    //
    // while (RUNNING.load(std::memory_order_relaxed)) {
    //     const ssize_t bytes_read = recvmsg(listener_socket, &msg, 0);
    //     if (bytes_read <= 0) {
    //         if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
    //         throw std::runtime_error("Failed to receive message");
    //     }
    // }
}

inline void run_arkthos_tcp_consensus(
    const unsigned int thread_id
) {
    // const auto server_socket = setup_multicast_server_socket(multicast_groups[thread_id], Config::RecvSendBufferSize);
    // std::array<int, Config::ConsensusThreads> sockets{};
    // for (unsigned int socket_id = 0; socket_id < Config::ConsensusThreads; ++socket_id) {
    //     sockets[socket_id] = setup_multicast_sender_socket(multicast_groups[socket_id], Config::RecvSendBufferSize, Config::AllowLoopback);
    // }
    //
    // using RecvBuffer = std::array<char, Config::MaxMessageSize>;
    // const std::unique_ptr<RecvBuffer> recv_buffer = std::make_unique<RecvBuffer>();
    //
    // while (RUNNING.load(std::memory_order_relaxed)) {
    //     const ssize_t bytes_read = recv(server_socket, recv_buffer->data(), recv_buffer->size(), 0);
    //     if (bytes_read <= 0) {
    //         if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
    //         throw std::runtime_error("Failed to receive message");
    //     }
    //     fprintf(stderr, "Received %ld bytes on thread %d\n", bytes_read, thread_id);
    // }
}

template<ArkthosConfigTcp Config>
void run_arkthos_tcp() {
    unsigned int worker_index = 0;
    constexpr unsigned int TotalWorkers = Config::ListenerThreads + Config::ConsensusThreads;
    std::array<std::thread, TotalWorkers> workers;
    //
    // constexpr std::array<Address, Config::ConsensusThreads> multicast_groups = [] consteval {
    //     std::array<Address, Config::ConsensusThreads> array;
    //     for (unsigned int i = 0; i < Config::ConsensusThreads; ++i)
    //         array[i] = Address(
    //             Config::McastA,
    //             Config::McastB,
    //             Config::McastC,
    //             Config::McastDBase + i,
    //             Config::McastPortBase + i
    //         );
    //     return array;
    // }();

    constexpr auto listener_groups = []() consteval {
        std::array<Address, Config::ListenerThreads> array{};
        for (unsigned int i = 0; i < Config::ListenerThreads; ++i) {
            array[i] = Address(
                Config::ListenerA,
                Config::ListenerB,
                Config::ListenerC,
                Config::ListenerD,
                Config::ListenerPort + i
            );
        }
        return array;
    }();

    using LogBuffer = std::array<char, Config::LogSize * Config::MaxMessageSize>;
    std::unique_ptr<LogBuffer> log = std::make_unique<LogBuffer>();

    for (unsigned int i = 0; i < TotalWorkers; ++i) {
        workers[worker_index++] = i < Config::ListenerThreads
            ? thread_guard(
                  "Listener Thread " + std::to_string(i),
                  [i, &listener_groups] {
                      run_arkthos_tcp_listener(i, listener_groups);
                  })
            : thread_guard(
                  "Consensus Thread " + std::to_string(i - Config::ListenerThreads),
                  [thread_id = i - Config::ListenerThreads] {
                      run_arkthos_tcp_consensus(thread_id);
                  });
    }

    while (RUNNING.load()) pause();
    for (std::thread &worker: workers) worker.join();
}
