#pragma once

template<size_t Capacity>
struct spsc_cursor_base {
    static constexpr size_t capacity = Capacity;
    static constexpr size_t mask     = Capacity - 1;
};

template<size_t Capacity>
struct spsc_cursor : public spsc_cursor_base<Capacity> {
    size_t m_head = 0;
    size_t m_tail = 0;

    inline size_t size() const {
        return m_tail - m_head; 
    }

    inline size_t head_index() const {
        return m_head & this->mask;
    }

    inline size_t tail_index() const {
        return m_tail & this->mask;
    }

    inline bool empty() const {
        return m_head == m_tail;
    }

    inline size_t available() const {
        return this->capacity - size();
    }

    inline size_t load_head() const {
        return m_head & this->mask;
    }

    inline size_t load_tail() const {
        return m_tail & this->mask;
    }

    inline size_t load_raw_tail() const {
        return m_tail;
    }

    inline void wait_for_not_empty() const {
        while (empty()) {
            // busy wait
            // 也可以加 std::this_thread::yield();
        }
    }

    inline void push(size_t num = 1) {
        m_tail += num;
    }

    inline void pop(size_t num = 1) {
        m_head += num;
    }

    inline void push_notify(size_t num = 1) {
        push(num);
    }

    inline void pop_notify(size_t num = 1) {
        pop(num);
    }
};


template<size_t Capacity>
struct spsc_cursor_ts : public spsc_cursor_base<Capacity> {
    std::atomic<size_t> m_head{0};
    std::atomic<size_t> m_tail{0};

    inline size_t size() const {
        return m_tail.load(std::memory_order_acquire)
             - m_head.load(std::memory_order_acquire);
    }

    inline size_t head_index() const {
        return m_head.load(std::memory_order_relaxed) & this->mask;
    }

    inline size_t tail_index() const {
        return m_tail.load(std::memory_order_relaxed) & this->mask;
    }

    inline bool empty() const {
        return m_head.load(std::memory_order_acquire) 
            == m_tail.load(std::memory_order_acquire);
    }

    inline size_t available() const {
        return this->capacity - size();
    }

    inline size_t load_head() const {
        return m_head.load(std::memory_order_acquire) & this->mask;
    }

    inline size_t load_tail() const {
        return m_tail.load(std::memory_order_acquire) & this->mask;
    }

    inline size_t load_raw_tail() const {
        return m_tail.load(std::memory_order_relaxed);
    }

    inline void wait_for_not_empty() const {
        // C++20 中有 atomic.wait / notify 概念
        auto old = m_head.load(std::memory_order_relaxed);
        while (empty()) {
            // 自旋或使用 wait
            m_head.wait(old, std::memory_order_relaxed);
            old = m_head.load(std::memory_order_relaxed);
        }
    }

    inline void push(size_t num = 1) {
        auto old = m_tail.load(std::memory_order_relaxed);
        m_tail.store(old + num, std::memory_order_release);
    }

    inline void pop(size_t num = 1) {
        auto old = m_head.load(std::memory_order_relaxed);
        m_head.store(old + num, std::memory_order_release);
    }

    inline void push_notify(size_t num = 1) {
        push(num);
        m_head.notify_one();
    }

    inline void pop_notify(size_t num = 1) {
        pop(num);
        m_head.notify_one();
    }
};