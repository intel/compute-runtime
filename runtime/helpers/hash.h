/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "common/compiler_support.h"
#include "runtime/helpers/aligned_memory.h"

#include <cstdint>

namespace OCLRT {
// clang-format off
#define HASH_JENKINS_MIX(a,b,c) \
{ \
    a -= b; a -= c; a ^= (c>>13); \
    b -= c; b -= a; b ^= (a<<8);  \
    c -= a; c -= b; c ^= (b>>13); \
    a -= b; a -= c; a ^= (c>>12); \
    b -= c; b -= a; b ^= (a<<16); \
    c -= a; c -= b; c ^= (b>>5);  \
    a -= b; a -= c; a ^= (c>>3);  \
    b -= c; b -= a; b ^= (a<<10); \
    c -= a; c -= b; c ^= (b>>15); \
}
// clang-format on
class Hash {
  public:
    Hash() {
        reset();
    };

    uint32_t getValue(const char *data, size_t size) {
        uint32_t value = 0;
        switch (size) {
        case 3:
            value = static_cast<uint32_t>(*reinterpret_cast<const unsigned char *>(data++));
            value <<= 8;
            CPP_ATTRIBUTE_FALLTHROUGH;
        case 2:
            value |= static_cast<uint32_t>(*reinterpret_cast<const unsigned char *>(data++));
            value <<= 8;
            CPP_ATTRIBUTE_FALLTHROUGH;
        case 1:
            value |= static_cast<uint32_t>(*reinterpret_cast<const unsigned char *>(data++));
            value <<= 8;
        }
        return value;
    }

    void update(const char *buff, size_t size) {
        if (buff == nullptr)
            return;

        if ((reinterpret_cast<uintptr_t>(buff) & 0x3) != 0) {
            const unsigned char *tmp = (const unsigned char *)buff;

            while (size >= sizeof(uint32_t)) {
                uint32_t value = (uint32_t)tmp[0] + (((uint32_t)tmp[1]) << 8) + ((uint32_t)tmp[2] << 16) + ((uint32_t)tmp[3] << 24);
                a ^= value;
                HASH_JENKINS_MIX(a, hi, lo);
                size -= sizeof(uint32_t);
                tmp += sizeof(uint32_t);
            }
            if (size > 0) {
                uint32_t value = getValue((char *)tmp, size);
                a ^= value;
                HASH_JENKINS_MIX(a, hi, lo);
            }
        } else {
            const uint32_t *tmp = reinterpret_cast<const uint32_t *>(buff);

            while (size >= sizeof(*tmp)) {
                a ^= *(tmp++);
                HASH_JENKINS_MIX(a, hi, lo);
                size -= sizeof(*tmp);
            }

            if (size > 0) {
                uint32_t value = getValue((char *)tmp, size);
                a ^= value;
                HASH_JENKINS_MIX(a, hi, lo);
            }
        }
    }

    uint64_t finish() {
        return (((uint64_t)hi) << 32) | lo;
    }

    void reset() {
        a = 0x428a2f98;
        hi = 0x71374491;
        lo = 0xb5c0fbcf;
    }

    static uint64_t hash(const char *buff, size_t size) {
        Hash hash;
        hash.update(buff, size);
        return hash.finish();
    }

  protected:
    uint32_t a, hi, lo;
};

template <typename T>
uint32_t hashPtrToU32(const T *src) {
    auto asInt = reinterpret_cast<uintptr_t>(src);
    constexpr auto m = sizeof(uintptr_t) / 8;
    asInt = asInt ^ ((asInt & ~(m - 1)) >> (m * 32));

    return static_cast<uint32_t>(asInt);
}
} // namespace OCLRT
