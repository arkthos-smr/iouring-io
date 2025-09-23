#include "RingConfig.hpp"

RingConfig::RingConfig(
    const RingIOType type,
    const unsigned int size,
    const int pin,
    const bool submit_all,
    const std::chrono::microseconds sq_thread_idle
) : type(type), size(size), pin(pin), submit_all(submit_all), sq_thread_idle(sq_thread_idle) {}

RingIOType RingConfig::get_type() const { return type; }

unsigned int RingConfig::get_size() const { return size; }

int RingConfig::get_pin() const { return pin; }

bool RingConfig::get_submit_all() const { return submit_all; }

std::chrono::microseconds RingConfig::get_sq_thread_idle() const { return sq_thread_idle; }