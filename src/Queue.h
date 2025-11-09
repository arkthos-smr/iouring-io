#pragma once


class ClientEntry {
    char* data;
    int buf_index;
public:
    ClientEntry() : data(nullptr), buf_index(-1) {}
    ClientEntry(char* data, const int buf_index) : data(data), buf_index(buf_index) {}
    [[nodiscard]] char* get_data() const { return data; }
    [[nodiscard]] int get_buf_index() const { return buf_index; }
};

template <unsigned int capacity>
class ClientQueue {
public:
    explicit ClientQueue() {
        data.fill(ClientEntry { nullptr, -1 });
    }

    void push(char* entry_data, const int buf_index) {
        const auto current_head = head.load(std::memory_order_relaxed);
        if (current_head - tail.load(std::memory_order_acquire) >= capacity) {
            throw std::runtime_error("Queue is full!");
        }
        data[current_head % capacity] = ClientEntry { entry_data, buf_index };
        head.store(current_head + 1, std::memory_order_release);
    }

    ClientEntry pop() {
        while (true) {
            auto tail_index = tail.load(std::memory_order_acquire);
            if (tail_index == head.load(std::memory_order_acquire)) return ClientEntry {};

            const auto slot = tail_index % capacity;
            const ClientEntry result = data[slot];
            if (tail.compare_exchange_weak(tail_index, tail_index + 1, std::memory_order_acq_rel)) {
                return result;
            }
        }
    }

private:
    std::atomic<size_t> head { 0 };
    std::atomic<size_t> tail { 0 };
    std::array<ClientEntry, capacity> data;
};
