#include "Ring.hpp"

#include <cassert>
#include <stdexcept>
#include <iostream>
#include <sys/mman.h>
#include <asm-generic/unistd.h>
#include <unistd.h>
#include <bits/types/siginfo_t.h>
#include <linux/futex.h>


// Wrapper for io_uring_setup syscall with error checking
int io_uring_setup(const unsigned entries, io_uring_params *params) {
    const int ring_fd = syscall(__NR_io_uring_setup, entries, params);
    if (ring_fd < 0) {
        throw std::runtime_error("io_uring_setup failed: " + std::string(std::strerror(errno)));
    }
    return ring_fd;
}

// Wrapper for io_uring_enter syscall with error checking
int io_uring_enter(
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
int io_uring_register(
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


// Ring::Ring(const RingConfig &config) : config(config) {
//     // Allocate and zero-initialize io_uring_params struct
//     params = std::make_unique<io_uring_params>();
//     std::memset(params.get(), 0, sizeof(io_uring_params));
//
//     // Basic flags set based on config
//     params->flags |= IORING_SETUP_SINGLE_ISSUER; // Single thread submitting requests
//     params->flags |= IORING_SETUP_CLAMP; // Clamp queue sizes to max supported
//     params->cq_entries = config.get_size() * 2;
//
//     // Setup poll or IOPOLL modes if specified in config
//     if (config.get_type() == RingIOType::SQPOLL) {
//         params->flags |= IORING_SETUP_SQPOLL;
//         params->sq_thread_idle = config.get_sq_thread_idle().count(); // Idle timeout for SQ poll thread
//
//         if (config.get_pin() > -1) {
//             // Pin SQ poll thread to specific CPU
//             params->flags |= IORING_SETUP_SQ_AFF;
//             params->sq_thread_idle = config.get_pin();
//         }
//     } else if (config.get_type() == RingIOType::IOPOLL) {
//         params->flags |= IORING_SETUP_IOPOLL; // Enable busy polling for I/O completions
//     }
//
//     // Submit all flag (submit all SQEs in one syscall)
//     if (config.get_submit_all()) {
//         params->flags |= IORING_SETUP_SUBMIT_ALL;
//     }
//
//
//     // Perform the actual io_uring setup syscall, returns ring FD
//     ring_fd = io_uring_setup(config.get_size(), params.get());
//
//     // Calculate size of SQ ring structure to mmap it
//     const auto sq_ring_size = params->sq_off.array + params->sq_entries * sizeof(__u32); \
//     sq_ptr = mmap(nullptr, sq_ring_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ring_fd,IORING_OFF_SQ_RING); \
//     if (sq_ptr == MAP_FAILED) throw std::runtime_error("mmap failed on sq_ptr"); \
//     const auto sqes_size = params->sq_entries * sizeof(io_uring_sqe); \
//     sqes = static_cast<io_uring_sqe *>(mmap(nullptr, sqes_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,ring_fd, IORING_OFF_SQES)); \
//     if (sqes == MAP_FAILED) throw std::runtime_error("mmap failed on sqes"); \
//     const auto cq_ring_size = params->cq_off.cqes + params->cq_entries * sizeof(io_uring_cqe); \
//     cq_ptr = mmap(nullptr, cq_ring_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, ring_fd, IORING_OFF_CQ_RING); \
//     if (cq_ptr == MAP_FAILED) throw std::runtime_error("mmap failed on cq_ptr"); \
//
//     // Setup pointers to atomic ring state variables and buffers within the mapped memory
//
//
//     const auto sq_head = reinterpret_cast<std::atomic<unsigned int> *>(static_cast<char *>(sq_ptr) + params->sq_off.head); \
//     const auto sq_tail = reinterpret_cast<std::atomic<unsigned int> *>(static_cast<char *>(sq_ptr) + params->sq_off.tail); \
//     const auto sq_ring_mask = *reinterpret_cast<unsigned int *>(static_cast<char *>(sq_ptr) + params->sq_off.ring_mask); \
//     const auto sq_array = reinterpret_cast<unsigned int *>(static_cast<char *>(sq_ptr) + params->sq_off.array); \
//     const auto sq_flags = reinterpret_cast<std::atomic<unsigned int> *>(static_cast<char *>(sq_ptr) + params->sq_off.flags); \
//
//     fixed_fds = std::vector(MAX_FILES, -1);
//     io_uring_register(ring_fd, IORING_REGISTER_FILES, fixed_fds.data(), MAX_FILES);
//
//     // const int event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
//     // uint64_t shutdown_val = 0;
//     //
//     // ring.submit_read(
//     //     event_fd, // Fixed fd index from io_uring_register_files
//     //     &shutdown_val, // Pointer to buffer
//     //     sizeof(shutdown_val), // Size to read
//     //     SHUTDOWN_USERDATA, // User data tag
//     //     0 // Flags (add IOSQE_IO_LINK or others if needed)
//     // );
// }
//
//
// Ring::~Ring() {
//     std::cout << "Destroying ring" << std::endl;
//
//     // Close ring fd if valid
//     if (ring_fd != -1) close(ring_fd);
//
//     // Unmap all mapped memory regions
//     if (sq_ptr) munmap(sq_ptr, params->sq_off.array + params->sq_entries * sizeof(unsigned int));
//     if (sqes) munmap(sqes, params->sq_entries * sizeof(io_uring_sqe));
//     if (cq_ptr) munmap(cq_ptr, params->cq_off.cqes + params->cq_entries * sizeof(io_uring_cqe));
// }


// unsigned int Ring::get_ring_fd() const {
//     return ring_fd;
// }

// io_uring_sqe *Ring::get_sqe() const {
//     // Load current head and tail to calculate usage
//     const auto head = sq_head->load(std::memory_order_acquire);
//     //hopefully noop
//     const auto tail = sq_tail->load(std::memory_order_relaxed);
//     // this would need to be atomic < to_submit >
//     // if constexpr DEBUG_MODE {
//         const auto used = tail - head;
//
//         // Check if there's room in the submission queue
//         if (const auto space = params->sq_entries - used; space <= 0) {
//             throw std::runtime_error("out of space in submission queue!");
//         }
//     //}
//     // Compute ring index with wrap-around
//     const auto index = tail & *sq_ring_mask;
//
//     // Get SQE pointer and store index into SQ array
//     io_uring_sqe *entry = &sqes[index];
//     sq_array[index] = index;
//
//     // Clear the SQE to avoid uninitialized fields
//     //this is just super slow for no reason
//     memset(entry, 0, sizeof(*entry));
//
//     //and this is just fundamentally broken
//     return entry;
// }


// this
// void Ring::advance_sq() const {
//     if (params->flags & IORING_SETUP_SQPOLL) { // sqpoll
//         // Wake up kernel thread if sleeping
//         if ((sq_flags->load(std::memory_order_acquire) & IORING_SQ_NEED_WAKEUP) != 0) {
//             io_uring_enter(ring_fd, 0, 0, IORING_ENTER_SQ_WAKEUP);
//         }
//     }
// if (to_submit > 0) {
//     // Publish new SQEs by advancing the tail pointer
//     sq_tail->store(sq_tail->load(std::memory_order_relaxed) + to_submit, std::memory_order_release);
//
//     // Check if sqpoll enabled
//     if (params->flags & IORING_SETUP_SQPOLL) { // sqpoll
//         // Wake up kernel thread if sleeping
//         if ((sq_flags->load(std::memory_order_acquire) & IORING_SQ_NEED_WAKEUP) != 0) {
//             io_uring_enter(ring_fd, 0, 0, IORING_ENTER_SQ_WAKEUP);
//         }
//     } else { // interrupt driven
//         // Submit the SQEs directly
//         io_uring_enter(ring_fd, to_submit, 0, 0);
//     }
//
//     // Reset pending submission count
//     to_submit = 0;
// }
// }

// io_uring_cqe* Ring::get_cqes() const {
//     auto head = cq_head->load(std::memory_order_acquire);
//     auto tail = cq_tail->load(std::memory_order_acquire);
//
//     // Check if any completions are already available
//     if (tail - head > 0) {
//         const auto index = head & *cq_ring_mask;
//         completed = tail - head;
//         return &cqes[index];
//     }
//
//     // No completions available
//     return nullptr;
// }
//
// unsigned int Ring::get_completed() const {
//     return completed;  // Number of CQEs returned by the last get_cqes() call
// }
//
// unsigned int Ring::get_cq_ring_mask() const {
//     return *cq_ring_mask;  // Used to wrap CQ indexes
// }

//
// void Ring::advance_cq() const {
//     if (completed > 0) {
//         // Tell the kernel we’ve consumed completion entries
//         cq_head->store(cq_head->load(std::memory_order_relaxed) + completed, std::memory_order_release);
//         completed = 0;
//     }
// }

// void Ring::register_buffers(
//     char *buffer_pool,
//     const unsigned int total_buffers,
//     const unsigned int buffer_size
// ) const {
//     // std::vector<iovec> vecs(total_buffers);
//     // for (unsigned int i = 0; i < total_buffers; ++i) {
//     //     vecs[i].iov_base = buffer_pool + i * buffer_size;
//     //     vecs[i].iov_len = buffer_size;
//     // }
//     // io_uring_register(ring_fd, IORING_REGISTER_BUFFERS, vecs.data(), total_buffers);;
// }

// void Ring::submit_no_op(const unsigned long data) const {
//     // submit(
//     //     .opcode = IORING_OP_NOP,
//     //     .user_data = data
//     // );
// }

// void Ring::submit_timeout(
//     const __kernel_timespec *spec,
//     const unsigned int count,
//     const unsigned int flags,
//     const unsigned long id
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_TIMEOUT;
//     sqe->off = count; // Number of events to wait for before timeout triggers (usually 1)
//     sqe->len = 1; // Must be 1 for basic timeout
//     sqe->addr = reinterpret_cast<unsigned long>(spec); // Pointer to timeout duration (in kernel timespec)
//     sqe->user_data = id; // Completion identifier
//     sqe->timeout_flags = flags;
// }
//
// // void Ring::test(
// //     const __kernel_timespec *spec,
// //     const unsigned int count,
// //     const unsigned int flags,
// //     const unsigned long id
// // ) {
// //     submit(
// //         .opcode = IORING_OP_TIMEOUT,
// //         .off = count,
// //         .len = 1,
// //         .addr = reinterpret_cast<unsigned long>(spec),
// //         .user_data = id,
// //         .timeout_flags = flags,
// //     )
// // }
//
// void Ring::submit_timeout_remove(
//     const unsigned int flags,
//     const unsigned long id
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_TIMEOUT_REMOVE;
//     sqe->addr = id; // User data of the timeout to cancel
//     sqe->timeout_flags = flags; // Usually 0, or e.g., IORING_TIMEOUT_MULTISHOT for multishot timeouts
// }
//
//
// void Ring::submit_timeout_update(
//     const __kernel_timespec *next,
//     const unsigned int flags,
//     const unsigned long id
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_TIMEOUT_REMOVE;
//
//     // Updating an existing timeout: addr = target ID, off = pointer to new time
//     sqe->off = reinterpret_cast<unsigned long int>(next); // Pointer to new timeout spec
//     sqe->addr = id; // ID of timeout to update
//     sqe->timeout_flags = flags | IORING_TIMEOUT_UPDATE; // Must include UPDATE flag
// }
//
// void Ring::submit_socket(
//     const int domain,
//     const int type,
//     const int protocol,
//     const unsigned long user_data,
//     const unsigned int flags
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_SOCKET;
//     sqe->fd = domain; // e.g., AF_INET, AF_UNIX
//     sqe->off = type; // e.g., SOCK_STREAM
//     sqe->len = protocol; // Usually 0
//     sqe->user_data = user_data; // Completion identifier
//     sqe->flags = flags; // e.g., IOSQE_IO_LINK to chain with next
// }
//
// int Ring::register_fixed_socket(const int fd) {
//     auto next_index = -1;
//     for (int i = 0; i < MAX_FILES; ++i) {
//         if (!used_fixed_fds.test(i)) {
//             used_fixed_fds.set(i);
//             next_index = i;
//             break;
//         }
//     }
//     if (next_index == -1) {
//         throw std::runtime_error("No more space for fixed fds. Need larger max files.");
//     }
//
//     fixed_fds[next_index] = fd;
//     io_uring_files_update update{};
//     update.offset = next_index;
//     update.resv = 0;
//     update.fds = reinterpret_cast<__u64>(&fixed_fds[next_index]);
//     io_uring_register(ring_fd, IORING_REGISTER_FILES_UPDATE, &update, 1);
//     return next_index;
// }
//
// void Ring::unregister_fixed_socket(const int slot) {
//     if (slot < 0 || slot >= MAX_FILES) throw std::out_of_range("Invalid fixed slot index");
//     if (!used_fixed_fds.test(slot)) return;
//     used_fixed_fds.reset(slot);
//
//     fixed_fds[slot] = -1;
//     io_uring_files_update update{};
//     update.offset = slot;
//     update.resv = 0;
//     update.fds = reinterpret_cast<__u64>(&fixed_fds[slot]);
//     io_uring_register(ring_fd, IORING_REGISTER_FILES_UPDATE, &update, 1);
// }
//
// void Ring::submit_listen(
//     const int fd,
//     const unsigned int backlog,
//     const unsigned long user_data,
//     const unsigned int flags
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_LISTEN;
//
//     sqe->fd = fd; // Socket file descriptor
//     sqe->len = backlog; // Listen backlog size
//     sqe->user_data = user_data; // Completion identifier
//     sqe->flags = flags; // Optional submission flags (e.g., IOSQE_IO_LINK)
// }
//
// void Ring::submit_bind(
//     const int fd,
//     const sockaddr *addr,
//     const socklen_t addr_len,
//     const unsigned long user_data,
//     const unsigned int flags
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_BIND;
//
//     sqe->fd = fd; // Socket file descriptor
//     sqe->addr = reinterpret_cast<unsigned long>(addr); // Pointer to sockaddr struct
//     sqe->addr2 = addr_len; // Size of the address struct
//     sqe->user_data = user_data; // Completion identifier
//     sqe->flags = flags; // Submission flags (e.g., link)
// }
//
//
// void Ring::submit_close(
//     const int fd,
//     const unsigned int file_slot,
//     const unsigned long user_data,
//     const unsigned int flags
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_CLOSE;
//
//     sqe->fd = fd; // File descriptor to close
//     sqe->file_index = file_slot; // Fixed file slot (optional, 0 if not used)
//     sqe->user_data = user_data; // Completion identifier
//     sqe->flags = flags; // Submission flags
// }
//
// // Submit single futex wait
// void Ring::submit_futex_wait(const int *uaddr, const int expected_val, const unsigned int futex_flags,
//                              const unsigned long user_data) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_FUTEX_WAIT;
//     sqe->addr = reinterpret_cast<unsigned long>(uaddr);
//     sqe->addr2 = expected_val;
//     sqe->addr3 = 0xFFFFFFFF; // must set a mask here
//     sqe->fd = futex_flags; // e.g. FUTEX_WAIT | FUTEX_PRIVATE_FLAG
//     sqe->user_data = user_data;
// }
//
// // Submit futex wake
// void Ring::submit_futex_wake(const int *uaddr, const int wake_count, const unsigned int futex_flags,
//                              const unsigned long user_data) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_FUTEX_WAKE;
//     sqe->addr = reinterpret_cast<unsigned long>(uaddr);
//     sqe->addr2 = wake_count; // Number of waiters to wake
//     sqe->addr3 = 0xFFFFFFFF; // Mask (all bits) - REQUIRED!
//     sqe->fd = futex_flags; // FUTEX2 flags
//     sqe->user_data = user_data;
// }
//
// void Ring::submit_msg_ring(
//     const int target_ring_fd, // fd of the target ring
//     const unsigned int len, // can be any u32 value, becomes res on target
//     const unsigned long user_data, // becomes user_data on target CQE
//     const unsigned int flags, // must be subset of IORING_MSG_RING_MASK
//     const unsigned int cmd, // optional: some command or small data, from sqe->addr
//     const unsigned int src_fd, // if sending an fd
//     const unsigned int dst_fd // destination fd or flags for cqe
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_MSG_RING;
//     sqe->fd = target_ring_fd;
//     sqe->off = user_data;
//     sqe->len = len;
//     sqe->addr = cmd;
//     sqe->addr3 = src_fd;
//     sqe->file_index = dst_fd;
//     sqe->msg_ring_flags = flags;
//     sqe->user_data = user_data; // so you can identify this on your own ring
// }
//
// void Ring::submit_provide_buffers(
//     const void *buffer_pool,
//     const unsigned int buffer_size,
//     const unsigned int count,
//     const unsigned int buf_group,
//     const unsigned int start_index,
//     const unsigned long user_data,
//     const unsigned int flags
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_PROVIDE_BUFFERS;
//     sqe->addr = reinterpret_cast<unsigned long>(buffer_pool); // pointer to buffers
//     sqe->len = buffer_size; // size of each buffer
//     sqe->fd = count; // number of buffers
//     sqe->buf_group = buf_group; // buffer group ID
//     sqe->off = static_cast<__u64>(start_index); // starting buffer index
//     sqe->user_data = user_data; // so you can identify this on your own ring
//     sqe->flags = flags;
// }
//
// void Ring::submit_remove_buffers(
//     const unsigned int count, // Number of buffers to remove
//     const unsigned int buf_group, // Buffer group ID
//     const unsigned long user_data, // User data to identify completion
//     const unsigned int flags
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_REMOVE_BUFFERS;
//     sqe->fd = count;
//     sqe->buf_group = buf_group;
//     sqe->user_data = user_data;
//     sqe->flags = flags;
// }
//
// void Ring::submit_connect(
//     const int fd,
//     const sockaddr *addr,
//     const socklen_t addr_len,
//     const unsigned long user_data,
//     const unsigned int flags
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_CONNECT;
//     sqe->fd = fd;
//     sqe->addr = reinterpret_cast<unsigned long>(addr);
//     sqe->addr2 = addr_len;
//     sqe->flags = flags;
//     sqe->user_data = user_data;
// }
//
//
// void Ring::submit_set_sockopt(
//     const int fd,
//     const int level,
//     const int optname,
//     const int optlen,
//     const void *optval,
//     const int cmd_flags,
//     const unsigned long user_data,
//     const unsigned int flags
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_URING_CMD;
//     sqe->fd = fd;
//     sqe->optval = reinterpret_cast<unsigned long>(optval);
//     sqe->optname = optname;
//     sqe->optlen = optlen;
//     sqe->level = level;
//     sqe->cmd_op = SOCKET_URING_OP_SETSOCKOPT;
//     sqe->uring_cmd_flags = cmd_flags;
//     sqe->flags = flags;
//     sqe->user_data = user_data;
// }
//
// void Ring::submit_accept_multishot(
//     const int fd,
//     const sockaddr *addr,
//     const socklen_t *addr_len,
//     const unsigned long user_data,
//     const unsigned int flags
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_ACCEPT;
//     sqe->fd = fd;
//     sqe->addr = reinterpret_cast<unsigned long>(addr);
//     sqe->addr2 = reinterpret_cast<unsigned long>(addr_len);
//     sqe->ioprio = IORING_ACCEPT_MULTISHOT;
//     sqe->flags = flags;
//     sqe->user_data = user_data;
// }
//
// void Ring::submit_sendmsg_zc(
//     const int fd_slot,
//     const msghdr *msg,
//     const int buf_index,
//     const unsigned long user_data,
//     const unsigned int flags
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_SENDMSG_ZC;
//     sqe->fd = fd_slot;
//     sqe->addr = reinterpret_cast<unsigned long>(msg);
//     sqe->len = 1;
//     sqe->buf_index = buf_index;
//     sqe->ioprio = IORING_RECVSEND_FIXED_BUF;
//     sqe->user_data = user_data;
//     sqe->flags = flags | IOSQE_FIXED_FILE;
// }
//
// void Ring::submit_send_zc(
//     const int fd_slot,
//     const void *buf,
//     const size_t len,
//     const sockaddr *addr,
//     const socklen_t addr_len,
//     const int buf_index,
//     const unsigned int msg_flags,
//     const unsigned int zc_flags,
//     const unsigned long user_data,
//     const unsigned int flags
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_SEND_ZC;
//     sqe->fd = fd_slot;
//     sqe->addr = reinterpret_cast<unsigned long>(buf);
//     sqe->len = len;
//     sqe->msg_flags = zc_flags;
//
//     if (addr != nullptr) {
//         sqe->addr2 = reinterpret_cast<unsigned long>(addr);
//         sqe->addr_len = addr_len;
//     }
//     sqe->buf_index = buf_index;
//     sqe->msg_flags = msg_flags;
//     sqe->ioprio = zc_flags | IORING_RECVSEND_FIXED_BUF;
//     sqe->user_data = user_data;
//     sqe->flags = flags | IOSQE_FIXED_FILE;
// }
//
// void Ring::submit_recv_multishot(
//     const int fd_slot,
//     const unsigned int buf_group,
//     const size_t mshot_len,
//     const size_t mshot_total_len,
//     const unsigned long user_data,
//     const unsigned int flags
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_RECV;
//     sqe->fd = fd_slot;
//     sqe->buf_group = buf_group;
//     sqe->ioprio = IORING_RECV_MULTISHOT;
//     sqe->len = mshot_len;
//     sqe->optlen = mshot_total_len;
//     sqe->msg_flags = 0;
//     sqe->flags = flags | IOSQE_BUFFER_SELECT | IOSQE_FIXED_FILE;
//     sqe->user_data = user_data;
// }
//
// void Ring::submit_read_multishot(
//     const int fd_slot,
//     const unsigned int offset,
//     const unsigned int buf_group,
//     const unsigned long user_data,
//     const unsigned int flags
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_READ_MULTISHOT;
//     sqe->fd = fd_slot;
//     sqe->off = offset;
//     sqe->buf_group = buf_group;
//     sqe->user_data = user_data;
//     sqe->flags = flags | IOSQE_BUFFER_SELECT | IOSQE_FIXED_FILE;
// }
//
// // void Ring::submit_read(
// //     const int fd,
// //     void* buffer,
// //     const size_t length,
// //     const unsigned long user_data,
// //     const unsigned int flags
// // ) const {
// //     const auto sqe = get_sqe();
// //     sqe->opcode = IORING_OP_READ;
// //     sqe->fd = fd;
// //     sqe->addr = reinterpret_cast<unsigned long>(buffer);
// //     sqe->len = static_cast<unsigned int>(length);
// //     sqe->user_data = user_data;
// //     sqe->flags = flags;
// // }
//
//
// void Ring::submit_recv_zc_multishot(
//     const int fd_slot,
//     const int zcrx_index,
//     const unsigned long user_data,
//     const unsigned int flags
// ) const {
//     const auto sqe = get_sqe();
//     sqe->opcode = IORING_OP_RECV_ZC;
//     sqe->fd = fd_slot;
//     sqe->ioprio = IORING_RECVSEND_FIXED_BUF;
//     sqe->zcrx_ifq_idx = zcrx_index;
//     sqe->ioprio = IORING_RECV_MULTISHOT;
//     sqe->user_data = user_data;
//     sqe->flags = flags | IOSQE_FIXED_FILE;
// }

//
// void Ring::zc_send() const {
//
// }
