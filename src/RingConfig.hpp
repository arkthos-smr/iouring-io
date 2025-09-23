#pragma once

#include <chrono>

/**
 * @enum RingIOType
 * @brief Specifies the io_uring ring type.
 */
enum class RingIOType {
    INTERRUPT, /**< Interrupt driven ring */
    SQPOLL,    /**< Submission queue polling */
    IOPOLL     /**< I/O polling */
};

/**
 * @class RingConfig
 * @brief Configuration for the io_uring ring setup.
 *
 * This class encapsulates parameters for configuring
 * an io_uring instance, such as ring type, size,
 * CPU pinning, and submission behavior.
 */
class RingConfig {
    RingIOType type;                            /**< The io_uring ring type */
    unsigned int size;                          /**< Number of entries in the ring */
    int pin;                                    /**< CPU core to pin the sqpoll thread to, -1 means no pinning */
    bool submit_all;                            /**< Whether to submit all queued operations at once */
    std::chrono::microseconds sq_thread_idle;   /**< Idle time for sqpoll thread in microseconds */

public:
    /**
     * @brief Constructs a RingConfig with specified parameters.
     * @param type The io_uring ring type.
     * @param size Number of entries in the ring.
     * @param pin CPU core to pin the sqpoll thread to (-1 disables pinning).
     * @param submit_all Whether to submit all queued operations at once.
     * @param sq_thread_idle Idle time for sqpoll thread (default 1 second).
     */
    explicit RingConfig(
        RingIOType type,
        unsigned int size,
        int pin,
        bool submit_all,
        std::chrono::microseconds sq_thread_idle = std::chrono::microseconds(1000000)
    );

    /**
     * @brief Returns the io_uring ring type.
     * @return RingIOType enum value.
     */
    [[nodiscard]] RingIOType get_type() const;

    /**
     * @brief Returns the size (number of entries) of the ring.
     * @return Number of entries as unsigned int.
     */
    [[nodiscard]] unsigned int get_size() const;

    /**
     * @brief Returns the CPU core pin for the sqpoll thread.
     * @return CPU core index or -1 if pinning is disabled.
     */
    [[nodiscard]] int get_pin() const;

    /**
     * @brief Indicates if all queued operations should be submitted at once.
     * @return True if submit all is enabled, false otherwise.
     */
    [[nodiscard]] bool get_submit_all() const;

    /**
     * @brief Returns the idle time of the sqpoll thread in microseconds.
     * @return Idle time as std::chrono::microseconds.
     */
    [[nodiscard]] std::chrono::microseconds get_sq_thread_idle() const;
};