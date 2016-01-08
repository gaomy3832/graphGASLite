#ifndef COMM_SYNC_H_
#define COMM_SYNC_H_

#include "utils/stream.h"
#include "utils/threads.h"

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
    explicit CommSync(const uint32_t threadCount)
        : threadCount_(threadCount), bar_(threadCount),
          barANDCurReduction_(true), barANDLastResult_(false)
    {
        // Nothing else to do.
    }

    /**
     * Number of threads.
     */
    uint32_t threadCount() const { return threadCount_; }

    /**
     * Synchronization barrier.
     */
    void barrier() {
        bar_.wait();
    }

    /**
     * Synchronization barrier, also do an AND reduction.
     */
    bool barrierAND(bool input) {
        barANDCurReduction_ &= input;
        auto scb = [this](){
            barANDLastResult_ = barANDCurReduction_;
            barANDCurReduction_ = true;
        };
        bar_.wait(scb);
        return barANDLastResult_;
    }

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
    bar_t bar_;

    // Used for barrierAND.
    bool barANDCurReduction_;
    bool barANDLastResult_;

};

#endif // COMM_SYNC_H_
