/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/utilities/stackvec.h"

#include <cstdint>
#include <limits>

namespace NEO {

static const size_t maxFlushSubrangeCount = 126;
struct L3Range {
    static constexpr uint64_t minAlignment = MemoryConstants::pageSize;
    static constexpr uint64_t minAlignmentMask = minAlignment - 1ULL;
    static constexpr uint64_t minAlignmentBitOffset = Math::ffs(minAlignment);
    static constexpr uint64_t maxSingleRange = 4 * MemoryConstants::gigaByte;
    static constexpr uint64_t maxMaskValue = Math::ffs(maxSingleRange / minAlignment);
    static const uint64_t policySize = 2;

    L3Range() = default;

    uint64_t getMask() const {
        return data.common.mask;
    }

    void setMask(uint64_t mask) {
        data.common.mask = mask;
    }

    uint64_t getAddress() const {
        return data.common.address << L3Range::minAlignmentBitOffset;
    }

    void setAddress(uint64_t address) {
        data.common.address = address >> L3Range::minAlignmentBitOffset;
    }
    void setPolicy(uint64_t policy) {
        data.common.policy = policy;
    }
    uint64_t getPolicy() const {
        return data.common.policy;
    }

    static constexpr bool meetsMinimumAlignment(uint64_t v) {
        return (0 == (v & minAlignmentMask));
    }

    static uint32_t getMaskFromSize(uint64_t size) {
        UNRECOVERABLE_IF(false == Math::isPow2(size));
        UNRECOVERABLE_IF((size < minAlignment) || (size > maxSingleRange));
        auto ret = Math::ffs(size >> minAlignmentBitOffset);

        static_assert(maxMaskValue < std::numeric_limits<uint32_t>::max(), "");
        return static_cast<uint32_t>(ret);
    }

    uint64_t getSizeInBytes() const {
        return (1ULL << (minAlignmentBitOffset + getMask()));
    }

    uint64_t getMaskedAddress() const {
        return getAddress() & (~maxNBitValue(minAlignmentBitOffset + getMask()));
    }

    static L3Range fromAddressSize(uint64_t address, uint64_t size) {
        L3Range ret{};
        ret.setAddress(address);
        ret.setMask(getMaskFromSize(size));
        return ret;
    }

    static L3Range fromAddressSizeWithPolicy(uint64_t address, uint64_t size, uint64_t policy) {
        L3Range ret = fromAddressSize(address, size);
        ret.setPolicy(policy);
        return ret;
    }
    static L3Range fromAddressMask(uint64_t address, uint64_t mask) {
        L3Range ret{};
        ret.setAddress(address);
        ret.setMask(mask);
        return ret;
    }

  protected:
    union Data {
        struct {
            uint64_t mask : minAlignmentBitOffset;
            uint64_t address : sizeof(uint64_t) * 8 - minAlignmentBitOffset - policySize;
            uint64_t policy : policySize;
        } common;
        uint64_t raw;
    } data;
    static_assert(sizeof(Data) == sizeof(uint64_t), "");
};

inline bool operator==(const L3Range &lhs, const L3Range &rhs) {
    return (lhs.getAddress() == rhs.getAddress()) && (lhs.getMask() == rhs.getMask());
}

inline bool operator!=(const L3Range &lhs, const L3Range &rhs) {
    return (false == (lhs == rhs));
}

template <typename ContainerT>
inline void coverRangeExactImpl(uint64_t address, uint64_t size, ContainerT &ret, uint64_t policy) {
    UNRECOVERABLE_IF(false == L3Range::meetsMinimumAlignment(address));
    UNRECOVERABLE_IF(false == L3Range::meetsMinimumAlignment(size));

    const uint64_t end = address + size;

    uint64_t offset = address;
    while (offset < end) {
        uint64_t maxRangeSizeBySize = Math::prevPowerOfTwo(end - offset);
        uint64_t maxRangeSizeByOffset = offset ? (1ULL << Math::ffs(offset)) : L3Range::maxSingleRange;
        uint64_t rangeSize = std::min(maxRangeSizeBySize, maxRangeSizeByOffset);
        rangeSize = std::min(rangeSize, +L3Range::maxSingleRange);
        ret.push_back(L3Range::fromAddressSizeWithPolicy(offset, rangeSize, policy));
        offset += rangeSize;
    }
}

using L3RangesVec = StackVec<L3Range, 32>;

template <typename RetVecT>
inline void coverRangeExact(uint64_t address, uint64_t size, RetVecT &ret, uint64_t policy) {
    coverRangeExactImpl(address, size, ret, policy);
}

} // namespace NEO
