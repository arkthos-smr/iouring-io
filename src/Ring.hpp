#pragma once

#include <memory>
#include <atomic>
#include <iostream>
#include <linux/io_uring.h>
#include <sys/mman.h>
#include <vector>
#include <asm-generic/unistd.h>
#include <unistd.h>

// Wrapper for io_uring_setup syscall with error checking
__attribute__((always_inline)) inline int io_uring_setup(const unsigned entries, io_uring_params *params) {
    const int ring_fd = syscall(__NR_io_uring_setup, entries, params);
    if (ring_fd < 0) {
        throw std::runtime_error("io_uring_setup failed: " + std::string(std::strerror(errno)));
    }
    return ring_fd;
}

// Wrapper for io_uring_enter syscall with error checking
__attribute__((always_inline)) inline int io_uring_enter(
    const int ring_fd,
    const unsigned int to_submit,
    const unsigned int min_complete,
    const unsigned int flags
) {
    const int result = syscall(__NR_io_uring_enter, ring_fd, to_submit, min_complete, flags, nullptr, 0);
    if (result < 0) {
        throw std::runtime_error("io_uring_enter failed: " + std::string(std::strerror(errno)));
    }
    return result;
}

// Wrapper for io_uring_register syscall with error checking
__attribute__((always_inline)) inline int io_uring_register(
    const unsigned int ring_fd,
    const unsigned int op,
    void *arg,
    const unsigned int nr_args
) {
    const int result = syscall(__NR_io_uring_register, ring_fd, op, arg, nr_args);
    if (result < 0) {
        std::string err_msg = "io_uring_register buffers failed: ";
        err_msg += strerror(-result);
        err_msg += " (" + std::to_string(result) + ")\n";
        throw std::runtime_error(err_msg);
    }
    return result;
}


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


constexpr unsigned long long ADDR_MASK = (static_cast<unsigned long long>(1) << 48) - 1;
constexpr unsigned long long OP_SHIFT = 1ULL << 48;
constexpr unsigned MAX_FILES = 128;


#define submit(...) \
do { \
    const unsigned int _r_s_tail = _r_sq_tail->load(std::memory_order::relaxed); \
    const unsigned int _r_s_index = _r_s_tail & _r_sq_ring_mask; \
    _r_sqes[_r_s_index] = io_uring_sqe { __VA_ARGS__ }; \
    _r_sq_array[_r_s_index] = _r_s_index; \
    _r_sq_tail->fetch_add(1, std::memory_order_release); \
} while(0)

#define submit_with(op, s_flags, s_data) \
do {                                     \
    static_assert(std::is_pointer_v<decltype(s_data)>, "s_data must be a pointer"); \
    submit( \
        .opcode = (op), \
        .flags = (s_flags), \
        .user_data = reinterpret_cast<unsigned long long>(s_data) | ((op) * OP_SHIFT) \
    ); \
} while(0)

#define submit_with_args(op, s_flags, s_data, ...) \
do { \
    static_assert(std::is_pointer_v<decltype(s_data)>, "s_data must be a pointer"); \
    submit( \
        .opcode = (op), \
        .flags = (s_flags), \
        __VA_ARGS__, \
        .user_data = reinterpret_cast<unsigned long long>(s_data) | ((op) * OP_SHIFT) \
    ); \
} while (0)

#define submit_no_op(s_flags, s_data) \
    submit_with(IORING_OP_NOP, s_flags, s_data)

#define submit_connect(socket, target_address, target_address_len, s_flags, s_data) \
    submit_with_args(IORING_OP_CONNECT, s_flags, s_data,                             \
        .fd = (socket), \
        .addr = reinterpret_cast<unsigned long>(target_address), \
        .addr2 = target_address_len \
    )

#define submit_socket(domain, type, protocol, s_flags, s_data) \
    submit_with_args(IORING_OP_SOCKET, s_flags, s_data, \
        .fd = (domain), \
        .off = (type), \
        .len = (protocol) \
    )

#define submit_set_sockopt(socket, opt_level, opt_name, opt_len, opt_val, cmd_flags, s_flags, s_data) \
    submit_with_args(IORING_OP_URING_CMD, s_flags, s_data, \
        .fd = socket, \
        .optval = reinterpret_cast<unsigned long>(opt_val), \
        .optname = (opt_name), \
        .optlen = (opt_len), \
        .level = (opt_level), \
        .cmd_op = SOCKET_URING_OP_SETSOCKOPT, \
        .uring_cmd_flags = cmd_flags \
    )

#define submit_listen(socket, backlog, s_flags, s_data) \
    submit_with_args(IORING_OP_LISTEN, s_flags, s_data, \
        .fd = socket, \
        .len = (backlog) \
    )

#define submit_bind(socket, address, address_len, s_flags, s_data) \
    submit_with_args(IORING_OP_BIND, s_flags, s_data, \
        .fd = socket, \
        .addr = reinterpret_cast<unsigned long>(address), \
        .addr2 = (address_len) \
    )

#define submit_close(socket, slot, s_flags, s_data) \
    submit_with_args(IORING_OP_CLOSE, s_flags, s_data, \
        .fd = socket, \
        .file_index = (slot) \
    )

#define submit_provide_buffers(buffer_pool, buffer_size, buffer_count, buffer_group, start_index, s_flags, s_data) \
    submit_with_args(IORING_OP_PROVIDE_BUFFERS, s_flags, s_data, \
        .addr = reinterpret_cast<unsigned long>(buffer_pool), \
        .len = buffer_size, \
        .fd = buffer_count, \
        .buf_group = buffer_group, \
        .off = start_index \
)

#define submit_remove_buffers(buffer_count, buffer_group, s_flags, s_data) \
    submit_with_args(IORING_OP_REMOVE_BUFFERS, s_flags, s_data, \
        .fd = buffer_count, \
        .buf_group = buffer_group \
    )


#define submit_timeout(id, spec, count, timeout_flags, s_flags) \
    submit_with_args(IORING_OP_TIMEOUT, s_flags, id,            \
        .off = count,                                           \
        .len = 1,                                               \
        .addr = reinterpret_cast<unsigned long>(spec),          \
        .timeout_flags = timeout_flags                          \
    )

#define submit_timeout_remove(id, timeout_flags, s_flags, s_data) \
    submit_with_args(IORING_OP_TIMEOUT_REMOVE, s_flags, s_data,   \
        .addr = id,                                               \
        .timeout_flags = timeout_flags                            \
    )

#define submit_timeout_update(id, spec, timeout_flags, s_flags, s_data) \
    submit_with_args(IORING_OP_TIMEOUT_REMOVE, s_flags, s_data,         \
        .off = reinterpret_cast<unsigned long int>(spec),               \
        .addr = id,                                                     \
        .timeout_flags = timeout_flags                                  \
    )

#define submit_futex_wait(uaddr, expected, futex_flags, s_flags, s_data) \
    submit_with_args(IORING_OP_FUTEX_WAIT, s_flags, s_data,              \
        .addr = reinterpret_cast<unsigned long>(uaddr),                  \
        .addr2 = expected,                                               \
        .addr3 = 0xFFFFFFFF,                                             \
        .fd = futex_flags                                                \
    )

#define submit_futex_wake(uaddr, wake_count, futex_flags, s_flags, s_data) \
    submit_with_args(IORING_OP_FUTEX_WAKE, s_flags, s_data,                \
        .addr = uaddr,                                                     \
        .addr2 = wake_count,                                               \
        .addr3 = 0xFFFFFFFF,                                               \
        .fd = futex_flags                                                  \
    )

#define submit_msg_ring(target_ring_fd, len, cmd, src_fd, dst_fd, msg_ring_flags, s_flags, s_data) \
    submit_with_args(IORING_OP_MSG_RING, s_flags, s_data,                                          \
        .fd = target_ring_fd,                                                                      \
        .len = len,                                                                                \
        .addr = cmd,                                                                               \
        .addr3 = src_fd,                                                                           \
        .file_index = dst_fd,                                                                      \
        .msg_ring_flags = msg_ring_flags                                                           \
    )

#define submit_accept_multishot(socket, cli_addr, cli_addr_len, s_flags, s_data) \
    submit_with_args(IORING_OP_ACCEPT, s_flags, s_data,              \
        .fd = socket,                                                    \
        .addr = reinterpret_cast<unsigned long>(cli_addr),               \
        .addr2 = reinterpret_cast<unsigned long>(cli_addr_len),          \
        .ioprio = IORING_ACCEPT_MULTISHOT                            \
    )

#define submit_sendmsg_zc(fd_slot, msg, buf_index, s_flags, s_data)            \
    submit_with_args(IORING_OP_SENDMSG_ZC, s_flags | IOSQE_FIXED_FILE, s_data, \
        .fd = fd_slot,                                                         \
        .addr = reinterpret_cast<unsigned long>(msg),                          \
        .len = 1,                                                              \
        .buf_index = buf_index,                                                \
        .ioprio = IORING_RECVSEND_FIXED_BUF                                    \
    )

#define submit_send_zc(fd_slot, buf, buf_len, rem_addr, rem_addr_len, s_buffer_index, message_flags, zc_flags, s_flags, s_data) \
    submit_with_args(IORING_OP_SEND_ZC, s_flags | IOSQE_FIXED_FILE, s_data,                                \
        .fd = fd_slot,                                                                                     \
        .addr = reinterpret_cast<unsigned long>(buf),                                                      \
        .len = buf_len,                                                                                        \
        .msg_flags = zc_flags,                                                                             \
        .addr2 = reinterpret_cast<unsigned long>(rem_addr),                                                    \
        .addr_len = rem_addr_len,                                                                              \
        .buf_index = s_buffer_index,                                                                            \
        .msg_flags = message_flags,                                                                            \
        .ioprio = zc_flags | IORING_RECVSEND_FIXED_BUF                                                     \
    )

#define submit_write_fixed(fd_slot, buf, buf_len, offset, s_buffer_index, s_flags, s_data) \
    submit_with_args(IORING_OP_WRITE_FIXED, s_flags | IOSQE_FIXED_FILE, s_data,            \
        .fd       = fd_slot,                                                               \
        .addr     = reinterpret_cast<unsigned long>(buf),                                  \
        .len      = buf_len,                                                               \
        .off      = offset,                                                                \
        .buf_index= s_buffer_index                                                         \
    )

#define submit_recv_multishot(fd_slot, buf_group, mshot_len, mshot_total_len, s_flags, s_data) \
    submit_with_args(IORING_OP_RECV, s_flags | IOSQE_BUFFER_SELECT | IOSQE_FIXED_FILE, s_data, \
        .fd = fd_slot,                                                                         \
        .buf_group = buf_group,                                                                \
        .ioprio = IORING_RECV_MULTISHOT,                                                       \
        .len = mshot_len,                                                                      \
        .optlen = mshot_total_len,                                                             \
        .msg_flags = 0                                                                         \
    )

#define submit_read_multishot(fd_slot, offset, buffer_group, s_flags, s_data)                               \
    submit_with_args(IORING_OP_READ_MULTISHOT, s_flags | IOSQE_BUFFER_SELECT | IOSQE_FIXED_FILE, s_data, \
        .fd = fd_slot,                                                                                   \
        .off = offset,                                                                                   \
        .buf_group = buffer_group                                                                           \
    )

#define submit_recv_zc_multishot(fd_slot, zcrx_index, s_flags, s_data)      \
    submit_with_args(IORING_OP_RECV_ZC, s_flags | IOSQE_FIXED_FILE, s_data, \
        .fd = fd_slot,                                                      \
        .ioprio = IORING_RECVSEND_FIXED_BUF | IORING_RECV_MULTISHOT,        \
        .zcrx_ifq_idx = zcrx_index                                          \
    )

#define on_futex_wake(block)    if (c_opcode == IORING_OP_FUTEX_WAKE) { block }
#define on_futex_wait(block)    if (c_opcode == IORING_OP_FUTEX_WAIT) { block }
#define on_listen(block)   if (c_opcode == IORING_OP_LISTEN) { block }
#define on_bind(block)   if (c_opcode == IORING_OP_BIND) { block }
#define on_cmd(block)       if (c_opcode == IORING_OP_URING_CMD)  { block }
#define on_no_op(block)     if (c_opcode == IORING_OP_NOP)        { block }
#define on_socket(block)    if (c_opcode == IORING_OP_SOCKET)     { block }
#define on_listen(block)    if (c_opcode == IORING_OP_LISTEN)     { block }
#define on_bind(block)      if (c_opcode == IORING_OP_BIND)       { block }
#define on_close(block)     if (c_opcode == IORING_OP_CLOSE)      { block }
#define on_accept(block)    if (c_opcode == IORING_OP_ACCEPT)     { block }
#define on_connect(block)   if (c_opcode == IORING_OP_CONNECT)     { block }
#define on_provide_buffers(block) if (c_opcode == IORING_OP_PROVIDE_BUFFERS) { block }
#define on_multishot_read(block) if (c_opcode == IORING_OP_READ_MULTISHOT)         { block }
#define on_zc_send(block) if (c_opcode == IORING_OP_SEND_ZC)              { block }

#define ring_init(code) do { code } while(0);
#define ring_loop(code) do { code } while(0);
#define ring_completions(code) do { code } while(0);

#define run_ring(size, pin, init, loop, block) \
do { \
    static_assert(std::is_integral_v<decltype(size)>, "size must be an integer"); \
    static_assert(std::is_integral_v<decltype(pin)>, "pin must be an integer");   \
    static_assert(pin >= 0, "pin must be >= 0"); \
    static_assert(size > 0, "size must be > 0"); \
    struct io_uring_params* _r_params = (struct io_uring_params*)malloc(sizeof(struct io_uring_params)); \
    if (!_r_params) throw std::runtime_error("io_uring_params alloc failed"); \
    std::memset(_r_params, 0, sizeof(io_uring_params)); \
    _r_params->flags |= IORING_SETUP_SINGLE_ISSUER | IORING_SETUP_CLAMP | IORING_SETUP_SQPOLL | IORING_SETUP_SUBMIT_ALL; \
    _r_params->sq_thread_idle = UINT32_MAX; \
    _r_params->cq_entries = (size) * 2; \
    const auto _r_ring_fd = io_uring_setup(size, _r_params); \
    \
    const auto _r_sq_ring_size = _r_params->sq_off.array + _r_params->sq_entries * sizeof(__u32); \
    const auto _r_sq_ptr = mmap(nullptr, _r_sq_ring_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, _r_ring_fd, IORING_OFF_SQ_RING); \
    if (_r_sq_ptr == MAP_FAILED) throw std::runtime_error("mmap failed on sq_ptr"); \
    const auto _r_sq_base = static_cast<char *>(_r_sq_ptr); \
    const auto _r_sq_head = reinterpret_cast<std::atomic<unsigned int>*>(_r_sq_base + _r_params->sq_off.head); \
    const auto _r_sq_tail = reinterpret_cast<std::atomic<unsigned int>*>(_r_sq_base + _r_params->sq_off.tail); \
    const auto _r_sq_ring_mask = *reinterpret_cast<unsigned int*>(_r_sq_base + _r_params->sq_off.ring_mask); \
    const auto _r_sq_array = reinterpret_cast<unsigned int*>(_r_sq_base + _r_params->sq_off.array); \
    const auto _r_sq_flags = reinterpret_cast<std::atomic<unsigned int>*>(_r_sq_base + _r_params->sq_off.flags); \
    const auto _r_sqes_size = _r_params->sq_entries * sizeof(io_uring_sqe); \
    const auto _r_sqes = static_cast<io_uring_sqe *>(mmap(nullptr, _r_sqes_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, _r_ring_fd, IORING_OFF_SQES)); \
    if (_r_sqes == MAP_FAILED) throw std::runtime_error("mmap failed on sqes"); \
    \
    const auto _r_cq_ring_size = _r_params->cq_off.cqes + _r_params->cq_entries * sizeof(io_uring_cqe); \
    const auto _r_cq_ptr = mmap(nullptr, _r_cq_ring_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, _r_ring_fd, IORING_OFF_CQ_RING); \
    if (_r_cq_ptr == MAP_FAILED) throw std::runtime_error("mmap failed on cq_ptr"); \
    const auto _r_cq_base = static_cast<char*>(_r_cq_ptr); \
    auto& _r_cq_head = *reinterpret_cast<std::atomic<unsigned int>*>(_r_cq_base + _r_params->cq_off.head); \
    const auto& _r_cq_tail = *reinterpret_cast<std::atomic<unsigned int>*>(_r_cq_base + _r_params->cq_off.tail); \
    const auto _r_cq_ring_mask = *reinterpret_cast<unsigned int*>(_r_cq_base + _r_params->cq_off.ring_mask); \
    const auto* _r_cqes = reinterpret_cast<io_uring_cqe*>(_r_cq_base + _r_params->cq_off.cqes); \
    unsigned int _r_cq_head_local = _r_cq_head.load(std::memory_order_seq_cst); \
        \
        \
        \
    init \
    if (_r_sq_flags->load(std::memory_order_relaxed) & IORING_SQ_NEED_WAKEUP) \
        io_uring_enter(_r_ring_fd, 0, 0, IORING_ENTER_SQ_WAKEUP); \
    while (RUNNING.load(std::memory_order_relaxed)) { \
        loop \
        const auto _r_tail = _r_cq_tail.load(std::memory_order_acquire); \
        if (_r_cq_head_local == _r_tail) continue; \
        while (_r_cq_head_local != _r_tail) { \
            const io_uring_cqe& completion = _r_cqes[_r_cq_head_local++ & _r_cq_ring_mask]; \
            unsigned char c_opcode = (completion.user_data) >> 48; \
            void* c_data = reinterpret_cast<void*>((completion.user_data) & ADDR_MASK); \
            block \
        } \
        _r_cq_head.store(_r_cq_head_local, std::memory_order_release); \
        if (_r_sq_flags->load(std::memory_order_relaxed) & IORING_SQ_NEED_WAKEUP) \
            io_uring_enter(_r_ring_fd, 0, 0, IORING_ENTER_SQ_WAKEUP); \
    } \
    free(_r_params); \
    if (_r_ring_fd >= 0) close(_r_ring_fd); \
    if (_r_sq_ptr) munmap(_r_sq_ptr, _r_params->sq_off.array + _r_params->sq_entries * sizeof(unsigned int)); \
    if (_r_sqes) munmap(_r_sqes, _r_params->sq_entries * sizeof(io_uring_sqe)); \
    if (_r_cq_ptr) munmap(_r_cq_ptr, _r_params->cq_off.cqes + _r_params->cq_entries * sizeof(io_uring_cqe)); \
} while(0)
