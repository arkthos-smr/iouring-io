#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <csignal>
#include "Ring.hpp"
#include "Raft.h"



void shutdown(const int signum) {
    std::cout << "Received signal: " << signum << std::endl;
    RUNNING.store(false);
}

int main() {
    struct sigaction action {};
    action.sa_handler = shutdown;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT,  &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGQUIT, &action, nullptr);
    sigaction(SIGHUP,  &action, nullptr);

    std::vector<std::thread> workers{};
    std::vector peers{
        Address { "127.0.0.1", 6984 },
        Address { "127.0.0.1", 6982 }
    };
    workers.emplace_back([&](){ run_raft_udp<100,100, 1>(100, 0, 0, peers); });
    workers.emplace_back([&]() { run_raft_udp<100,100, 1>(1, 1, 0, peers); });

    // std::vector<std::thread> workers{};
    // workers.emplace_back([] {
    //     auto noopdata = UserData{};
    //     auto user_data = UserData{};
    //     auto t = Test { 69 };
    //
    //
    //     // submit_socket(ring, AF_INET, SOCK_STREAM, 0, 0, user_data);
    //     // submit_no_op(ring, 0, &noopdata);
    //     // submit_no_op(ring, 1, & user_data);
    //     //
    //
    //     // submit_noop(ring, AF_INET, SOCK_STREAM, 0, 0, noopdata);
    //     run_ring(
    //         128, 1,
    //         ring_init(
    //             submit_socket(AF_INET, SOCK_STREAM, 0, 0, &t);
    //             submit_no_op(0, &noopdata);
    //         ),
    //         ring_block(
    //             on_no_op(
    //                 std::cout << "Got a no op" << std::endl;
    //                 // ops += 1;
    //                 // submit_no_op(0, c_data);
    //             );
    //
    //             on_socket(
    //                 std::cout << "got socket index: " << completion.res << std::endl;
    //                 auto socket_data = static_cast<Test*>(c_data);
    //                 std::cout << "Scoket data: " << socket_data->i << std::endl;
    //             );
    //
    //
    //         )
    //     );
    // });

    while (RUNNING.load()) {
        pause();
    }

    for (auto &worker : workers) {
        worker.join();
    }

    std::cout << "Shutdown!" << std::endl;

    return 0;
}
