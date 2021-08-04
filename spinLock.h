#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <atomic>

class spinlock_mutex {
private:
    std::atomic_flag flag;
public:
    spinlock_mutex() : flag(ATOMIC_FLAG_INIT) {}

    void lock() {
        while (flag.test_and_set(std::memory_order_acquire));
    }

    void unlock() {
        flag.clear(std::memory_order_release);
    }
};

#endif //SPINLOCK_H
