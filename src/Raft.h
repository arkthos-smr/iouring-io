#pragma once
#include <string>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <vector>
#include <thread>
#include "Ring.hpp"
#include "sys/socket.h"
#include <netinet/tcp.h>
#include <fcntl.h>

inline void tune_udp_socket(const int fd) {
    if (fd < 0) {
        throw std::invalid_argument("Invalid socket descriptor");
    }

    int bufsize = 1 << 20; // 1 MB

    // Increase send buffer
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize)) < 0) {
        close(fd);
        throw std::runtime_error("setsockopt(SO_SNDBUF) failed: " + std::string(std::strerror(errno)));
    }

    // Increase receive buffer
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize)) < 0) {
        close(fd);
        throw std::runtime_error("setsockopt(SO_RCVBUF) failed: " + std::string(std::strerror(errno)));
    }

    // Mark as low latency (IP_TOS = IPTOS_LOWDELAY)
    constexpr int tos = 0x10;
    if (setsockopt(fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0) {
        close(fd);
        throw std::runtime_error("setsockopt(IP_TOS) failed: " + std::string(std::strerror(errno)));
    }

    // Make socket non-blocking
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

struct Address {
    std::string host;
    unsigned short port;
    sockaddr_in addr{};

    Address(const std::string& host, const unsigned short port) : host(host), port(port) {
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
    }
};

template<size_t log_size, size_t message_size, size_t threads>
void run_raft_udp(
    const unsigned int connections,
    const unsigned int pipes,
    const unsigned char node_id,
    const unsigned char leader_id,
    std::vector<Address> &peers
) {
    std::vector<std::thread> local_workers;
    local_workers.reserve(threads);

    auto acks = new std::atomic<unsigned char>[log_size];
    char** log = new char*[log_size];
    for (size_t i = 0; i < log_size; ++i) {
        log[i] = new char[message_size];
    }


    const auto pipes_per_thread = pipes / threads;
    for (int threadId = 0; threadId < threads; threadId++) {
        local_workers.emplace_back([&peers, connections, pipes_per_thread, node_id, leader_id]() {
            int peer_sockets[peers.size()+1];
            auto node_address = peers[node_id];
            const auto server_socket = socket(AF_INET, SOCK_DGRAM, 0);
            if (server_socket < 0) {
                throw std::runtime_error("Failed to create server socket");
            }

            constexpr int flag = 1;
            if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
                close(server_socket);
                throw std::runtime_error("error setting reuseaddr opt");
            }

            if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(flag)) < 0) {
                close(server_socket);
                throw std::runtime_error("Failed to set SO_REUSEPORT");
            }

            if (bind(server_socket, reinterpret_cast<sockaddr*>(&node_address.addr), sizeof(node_address.addr)) < 0) {
                close(server_socket);
                throw std::runtime_error("bind failed");
            }

            tune_udp_socket(server_socket);

            peer_sockets[0] = server_socket;
            if (leader_id == node_id) {
                for (int i = 0; i < peers.size(); ++i) {
                    if (i != node_id) {
                        auto peer = peers[i];
                        const auto client_socket = socket(AF_INET, SOCK_DGRAM, 0);
                        tune_udp_socket(client_socket);
                        if (connect(client_socket, reinterpret_cast<sockaddr*>(&peer.addr), sizeof(peer.addr)) != 0) {
                            throw std::runtime_error("Failed to connect to peer");
                        }
                        peer_sockets[i+1] = client_socket;
                    } else peer_sockets[i+1] = -1;
                }
            }

            constexpr unsigned int buffer_count = 128;
            std::vector<iovec> io_vecs(buffer_count);
            const auto write_buffers = new char[buffer_count * message_size];
            const auto write_references = new unsigned int[buffer_count];
            const auto write_buffer_stack = new unsigned int[buffer_count];

            for (size_t i = 0; i < buffer_count; ++i) {
                io_vecs[i].iov_base = write_buffers + i * message_size;
                io_vecs[i].iov_len = message_size;
                write_references[i] = 0;
                write_buffer_stack[i] = static_cast<unsigned int>(i);
            }

            fprintf(stderr, "Node id: %d\n", node_id);
            run_ring(
                4096, 1,
                ring_init(
                    io_uring_register(_r_ring_fd, IORING_REGISTER_FILES, peer_sockets, peers.size()+1);
                    io_uring_register(_r_ring_fd, IORING_REGISTER_BUFFERS, io_vecs.data(), buffer_count);
                ),
                ring_loop(),
                ring_completions(

                )
            );

            for (int i = 0; i < peers.size(); ++i) { if (i != node_id) close(peer_sockets[i]); }
            close(server_socket);
            delete[] write_buffers;
            delete[] write_references;
            delete[] write_buffer_stack;
        });
    }


    for (auto &worker: local_workers) { worker.join(); }
    for (size_t i = 0; i < log_size; ++i) { delete[] log[i]; }
    delete[] log;
    delete[] acks;
}


