/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"

struct PatchStoreOperation {
    template <typename T>
    void operator()(T *memory, T value) {
        *memory = value;
    }
};

inline void patchWithRequiredSize(void *memoryToBePatched, uint32_t patchSize, uint64_t patchValue) {
    if (patchSize == sizeof(uint64_t)) {
        if (isAligned<sizeof(uint64_t)>(memoryToBePatched)) {
            uint64_t *curbeAddress = reinterpret_cast<uint64_t *>(memoryToBePatched);
            PatchStoreOperation{}(curbeAddress, patchValue);
        } else {
            memcpy_s(memoryToBePatched, patchSize, &patchValue, sizeof(patchValue));
        }
    } else if (patchSize == sizeof(uint32_t)) {
        uint32_t *curbeAddress = reinterpret_cast<uint32_t *>(memoryToBePatched);
        PatchStoreOperation{}(curbeAddress, static_cast<uint32_t>(patchValue));
    } else {
        UNRECOVERABLE_IF(patchSize != 0);
    }
}