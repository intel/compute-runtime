/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace OCLRT {

template <typename T>
struct Range {
    Range(T *base, size_t count)
        : Beg(base), End(base + count) {
    }

    T *begin() {
        return Beg;
    }

    T *end() {
        return End;
    }

    T *Beg;
    T *End;
};

template <typename T>
Range<T> CreateRange(T *base, size_t count) {
    return Range<T>(base, count);
}
} // namespace OCLRT