#ifndef UTILS_BARRIER_H_
#define UTILS_BARRIER_H_
/**
 * Wrappers for pthread barrier.
 */
#include <pthread.h>
#include "log.h"

typedef pthread_barrier_t bar_t;

inline void bar_init(bar_t& bar, unsigned count) {
    int ret = pthread_barrier_init(&bar, NULL, count);
    if (ret != 0) panic("pthread_barrier_init fails: %d.", ret);
}

inline void bar_destroy(bar_t& bar) {
    int ret = pthread_barrier_destroy(&bar);
    if (ret != 0) panic("pthread_barrier_destroy fails: %d.", ret);
}

inline int bar_wait(bar_t& bar) {
    int ret = pthread_barrier_wait(&bar);
    if (ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
        panic("pthread_barrier_wait fails: %d.", ret);
    }
    return ret == PTHREAD_BARRIER_SERIAL_THREAD ? 1 : 0;
}

#endif // UTILS_BARRIER_H_

