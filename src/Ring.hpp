#pragma once

#include <memory>
#include <atomic>
#include <linux/io_uring.h>
#include <sys/mman.h>
#include <vector>

#include "RingConfig.hpp"  // Your RingConfig header

int io_uring_setup(unsigned int entries, io_uring_params* params);
int io_uring_enter(int ring_fd, unsigned int to_submit, unsigned int min_complete, unsigned int flags);
int io_uring_register(unsigned int ring_fd, unsigned int opcode, void* arg, unsigned int nr_args);


struct FutexWaitEntry {
    unsigned long long uaddr;
    unsigned int val;
    unsigned int flags;
};

/**
 * @class Ring
 * @brief Wrapper around io_uring that manages submission and completion queues.
 * Provides methods for submitting operations and retrieving completions,
 * abstracting low-level io_uring syscalls and memory management.
 */




inline std::atomic RUNNING{true};



constexpr unsigned MAX_FILES = 128;

// class Ring {
//
// public:
//
//     const RingConfig& config;                                   /**< Configuration object used to setup the ring */
//     int ring_fd = -1;                                           /**< File descriptor for the io_uring instance */
//     mutable std::atomic<unsigned int> atomic_to_submit = 0;
//     std::unique_ptr<io_uring_params> params;                    /**< io_uring parameters */
//     void* sq_ptr = nullptr;                                     /**< Mapped pointer to submission queue ring */
//     void* cq_ptr = nullptr;                                     /**< Mapped pointer to completion queue ring */
//     io_uring_sqe* sqes = nullptr;                               /**< Pointer to submission queue entries */
//     std::atomic<unsigned int>* sq_head = nullptr;               /**< Submission queue head pointer */
//     std::atomic<unsigned int>* sq_tail = nullptr;               /**< Submission queue tail pointer */
//     unsigned int sq_ring_mask;                       /**< Submission queue ring mask */
//     unsigned int* sq_array = nullptr;                           /**< Submission queue array */
//     std::atomic<unsigned int>* sq_flags = nullptr;              /**< Submission queue flags */
//     std::vector<int> fixed_fds;
//     std::bitset<MAX_FILES> used_fixed_fds;
//
//
//     /**
//      * @brief Constructs an io_uring Ring with the specified configuration.
//      * @param config Reference to a RingConfig object that controls setup parameters.
//      * @throws std::runtime_error if io_uring setup or mmap operations fail.
//      */
//     explicit Ring(const RingConfig &config);
//
//     /**
//      * @brief Destroys the Ring, closes file descriptors and unmaps memory.
//      */
//     ~Ring();
//
//
//     void register_buffers(char *buffer_pool, unsigned int total_buffers, unsigned int buffer_size) const;
//
// };

struct UserData {
    unsigned char opcode;

};

#define submit(...) \
do { \
    const unsigned int s_tail = sq_tail->load(std::memory_order::relaxed); \
    const unsigned int s_index = s_tail & sq_ring_mask; \
    sqes[s_index] = io_uring_sqe { \
        __VA_ARGS__ \
    }; \
    sq_array[s_index] = s_index; \
    sq_tail->fetch_add(1, std::memory_order_release); \
} while (0)

#define submit_with(op, s_flags, s_data) \
do { \
    (s_data)->opcode = (op); \
    submit( \
        .opcode = (op), \
        .flags = (s_flags), \
        .user_data = reinterpret_cast<unsigned long>(s_data) \
    ); \
} while (0)

#define submit_with_args(op, s_flags, s_data, ...) \
do { \
    (s_data)->opcode = (op); \
    submit( \
        .opcode = (op), \
        .flags = (s_flags), \
        __VA_ARGS__, \
        .user_data = reinterpret_cast<unsigned long>(s_data) \
    ); \
} while (0)

#define submit_no_op(s_flags, s_data) \
    submit_with(IORING_OP_NOP, s_flags, s_data)

#define submit_connect(socket, target_address, target_address_len, s_flags, s_data) \
    submit_with_args(IORING_OP_CONNCT, s_flags, s_data, \
        .fd = (socket), \
        .addr = reinterpret_cast<unsigned long>(target_address), \
        .addr2 = target_address_len, \
    )

#define submit_accept(socket, client_address, client_address_len, s_flags, s_data) \
    submit_with_args(IORING_OP_ACCEPT, s_flags, s_data, \
        .fd = socket, \
        .addr = reinterpret_cast<unsigned long>(client_address), \
        .addr2 = reinterpret_cast<unsigned long>(client_address_len), \
        .ioprio = IORING_ACCEPT_MULTISHOT \
    )

#define submit_socket(domain, type, protocol, s_flags, s_data) \
    submit_with_args(IORING_OP_SOCKET, s_flags, s_data, \
        .fd = (domain), \
        .off = (type), \
        .len = (protocol) \
    )

#define submit_set_sockopt(fd, level, optname, optlen, optval, cmd_flags, s_flags, s_data) \
    submit_with_args(IORING_OP_URING_CMD, s_flags, s_data, \
        .optval = reinterpret_cast<unsigned long>(optval), \
        .optname = (optname), \
        .optlen = (optlen), \
        .level = (level), \
        .cmd_op = SOCKET_URING_OP_SETSOCKOPT, \
        .uring_cmd_flags = cmd_flags \
    )

#define submit_listen(fd, backlog, s_flags, s_data) \
    submit_with_args(IORING_OP_LISTEN, s_flags, s_data, \
        .fd = (fd), \
        .len = (backlog) \
    )

#define submit_bind(fd, address, address_len, s_flags, s_data) \
    submit_with_args(IORING_OP_BIND, s_flags, s_data, \
        .fd = (fd), \
        .addr = reinterpret_cast<unsigned long>(address), \
        .addr2 = (address_len) \
    )

#define submit_close(fd, slot, s_flags, s_data) \
    submit_with_args(IORING_OP_CLOSE, s_flags, s_data, \
        .fd = (fd), \
        .file_slot = (slot) \
    )

#define submit_provide_buffers(buffer_pool, buffer_size, buffer_count, buffer_group, start_index, s_flags, s_data) \
    submit_with_args(IORING_OP_PROVIDE_BUFFERS, s_flags, s_data, \
        .addr = reinterpret_cast<unsigned long>(buffer_pool), \
        .len = buffer_size, \
        .fd = buffer_count, \
        .buf_group = buffer_group \
        .off = start_index \
)

#define submit_remove_buffers(buffer_count, buffer_group, s_flags, s_data) \
    submit_with_args(IORING_OP_REMOVE_BUFFERS, s_flags, s_data, \
        .fd = buffer_count, \
        .buf_group = buffer_group \
    )

#define on_no_op(block)     if (c_data->opcode == IORING_OP_NOP)    { block }
#define on_socket(block)    if (c_data->opcode == IORING_OP_SOCKET) { block }
#define on_listen(block)    if (c_data->opcode == IORING_OP_LISTEN) { block }
#define on_bind(block)      if (c_data->opcode == IORING_OP_BIND)   { block }
#define on_close(block)     if (c_data->opcode == IORING_OP_CLOSE)  { block }

#define ring_init(code) do { code } while(0);
#define ring_block(code) do { code } while(0);

#define run_ring(size, pin, init, block) \
    do { \
        static_assert(std::is_integral_v<decltype(size)>, "size must be an integer");\
        static_assert(std::is_integral_v<decltype(pin)>, "pin must be an integer");  \
        static_assert(pin >= 0, "pin must be >= 0");                                  \
        static_assert(size > 0, "size must be > 0");                                  \
        struct io_uring_params* params =  (struct io_uring_params*)malloc(sizeof(struct io_uring_params)); \
        if (!params) throw std::runtime_error("io_uring_params alloc failed"); \
        std::memset(params, 0, sizeof(io_uring_params)); \
        params->flags |= IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_CLAMP | IORING_SETUP_SQPOLL | IORING_SETUP_SUBMIT_ALL; \
        params->sq_thread_idle = UINT32_MAX; \
        params->cq_entries = (size) * 2; \
        const auto ring_fd = io_uring_setup(size, params); \
        \
        const auto sq_ring_size = params->sq_off.array + params->sq_entries * sizeof(__u32); \
        const auto sq_ptr = mmap(nullptr, sq_ring_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ring_fd,IORING_OFF_SQ_RING); \
        if (sq_ptr == MAP_FAILED) throw std::runtime_error("mmap failed on sq_ptr"); \
        const auto sq_base = static_cast<char *>(sq_ptr); \
        const auto sq_head = reinterpret_cast<std::atomic<unsigned int>*>(sq_base + params->sq_off.head); \
        const auto sq_tail = reinterpret_cast<std::atomic<unsigned int>*>(sq_base + params->sq_off.tail); \
        const auto sq_ring_mask = *reinterpret_cast<unsigned int*>(sq_base + params->sq_off.ring_mask); \
        const auto sq_array = reinterpret_cast<unsigned int*>(sq_base + params->sq_off.array); \
        const auto sq_flags = reinterpret_cast<std::atomic<unsigned int>*>(sq_base + params->sq_off.flags); \
        const auto sqes_size = params->sq_entries * sizeof(io_uring_sqe); \
        const auto sqes = static_cast<io_uring_sqe *>(mmap(nullptr, sqes_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,ring_fd, IORING_OFF_SQES)); \
        if (sqes == MAP_FAILED) throw std::runtime_error("mmap failed on sqes"); \
        \
        const auto cq_ring_size = params->cq_off.cqes + params->cq_entries * sizeof(io_uring_cqe); \
        const auto cq_ptr = mmap(nullptr, cq_ring_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ring_fd, IORING_OFF_CQ_RING); \
        if (cq_ptr == MAP_FAILED) throw std::runtime_error("mmap failed on cq_ptr"); \
        const auto cq_base = static_cast<char*>(cq_ptr); \
        auto& cq_head = *reinterpret_cast<std::atomic<unsigned int>*>(cq_base + params->cq_off.head); \
        const auto& cq_tail = *reinterpret_cast<std::atomic<unsigned int>*>(cq_base + params->cq_off.tail); \
        const auto cq_ring_mask = *reinterpret_cast<unsigned int*>(cq_base + params->cq_off.ring_mask); \
        const auto* cqes = reinterpret_cast<io_uring_cqe*>(cq_base + params->cq_off.cqes); \
        unsigned int head = cq_head.load(std::memory_order_seq_cst); \
        init \
        if (sq_flags->load(std::memory_order_relaxed) & IORING_SQ_NEED_WAKEUP) \
            io_uring_enter(ring_fd, 0, 0, IORING_ENTER_SQ_WAKEUP); \
        while (RUNNING.load(std::memory_order_relaxed)) { \
            const auto tail = cq_tail.load(std::memory_order_acquire); \
            if (head == tail) continue; \
            while (head != tail) { \
                const io_uring_cqe& completion = cqes[head++ & cq_ring_mask]; \
                UserData* c_data = reinterpret_cast<UserData*>(completion.user_data); \
                block \
            } \
            cq_head.store(head, std::memory_order_release); \
        } \
        free(params); \
        if (ring_fd >= 0) close(ring_fd); \
        if (sq_ptr) munmap(sq_ptr, params->sq_off.array + params->sq_entries * sizeof(unsigned int)); \
        if (sqes) munmap(sqes, params->sq_entries * sizeof(io_uring_sqe)); \
        if (cq_ptr) munmap(cq_ptr, params->cq_off.cqes + params->cq_entries * sizeof(io_uring_cqe)); \
    } while (0) \
