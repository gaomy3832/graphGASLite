#ifndef COMMON_H_
#define COMMON_H_

#include <string>
#include "utils/exception.h"

using std::string;

template<typename UnitType, typename RepType>
struct IndexType {
public:
    IndexType(const UnitType& idx) : idx_(idx) {}
    operator UnitType() const { return idx_; }
    typedef UnitType Type;
private:
    UnitType idx_;
};

#endif // COMMON_H_
