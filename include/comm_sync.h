#ifndef COMM_SYNC_H_
#define COMM_SYNC_H_

#include "utils/stream.h"
#include "utils/threads.h"

namespace GraphGASLite {

template<typename KType, typename VType>
class CommSync {
public:
    typedef KType KeyType;
    typedef VType ValType;

    /**
     * The key-value pair type used to communicate between threads.
     */
    class KeyValue {
    public:
        KeyValue(const KeyType& key, const ValType& val)
            : key_(key), val_(val)
        {
            // Nothing else to do.
        }

        KeyValue() : key_(), val_() { }

        const KeyType& key() const { return key_; }

        const ValType& val() const { return val_; }

    private:
        KeyType key_;
        ValType val_;
    };

    enum RecvStatusType {
        // Nothing is received.
        RECV_NONE,
        // Some data is received, but not finished.
        RECV_CONTINUED,
        // All data is received.
        RECV_FINISHED,
    };

    typedef Stream<KeyValue> KeyValueStream;

public:
    explicit CommSync(const uint32_t threadCount, const KeyValue& endTag);

    ~CommSync();

    /**
     * Number of threads.
     */
    uint32_t threadCount() const { return threadCount_; }

    /**
     * Thread register.
     */
    void threadIdIs(const uint32_t threadId);

    /**
     * Synchronization barrier.
     */
    void barrier(const uint32_t threadId);

    /**
     * Synchronization barrier, also do an AND reduction.
     */
    bool barrierAND(const uint32_t threadId, bool input);

    /**
     * Send a key-value pair from \c prodId to \c consId.
     */
    void keyValNew(const uint32_t prodId, const uint32_t consId,
            const KeyType& key, const ValType& val);

    /**
     * Send end-of-message tag from \c prodId to \c consId.
     */
    void endTagNew(const uint32_t prodId, const uint32_t consId);

    /**
     * Delete all key-value pairs associated with \c prodId at producer side
     * after communication is done.
     */
    void keyValProdDelAll(const uint32_t prodId);

    /**
     * Delete all key-value pairs associated with \c consId at consumer side
     * after communication is done.
     */
    void keyValConsDelAll(const uint32_t consId);

    /**
     * Receive all key-value pairs available now, and partition them into subpartitions.
     *
     * @param consId            Thread index of the consumer.
     * @param partitionCount    Number of subpartitions.
     * @param partitionFunc     Partition function. For a key \c k, partition index
     *                          is calculated as <tt>partitionFunc(k) % partitionCount</tt>.
     *
     * @return      A pair consisting of the subpartitions, and the receiving status.
     */
    std::pair<std::vector<KeyValueStream>, RecvStatusType> keyValPartitions(
            const uint32_t consId, const size_t partitionCount,
            std::function<size_t(const KeyType&)> partitionFunc);

private:
    const uint32_t threadCount_;

    /* Synchronization. */

    // Barrier.
    bar_t bar_;

    // Used for barrierAND.
    bool barANDCurReduction_;
    bool barANDLastResult_;

    /* Communication. */

    /**
     * End-of-message tag for communication.
     */
    const KeyValue endTag_;

    /**
     * Each producer thread is associated with multiple streams, each of which
     * is for a consumer thread.
     *
     * Indexed by [prodId][consId].
     */
    std::vector<std::vector<KeyValueStream>> streamLists_;

};



template<typename KType, typename VType>
CommSync<KType, VType>::
CommSync(const uint32_t threadCount, const KeyValue& endTag)
    : threadCount_(threadCount),
      bar_(threadCount), barANDCurReduction_(true), barANDLastResult_(false),
      endTag_(endTag)
{
    // Initialize communication streams.
    streamLists_.resize(threadCount_);
    for (auto& sl : streamLists_) {
        sl.resize(threadCount_);
    }
}

template<typename KType, typename VType>
CommSync<KType, VType>::
~CommSync() {
    // Nothing to do.
}

template<typename KType, typename VType>
void CommSync<KType, VType>::
threadIdIs(const uint32_t) {
    // Nothing to do.
}

template<typename KType, typename VType>
void CommSync<KType, VType>::
barrier(const uint32_t) {
    bar_.wait();
}

template<typename KType, typename VType>
bool CommSync<KType, VType>::
barrierAND(const uint32_t, bool input) {
    barANDCurReduction_ &= input;
    auto scb = [this](){
        barANDLastResult_ = barANDCurReduction_;
        barANDCurReduction_ = true;
    };
    bar_.wait(scb);
    return barANDLastResult_;
}

template<typename KType, typename VType>
void CommSync<KType, VType>::
keyValNew(const uint32_t prodId, const uint32_t consId,
        const KeyType& key, const ValType& val) {
    streamLists_[prodId][consId].put(KeyValue(key, val));
}

template<typename KType, typename VType>
void CommSync<KType, VType>::
endTagNew(const uint32_t prodId, const uint32_t consId) {
    // Nothing to do.
}

template<typename KType, typename VType>
void CommSync<KType, VType>::
keyValProdDelAll(const uint32_t prodId) {
    for (auto& s : streamLists_[prodId]) {
        s.reset();
    }
}

template<typename KType, typename VType>
void CommSync<KType, VType>::
keyValConsDelAll(const uint32_t consId) {
    for (auto& sl : streamLists_) {
        sl[consId].reset();
    }
}

template<typename KType, typename VType>
std::pair<std::vector<typename CommSync<KType, VType>::KeyValueStream>,
    typename CommSync<KType, VType>::RecvStatusType> CommSync<KType, VType>::
keyValPartitions(const uint32_t consId, const size_t partitionCount,
        std::function<size_t(const KeyType&)> partitionFunc) {

    std::vector<KeyValueStream> prtns(partitionCount);

    // Take barrier to ensure all threads have finished sending all data.
    barrier(consId);

    // Local stream.
    if (partitionCount == 1) {
        prtns[0].swap(streamLists_[consId][consId]);
    } else {
        for (const auto& kv : streamLists_[consId][consId]) {
            auto pid = partitionFunc(kv.key()) % partitionCount;
            prtns[pid].put(kv);
        }
    }

    // Remote streams.
    for (uint32_t prodId = 0; prodId < threadCount_; prodId++) {
        // Skip local stream.
        if (prodId == consId) continue;

        for (const auto& kv : streamLists_[prodId][consId]) {
            auto pid = partitionFunc(kv.key()) % partitionCount;
            prtns[pid].put(kv);
        }
    }

    return std::make_pair(std::move(prtns), RECV_FINISHED);
}

} // namespace GraphGASLite

#endif // COMM_SYNC_H_
