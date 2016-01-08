#ifndef COMM_SYNC_H_
#define COMM_SYNC_H_

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

public:
    explicit CommSync(const uint32_t threadCount)
        : threadCount_(threadCount), bar_(threadCount),
          barANDCurReduction_(true), barANDLastResult_(false)
    {
        // Nothing else to do.
    }

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

private:
    const uint32_t threadCount_;
    bar_t bar_;

    // Used for barrierAND.
    bool barANDCurReduction_;
    bool barANDLastResult_;

};

#endif // COMM_SYNC_H_
