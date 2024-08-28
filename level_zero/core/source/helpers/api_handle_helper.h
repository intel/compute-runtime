/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

constexpr uint64_t objMagicValue = 0x8D7E6A5D4B3E2E1FULL;

template <typename T>
inline T toInternalType(T input) {
    if (!input || input->objMagic == objMagicValue) {
        return input;
    }
    input = *reinterpret_cast<T *>(input);
    if (input->objMagic == objMagicValue) {
        return input;
    }
    return nullptr;
}
