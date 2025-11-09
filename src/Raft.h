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
#include <iostream>
#include <future>

#include "Queue.h"

inline void tune_udp_socket(const int fd) {
    if (fd < 0) {
        throw std::invalid_argument("Invalid socket descriptor");
    }

    int bufsize = 1 << 30; // 1 GB

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
    //
    // // Mark as low latency (IP_TOS = IPTOS_LOWDELAY)
    // constexpr int tos = 0x10;
    // if (setsockopt(fd, IPPROTO_IP, IP_TOS, &tos, sizeof(tos)) < 0) {
    //     close(fd);
    //     throw std::runtime_error("setsockopt(IP_TOS) failed: " + std::string(std::strerror(errno)));
    // }

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

    Address(const std::string &host, const unsigned short port) : host(host), port(port) {
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
        }
    }
};

constexpr unsigned char OP_PROPOSE = 0;
constexpr unsigned char OP_ACK = 1;

constexpr unsigned char MSG_NULL = 0;
constexpr unsigned char MSG_ACK = 1;


constexpr unsigned int APPLIED_MASK = 1u << 31;
constexpr int MAX_NODES = 31;
// constexpr std::array<unsigned int, MAX_NODES> NODE_MASKS = []() {
//     std::array<unsigned int, MAX_NODES> masks = {};
//     for (int i = 0; i < MAX_NODES; ++i) {
//         masks[i] = 1u << (30 - i);
//     }
//     return masks;
// }();


inline void run_raft_udp_client(
    const unsigned int num_clients,
    const unsigned int num_threads,
    const unsigned int data_size,
    const unsigned int num_ops,
    const float read_ratio,
    Address leader_address
) {
    std::vector<std::thread> local_workers;
    local_workers.reserve(num_threads);

    const unsigned int cli_per_thread = num_clients / num_threads;
    const unsigned int total_clients = cli_per_thread * num_threads;
    std::atomic ready_clients{ total_clients };
    std::atomic completed_clients{ total_clients };

    const unsigned int ops_per_client = num_ops / num_threads;
    const unsigned int buffer_size = data_size + 100;

    for (int i = 0; i < num_threads; i++) {
        local_workers.emplace_back([cli_per_thread, &leader_address, ops_per_client, data_size,  buffer_size, &ready_clients, &completed_clients]() {
            int client_sockets[cli_per_thread];
            for (int j = 0; j < cli_per_thread; ++j) {
                const auto client_socket = socket(AF_INET, SOCK_DGRAM, 0);
                tune_udp_socket(client_socket);
                if (connect(client_socket, reinterpret_cast<sockaddr *>(&leader_address.addr), sizeof(leader_address)) != 0) {
                    throw std::runtime_error("Failed to connect to peer");
                }
                client_sockets[j] = client_socket;
            }

            constexpr unsigned int buffer_count = 10000;
            std::vector<iovec> io_vecs(buffer_count);
            const auto write_buffers = new char[buffer_count * data_size];
            const auto write_references = new unsigned int[buffer_count];
            const auto write_buffer_stack = new unsigned int[buffer_count];
            auto write_buffer_stack_head = buffer_count;
            for (size_t i = 0; i < buffer_count; ++i) {
                io_vecs[i].iov_base = write_buffers + i * data_size;
                io_vecs[i].iov_len = data_size;
                write_references[i] = 0;
                write_buffer_stack[i] = static_cast<unsigned int>(i);
            }

            char* read_buffers = nullptr;
            if (posix_memalign(reinterpret_cast<void**>(&read_buffers), 4096, buffer_count * buffer_size) != 0) {
                throw std::runtime_error("posix_memalign failed");
            }

            unsigned int completed_ops[cli_per_thread] = {};

            run_ring(32000, 1,
                 ring_init(
                    io_uring_register(_r_ring_fd, IORING_REGISTER_BUFFERS, io_vecs.data(), buffer_count);
                    io_uring_register(_r_ring_fd, IORING_REGISTER_FILES, client_sockets, cli_per_thread);
                    submit_provide_buffers(read_buffers, data_size, buffer_count, 1, 0, 0, (void*) nullptr);
                    for (int cli_index = 0; cli_index < cli_per_thread; ++cli_index) {
                        submit_read_multishot(cli_index, 0, 1, 0, (void*) (uintptr_t) cli_index);
                    }
                    ready_clients.fetch_sub(1, std::memory_order_release);
                    while (ready_clients.load(std::memory_order_acquire) != 0) {
                        std::this_thread::yield();
                    }

                    for (int cli_index = 0; cli_index < cli_per_thread; ++cli_index) {

                    }
                    // wait on every other client to be ready to write, then submit 1 write/read for each client, wait for reads
                 ),
                 ring_loop(),
                 ring_completions(
                     on_multishot_read(
                         if (completion.res <= 0) {
                             std::cerr << "Socket closed or error during read: " << std::strerror(-completion.res) << std::endl;
                         } else {
                             const auto buf_id = completion.flags >> IORING_CQE_BUFFER_SHIFT;
                             auto buffer = read_buffers + (buf_id * buffer_size);
                             const auto cli_index = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(c_data));
                             completed_ops[cli_index] += 1;
                             if (completed_ops[cli_index] == ops_per_client) {
                                 completed_clients.fetch_sub(1, std::memory_order_release);
                             } else {
                                 // write out again
                             }

                             submit_provide_buffers(
                                 buffer,
                                 buffer_size,
                                 1,
                                 1,
                                 buf_id,
                                 0,
                                 (void*) nullptr
                            );
                         }
                     )

                     on_zc_send (
                         if (completion.res < 0) {
                            std::cerr << "SEND_ZC failed: " << std::strerror(-completion.res) << std::endl;
                         } else if (completion.flags & IORING_CQE_F_NOTIF) {
                             const auto buffer_index = *static_cast<unsigned int*>(c_data);
                             write_references[buffer_index] -= 1;
                             if (write_references[buffer_index] == 0) {
                                 write_buffer_stack[write_buffer_stack_head++] = buffer_index;
                             }
                         }
                     )
                 )
            );

            delete[] read_buffers;
            delete[] write_buffers;
        });
    }

    while (ready_clients.load(std::memory_order_acquire) != 0) { std::this_thread::yield(); }
    const auto start_time = std::chrono::steady_clock::now();
    while (completed_clients.load(std::memory_order_acquire) != 0) { std::this_thread::yield(); }
    const auto end_time = std::chrono::steady_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    std::cout << "Duration: " << duration << std::endl;

    for (auto &worker: local_workers) { worker.join(); }
}

template <size_t max_clients, size_t message_size>
unsigned char run_raft_udp_client_listener(
    const unsigned char node_id,
    const unsigned char leader_id,
    Address client_listen_address,
    ClientQueue<max_clients>& client_queue,
    std::vector<std::thread>& local_workers
) {
     if (node_id == leader_id) {
        std::promise<unsigned char> promise;
        std::future<unsigned char> waiter = promise.get_future();
        local_workers.emplace_back([&promise, &client_listen_address, client_queue]() {
            int listener_sockets[1];
            const auto listener_socket = socket(AF_INET, SOCK_DGRAM, 0);
            if (listener_socket < 0) {
                throw std::runtime_error("Failed to create server socket");
            }

            constexpr int flag = 1;
            if (setsockopt(listener_socket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
                close(listener_socket);
                throw std::runtime_error("error setting reuseaddr opt");
            }

            if (setsockopt(listener_socket, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof(flag)) < 0) {
                close(listener_socket);
                throw std::runtime_error("Failed to set SO_REUSEPORT");
            }

            if (bind(listener_socket, reinterpret_cast<sockaddr*>(&client_listen_address.addr), sizeof(client_listen_address.addr)) < 0) {
                close(listener_socket);
                throw std::runtime_error("bind failed");
            }

            tune_udp_socket(listener_socket);
            listener_sockets[0] = listener_socket;

            constexpr unsigned int buffer_count = 15800;
            std::vector<iovec> io_vecs(buffer_count);
            const auto write_buffers = new char[buffer_count * message_size];
            const auto write_references = new unsigned int[buffer_count];
            const auto write_buffer_stack = new unsigned int[buffer_count];
            auto write_buffer_stack_head = buffer_count;

            for (size_t i = 0; i < buffer_count; ++i) {
                io_vecs[i].iov_base = write_buffers + i * message_size;
                io_vecs[i].iov_len = message_size;
                write_references[i] = 0;
                write_buffer_stack[i] = static_cast<unsigned int>(i);
            }

            char *read_buffers = nullptr;
            if (posix_memalign(reinterpret_cast<void **>(&read_buffers), 4096, buffer_count * message_size) != 0) {
                throw std::runtime_error("posix_memalign failed");
            }

            run_ring(32000, 1,
                ring_init(
                    io_uring_register(_r_ring_fd, IORING_REGISTER_FILES, listener_sockets, 1);
                    io_uring_register(_r_ring_fd, IORING_REGISTER_BUFFERS, io_vecs.data(), buffer_count);
                    submit_provide_buffers(read_buffers, message_size, buffer_count, 1, 0, 0, (void*) nullptr);
                    submit_read_multishot(0, 0, 1, 0, (void*) nullptr); // swap to recvmsg
                    promise.set_value(_r_ring_fd);
                ),
                ring_loop(),
                ring_completions(
                    on_multishot_read(
                        if (completion.res <= 0) {
                            std::cerr << "Socket closed or error during read: " << std::strerror(-completion.res) << std::endl;
                        } else {
                            const auto buf_id = completion.flags >> IORING_CQE_BUFFER_SHIFT;
                            auto buffer = read_buffers + (buf_id * message_size);


                        }
                    );
                )
            );

            delete[] read_buffers;
            delete[] write_buffers;
            delete[] write_references;
            delete[] write_buffer_stack;
        });

        return waiter.get();
    }

    return 0;
}

template<size_t log_size, size_t message_size, size_t threads, size_t pipes, size_t max_clients>
void run_raft_udp(
    const unsigned char node_id,
    const unsigned char leader_id,
    Address client_listen_address,
    std::vector<Address> &peers
) {
    static_assert(pipes < log_size, "log size must be larger than total pipes");
    std::vector<std::thread> local_workers;
    local_workers.reserve(threads);

    ClientQueue<max_clients> client_queue;
    const unsigned char client_listen_fd = run_raft_udp_client_listener(node_id, leader_id, client_listen_address, client_queue, local_workers);
    std::cout << "client listen fd" << client_listen_fd << std::endl;

    std::atomic<unsigned int> apply_index{0};
    auto acks = new std::atomic<unsigned int>[log_size];
    char **log = new char *[log_size];
    for (size_t i = 0; i < log_size; ++i) {
        log[i] = new char[message_size];
    }

    const auto pipes_per_thread = pipes / threads;
    for (unsigned int thread_id = 0; thread_id < threads; thread_id++) {
        local_workers.emplace_back([&apply_index, &acks, &peers, pipes_per_thread, node_id, leader_id, thread_id]() {
            int peer_sockets[peers.size() + 1];
            const auto majority = peers.size() / 2 + 1;
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

            if (bind(server_socket, reinterpret_cast<sockaddr *>(&node_address.addr), sizeof(node_address.addr)) < 0) {
                close(server_socket);
                throw std::runtime_error("bind failed");
            }

            tune_udp_socket(server_socket);

            peer_sockets[0] = server_socket;
            for (int i = 0; i < peers.size(); ++i) {
                auto peer = peers[i];
                const auto client_socket = socket(AF_INET, SOCK_DGRAM, 0);
                tune_udp_socket(client_socket);
                if (connect(client_socket, reinterpret_cast<sockaddr *>(&peer.addr), sizeof(peer.addr)) != 0) {
                    throw std::runtime_error("Failed to connect to peer");
                }
                peer_sockets[i + 1] = client_socket;
            }

            constexpr unsigned int buffer_count = 15800;
            std::vector<iovec> io_vecs(buffer_count);
            const auto write_buffers = new char[buffer_count * message_size];
            const auto write_references = new unsigned int[buffer_count];
            const auto write_buffer_stack = new unsigned int[buffer_count];
            auto write_buffer_stack_head = buffer_count;

            for (size_t i = 0; i < buffer_count; ++i) {
                io_vecs[i].iov_base = write_buffers + i * message_size;
                io_vecs[i].iov_len = message_size;
                write_references[i] = 0;
                write_buffer_stack[i] = static_cast<unsigned int>(i);
            }

            char *read_buffers = nullptr;
            if (posix_memalign(reinterpret_cast<void **>(&read_buffers), 4096, buffer_count * message_size) != 0) {
                throw std::runtime_error("posix_memalign failed");
            }

            run_ring(
                32000, 1,
                ring_init(
                    io_uring_register(_r_ring_fd, IORING_REGISTER_FILES, peer_sockets, peers.size()+1);
                    io_uring_register(_r_ring_fd, IORING_REGISTER_BUFFERS, io_vecs.data(), buffer_count);
                    submit_provide_buffers(read_buffers, message_size, buffer_count, 1, 0, 0, (void*) nullptr);
                    submit_read_multishot(0, 0, 1, 0, (void*) nullptr);

                    if (node_id == leader_id) {
                        for (int i = 0; i < pipes_per_thread; i++) {
                            const auto write_buffer_head = --write_buffer_stack_head;
                            if (write_buffer_head < 0) throw std::runtime_error("write_buffer_id is negative");
                            unsigned short buffer_index = write_buffer_stack[write_buffer_head];
                            write_references[buffer_index] = peers.size() - 1;
                            const auto buffer = write_buffers + buffer_index * message_size;
                            const unsigned int slot = thread_id * (pipes / threads) + i;
                            buffer[0] = OP_PROPOSE;
                            std::memcpy(buffer + 1, &slot, sizeof(slot));

                            std::atomic<unsigned int>& ack = acks[slot];
                            ack.fetch_or(APPLIED_MASK, std::memory_order_release);

                            buffer[5] = MSG_NULL;
                            for (int peer_id = 0; peer_id < peers.size(); ++peer_id) {
                                if (peer_id != node_id) {
                                    submit_send_zc(
                                        peer_id + 1,
                                        buffer,
                                        6,
                                        nullptr,
                                        0,
                                        buffer_index,
                                        0,
                                        0,
                                        0,
                                        (void*) &write_buffer_stack[write_buffer_head]
                                    );
                                }
                            }
                        }
                        fprintf(stderr, "Wrote out all packets from thread: %d\n", thread_id);
                    }
                ),
                ring_loop(),
                ring_completions(
                    on_multishot_read(
                        if (completion.res <= 0) {
                            std::cerr << "Socket closed or error during read: " << std::strerror(-completion.res) << std::endl;
                        } else {
                            const auto buf_id = completion.flags >> IORING_CQE_BUFFER_SHIFT;
                            auto buffer = read_buffers + (buf_id * message_size);
                            const auto op = buffer[0];
                            unsigned int slot;
                            std::memcpy(&slot, buffer + 1, sizeof(slot));
                            unsigned int seq_id;
                            std::memcpy(&seq_id, buffer + 5, sizeof(seq_id));

                            if (op == OP_PROPOSE) {
                                const bool is_null = buffer[5] == MSG_NULL;
                                const auto write_buffer_head = --write_buffer_stack_head;
                                if (write_buffer_head < 0) throw std::runtime_error("write_buffer_id is negative");
                                unsigned short buffer_index = write_buffer_stack[write_buffer_head];
                                write_references[buffer_index] = peers.size() - 1;
                                const auto ack_buffer = write_buffers + buffer_index * message_size;
                                ack_buffer[0] = OP_ACK;
                                std::memcpy(ack_buffer + 1, &slot, sizeof(slot));

                                submit_send_zc(
                                    leader_id + 1,
                                    ack_buffer,
                                    5,
                                    nullptr,
                                    0,
                                    buffer_index,
                                    0,
                                    0,
                                    0,
                                    (void*) &write_buffer_stack[write_buffer_head]
                                );
                            } else if (op == OP_ACK) {
                                if (node_id != leader_id) { throw std::runtime_error("Non-leader recieved ack!"); }
                                // fprintf(stderr, "[%d] Got ack for slot: %d\n", thread_id, slot);

                                auto current_slot = slot;
                                while (true) {
                                    std::atomic<unsigned int>& ack = acks[current_slot];
                                    unsigned int current_ack = ack.load(std::memory_order_acquire);
                                    const auto current_apply = apply_index.load(std::memory_order_acquire);
                                    // can only increment ack if (ack & start != 0)
                                    if ((current_ack & APPLIED_MASK) == 0 || current_slot < current_apply) {
                                        fprintf(stderr, "Breaking: %d\n", (current_slot < current_apply) ? 1 : 0);
                                        break;
                                    }
                                    unsigned int next = current_ack;
                                    if (current_slot == slot) {
                                        ++next;
                                    }

                                    const auto has_majority = (next & (APPLIED_MASK - 1)) >= majority - 1;
                                    const auto should_apply = current_slot == current_apply && has_majority;
                                    if (should_apply) next = next & (APPLIED_MASK - 1);
                                    if (ack.compare_exchange_strong(current_ack, next)) {
                                        if (should_apply) {
                                            fprintf(stderr, "[%d] Applied index: %d\n", thread_id, current_slot);
                                            apply_index.fetch_add(1, std::memory_order_release);
                                            ack.store(0, std::memory_order_release);
                                            ++current_slot;
                                            // propose out next
                                        } else break;
                                    }
                                }
                            }

                            submit_provide_buffers(
                                buffer,
                                message_size,
                                1,
                                1,
                                buf_id,
                                0,
                                (void*) nullptr
                            );
                            }
                        )

                        on_zc_send (
                            if (completion.res < 0) {
                                std::cerr << "SEND_ZC failed: " << std::strerror(-completion.res) << std::endl;
                            } else if (completion.flags & IORING_CQE_F_NOTIF) {
                                const auto buffer_index = *static_cast<unsigned int*>(c_data);
                                write_references[buffer_index] -= 1;
                                if (write_references[buffer_index] == 0) {
                                    write_buffer_stack[write_buffer_stack_head++] = buffer_index;
                                }
                            }
                        )
                )
            );

            for (int i = 0; i < peers.size(); ++i) { close(peer_sockets[i]); }
            close(server_socket);
            delete[] write_buffers;
            delete[] write_references;
            delete[] write_buffer_stack;
            free(read_buffers);
        });
    }


    for (auto &worker: local_workers) { worker.join(); }
    for (size_t i = 0; i < log_size; ++i) { delete[] log[i]; }
    delete[] log;
    delete[] acks;
}
