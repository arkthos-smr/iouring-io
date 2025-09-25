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

struct Test {
    int i;
};

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
    workers.emplace_back([] {
        auto noopdata = UserData{};
        auto user_data = UserData{};
        auto t = Test { 69 };


        // submit_socket(ring, AF_INET, SOCK_STREAM, 0, 0, user_data);
        // submit_no_op(ring, 0, &noopdata);
        // submit_no_op(ring, 1, & user_data);
        //

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
                submit_socket(AF_INET, SOCK_STREAM, 0, 0, &t);
                submit_no_op(0, &noopdata);
                submit_no_op(0, &user_data);
            );,
            ring_block(
                on_no_op(
                    std::cout << "Got a no op" << std::endl;
                    // ops += 1;
                    // submit_no_op(0, c_data);
                );

                on_socket(
                    std::cout << "got socket index: " << completion.res << std::endl;
                    auto socket_data = static_cast<Test*>(c_data);
                    std::cout << "Scoket data: " << socket_data->i << std::endl;
                );


            );
        );
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
