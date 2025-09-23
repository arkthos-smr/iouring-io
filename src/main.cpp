#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/futex.h>
#include <csignal>
#include <vector>

#include "Ring.hpp"


void shutdown(const int signum) {
    std::cout << "Received signal: " << signum << std::endl;
    RUNNING.store(false);
}

int main() {
    int c = 10;
    {
        int c = 2;
    }

    struct sigaction action {};
    action.sa_handler = shutdown;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT,  &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGQUIT, &action, nullptr);
    sigaction(SIGHUP,  &action, nullptr);

    std::vector<std::thread> workers{};
    workers.emplace_back([] {
        auto noopdata = UserData{};
        auto user_data = UserData{};


        // submit_socket(ring, AF_INET, SOCK_STREAM, 0, 0, user_data);
        // submit_no_op(ring, 0, &noopdata);
        // submit_no_op(ring, 1, & user_data);
        //
        // sockaddr_in client_addr{};
        // socklen_t addrlen = sizeof(client_addr);
        // submit_accept(ring, 0, &client_addr, &addrlen, 0, &noopdata);
        // submit_noop(ring, AF_INET, SOCK_STREAM, 0, 0, noopdata);
        unsigned int ops = 0;

        std::thread test([&]{
            unsigned int lastOps = ops;
            while (RUNNING.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                std::cout << "Total ops: " << (ops - lastOps) << std::endl;
                lastOps = ops;
            }
        });
        test.detach();

        run_ring(
            128, 1,
            ring_init(
                submit_no_op(0, &noopdata);
                submit_no_op(0, &user_data);
            ),
            ring_block(
                on_no_op(
                    ops += 1;
                    submit_no_op(0, c_data);
                );
            )
        );;
        // run(ring, [&](const io_uring_cqe *completion) {
        //     onNoOp(
        //         ops.fetch_add(1, std::memory_order_relaxed);
        //         ring.submit_no_op(69);
        //     );
        // });
        // run_ring<RAFT_TCP>(ring);
        // sockaddr_in addr{};
        // addr.sin_family = AF_INET;
        // addr.sin_port = htons(12346);
        // addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        //
        // sockaddr_in cli_addr{};
        // socklen_t cli_len = sizeof(cli_addr);
        //
        // constexpr size_t read_buffer_size = 4000;
        // constexpr size_t write_buffer_size = 4096;
        // constexpr size_t read_buffer_count = 100;
        // constexpr size_t write_buffer_count = 100;
        // constexpr unsigned int READ_GROUP = 1;
        // const auto read_buffer_pool = std::make_unique<char[]>(read_buffer_size * read_buffer_count);
        // const auto write_buffer_pool = std::make_unique<char[]>(write_buffer_count * write_buffer_size);
        //
        // ring->register_buffers(write_buffer_pool.get(), write_buffer_count, write_buffer_size);
        //
        // ring->submit_provide_buffers(
        //     read_buffer_pool.get(),
        //     read_buffer_size,
        //     read_buffer_count,
        //     READ_GROUP,
        //     0,
        //     1,
        //     0
        // );
        //
        // ring->submit_socket(AF_INET, SOCK_STREAM, 0, 1001, 0);
        // ring->advance_sq();
        //
        // // msg.msg_name = nullptr; // for connected sockets
        // // msg.msg_namelen = 0;
        //
        //
        //
        // while (RUNNING.load(std::memory_order_relaxed)) {
        //     if (const auto completions = ring->get_cqes(WaitMode::NON_BLOCKING); completions != nullptr) {
        //         for (size_t i = 0; i < ring->get_completed(); ++i) {
        //             const auto &completion = completions[i & ring->get_cq_ring_mask()];
        //             std::cout << "\n" << std::endl;
        //             if (completion.user_data == 1) {
        //                 std::cout << "Got write buffer: " << completion.user_data << std::endl;
        //             }
        //             if (completion.user_data == 2) {
        //                 std::cout << "Got read buffer: " << completion.user_data << " " << completion.res << std::endl;
        //             }
        //             if (completion.user_data == 1001) {
        //                 if (completion.res < 0) {
        //                     std::cout << "Creating socket returned error: " << completion.res << std::endl;
        //                 } else {
        //                     const auto socket = completion.res;
        //                     std::cout << "Socket created: " << socket << std::endl;
        //                     int reuse = 1;
        //
        //                     ring->submit_set_sockopt(socket, SOL_SOCKET, SO_REUSEADDR, sizeof(reuse), &reuse, 0, 999, IOSQE_IO_LINK);
        //                     ring->submit_bind(socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr), 1002, IOSQE_IO_LINK);
        //                     ring->submit_listen(socket, 128, 1003, IOSQE_IO_LINK);
        //                     ring->submit_socket(AF_INET, SOCK_STREAM, 0, 2001, IOSQE_IO_LINK);
        //                     ring->submit_accept_multishot(socket, reinterpret_cast<sockaddr*>(&cli_addr), &cli_len, 1004, 0);
        //                 }
        //             } else if (completion.user_data == 1002) {
        //                 if (completion.res < 0) {
        //                     std::cout << "Bind socket returned error: " << completion.res << std::endl;
        //                 } else {
        //                     std::cout << "Socket now bound!" << std::endl;
        //                 }
        //             } else if (completion.user_data == 1003) {
        //                 if (completion.res < 0) {
        //                     std::cout << "Listen socket returned error: " << completion.res << std::endl;
        //                 } else {
        //                     std::cout << "Socket now listening!" << std::endl;
        //                 }
        //             } else if (completion.user_data == 1004) {
        //                 if (completion.res < 0) {
        //                     std::cout << "Error accepting socket: " << completion.res << std::endl;
        //                 } else {
        //                     std::cout << "Got an accept with socket: " << completion.res << std::endl;
        //                     const auto socket = completion.res;
        //                     const auto socket_index = ring->register_fixed_socket(socket);
        //                     ring->submit_read_multishot(socket_index, 0, READ_GROUP, 1234, 0);
        //
        //                     ring->submit_send_zc(
        //                         socket_index,
        //                         write_buffer_pool.get(),
        //                         write_buffer_size,
        //                         nullptr,
        //                         0,
        //                         0,
        //                         0,
        //                         0,
        //                         69,
        //                         0
        //                     );
        //
        //                     ring->submit_send_zc(
        //                         socket_index,
        //                         write_buffer_pool.get() + (1 * write_buffer_size),
        //                         write_buffer_size,
        //                         nullptr,
        //                         0,
        //                         1,
        //                         0,
        //                         0,
        //                         69,
        //                         0
        //                     );
        //                 }
        //             } else if (completion.user_data == 2001) {
        //                 if (completion.res < 0) {
        //                      std::cout << "Creating client socket returned error: " << completion.res << std::endl;
        //                  } else {
        //                      const auto socket = completion.res;
        //                      const auto socket_index = ring->register_fixed_socket(socket);
        //                      std::cout << "Client socket created: " << socket << std::endl;
        //                      ring->submit_connect(socket, reinterpret_cast<sockaddr *>(&addr), sizeof(addr), 2002, 0);
        //                      ring->submit_read_multishot(socket_index, 0, READ_GROUP, 12345, 0);
        //                  }
        //             } else if (completion.user_data == 2002) {
        //                 if (completion.res < 0) {
        //                     std::cout << "Connect client socket returned error: " << completion.res << std::endl;
        //                 } else {
        //                     std::cout << "Client now connected: " << completion.res << std::endl;
        //                 }
        //             } else if (completion.user_data == 999) {
        //                 if (completion.res < 0) {
        //                     std::cout << "Setting socket option error: " << completion.res << std::endl;
        //                 } else {
        //                     std::cout << "Set socket option: " << completion.res << std::endl;
        //                 }
        //             } else if (completion.user_data == 1234) {
        //                 if (completion.res < 0) {
        //                     std::cout << "Got bad server side read: " << completion.res << std::endl;
        //                 } else {
        //                     if (completion.flags & IORING_CQE_F_BUFFER) {
        //                         uint16_t buffer_id = completion.flags >> IORING_CQE_BUFFER_SHIFT;
        //                         std::cout << "Server read used buffer id: " << buffer_id << std::endl;
        //
        //                         ring->submit_provide_buffers(
        //                             read_buffer_pool.get() + (buffer_id * read_buffer_size),
        //                             read_buffer_size,
        //                             1,
        //                             READ_GROUP,
        //                             buffer_id,
        //                             2,
        //                             0
        //                         );
        //                     }
        //
        //                     if (completion.flags & IORING_CQE_F_MORE) {
        //                         std::cout << "More data coming for this read..." << std::endl;
        //                     } else {
        //                         std::cout << "This was the final read for this user_data." << std::endl;
        //                     }
        //                 }
        //             } else if (completion.user_data == 12345) {
        //                 if (completion.res < 0) {
        //                     if (completion.res == -ENOBUFS) {
        //                         std::cout << "No read bufferse available!" << std::endl;
        //                     } else {
        //                         std::cout << "Got bad client side read: " << completion.res << std::endl;
        //                     }
        //                 } else {
        //                     std::cout << "Client Read total of: " << completion.res << std::endl;
        //                 }
        //             } else if (completion.user_data == 69) {
        //                 if (completion.res < 0) {
        //                     std::cout << "Got bad client side write " << completion.res << std::endl;
        //                 } else {
        //                     std::cout << "Client Write total of: " << completion.res << " - " << completion.flags << std::endl;
        //                     if (completion.flags & IORING_CQE_F_MORE) {
        //                         std::cout << "More is coming" << std::endl;
        //                     }
        //                     if (completion.flags & IORING_CQE_F_NOTIF) {
        //                         std::cout << "Buffer is better to be reused." << std::endl;
        //                     }
        //                 }
        //             }
        //         }
        //
        //         ring->advance_cq();
        //         ring->advance_sq();
        //     }
        // }
    });

    while (RUNNING.load()) {
        pause();
    }

    std::cout << "Joining worker threads" << std::endl;

    for (auto &worker : workers) {
        worker.join();
    }

    std::cout << "Shutdown!" << std::endl;

    return 0;
}
