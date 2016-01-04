#ifndef COMM_SYNC_H_
#define COMM_SYNC_H_

#include "utils/threads.h"

class CommSync {
public:
    explicit CommSync(const uint32_t threadCount)
        : threadCount_(threadCount), bar_(threadCount)
    {
        // Nothing else to do.
    }

    /**
     * Synchronization barrier.
     */
    void barrier() {
        bar_.wait();
    }

private:
    const uint32_t threadCount_;
    bar_t bar_;

};

#endif // COMM_SYNC_H_
