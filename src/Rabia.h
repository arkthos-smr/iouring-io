#pragma once
#include <fcntl.h>
#include <netinet/in.h>
#include "Temp.h"

struct Config {
    static constexpr size_t LogSize = 1024;
    static constexpr size_t MaxMessageSize = 1400;
    static constexpr size_t ConsensusThreads = 1;
    static constexpr size_t Pipes = 1;
    static constexpr size_t ListenerThreads = 1;
};

template<typename T>
concept RabiaConfig =
    requires {
    { T::LogSize } -> std::convertible_to<size_t>;
    { T::MaxMessageSize } -> std::convertible_to<size_t>;
    { T::Threads } -> std::convertible_to<size_t>;
    { T::Pipes } -> std::convertible_to<size_t>;
    { T::ListenerThreads } -> std::convertible_to<size_t>;
};

class RabiaPeer {
    std::string_view host;
    unsigned short port;
public:
    constexpr RabiaPeer(const std::string_view host, const unsigned short port) : host(host), port(port) {}
    [[nodiscard]] constexpr std::string_view get_host() const noexcept { return host; }
    [[nodiscard]] constexpr uint16_t get_start_port() const noexcept { return port; }
};

template<RabiaConfig Config>
void run_rabia_udp(
    const unsigned char node_id,
    const Address& client_listen_address,
    std::vector<RabiaPeer> &peers
) {
    constexpr size_t log_size   = Config::LogSize;
    constexpr size_t max_msg_size   = Config::MaxMessageSize;
    constexpr size_t consensus_threads    = Config::ConsensusThreads;
    constexpr size_t pipes      = Config::Pipes;
    constexpr size_t listener_threads  = Config::ListenerThreads;

    std::vector<std::thread> workers;
    workers.reserve(listener_threads + consensus_threads);

    using LogBuffer = std::array<char, log_size * max_msg_size>;
    std::unique_ptr<LogBuffer> log = std::make_unique<LogBuffer>();

    for (unsigned int thread_id = 0; thread_id < listener_threads; ++thread_id) {
        workers.emplace_back([&client_listen_address, node_id, thread_id]() {

        });
    }

    for (unsigned int thread_id = 0; thread_id < consensus_threads; ++thread_id) {
        workers.emplace_back([log_ptr = log.get(), node_id, thread_id, pipes, &peers]() {
            const RabiaPeer& peer = peers[node_id];
            const auto thread_address = Address(std::string(peer.get_host()), peer.get_start_port() + thread_id);
            auto sockets = std::vector<int>(peers.size());
            for (unsigned int i = 0; i < peers.size(); ++i) {
                sockets[i] = (i == node_id ? setup_server_socket(thread_address, 1 << 30) : setup_socket(1 << 30));
            }

            using RecvBuffer = std::array<char, max_msg_size>;
            std::unique_ptr<RecvBuffer> recv_buffer = std::make_unique<std::array<char, max_msg_size>>();

            sockaddr_in src_addr{};
            constexpr socklen_t src_len = sizeof(src_addr);

            std::array<char, 256> control_buf{};
            iovec iov{
                .iov_base = recv_buffer->data(),
                .iov_len = recv_buffer->size()
            };

            msghdr msg {
                .msg_name = &src_addr,
                .msg_namelen = src_len,
                .msg_iov = &iov,
                .msg_iovlen = 1,
                .msg_control = control_buf.data(),
                .msg_controllen = control_buf.size(),
                .msg_flags = 0
            };

            const auto server_socket = sockets[node_id];
            while (RUNNING.load(std::memory_order_relaxed)) {
                const auto bytes_read = recvmsg(server_socket, &msg, 0);

                if (bytes_read <= 0) {
                    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue;
                    throw std::runtime_error("Failed to receive message");
                }
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }
}
