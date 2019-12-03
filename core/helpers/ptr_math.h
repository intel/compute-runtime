/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>

static const int ptrGarbageContent[16] = {
    0x0131, 0x133, 0xA, 0xEF,
    0x0131, 0x133, 0xA, 0xEF,
    0x0131, 0x133, 0xA, 0xEF,
    0x0131, 0x133, 0xA, 0xEF};
static const auto ptrGarbage = (void *)ptrGarbageContent;

template <typename T>
inline T ptrOffset(T ptrBefore, size_t offset) {
    auto addrBefore = (uintptr_t)ptrBefore;
    auto addrAfter = addrBefore + offset;
    return (T)addrAfter;
}

template <>
inline uint64_t ptrOffset(uint64_t ptrBefore, size_t offset) {
    return ptrBefore + offset;
}

template <typename TA, typename TB>
inline size_t ptrDiff(TA ptrAfter, TB ptrBefore) {
    auto addrBefore = (uintptr_t)ptrBefore;
    auto addrAfter = (uintptr_t)ptrAfter;
    return addrAfter - addrBefore;
}

template <typename T>
inline uint64_t ptrDiff(uint64_t ptrAfter, T ptrBefore) {
    return ptrAfter - ptrBefore;
}

template <typename IntegerAddressType>
inline void *addrToPtr(IntegerAddressType addr) {
    uintptr_t correctBitnessAddress = static_cast<uintptr_t>(addr);
    void *ptrReturn = reinterpret_cast<void *>(correctBitnessAddress);
    return ptrReturn;
}

struct PatchStoreOperation {
    template <typename T>
    void operator()(T *memory, T value) {
        *memory = value;
    }
};

struct PatchIncrementOperation {
    template <typename T>
    void operator()(T *memory, T value) {
        *memory += value;
    }
};

template <typename PatchOperationT = PatchStoreOperation>
inline void patchWithRequiredSize(void *memoryToBePatched, uint32_t patchSize, uint64_t patchValue) {
    if (patchSize == sizeof(uint64_t)) {
        uint64_t *curbeAddress = reinterpret_cast<uint64_t *>(memoryToBePatched);
        PatchOperationT{}(curbeAddress, patchValue);
    } else {
        uint32_t *curbeAddress = reinterpret_cast<uint32_t *>(memoryToBePatched);
        PatchOperationT{}(curbeAddress, static_cast<uint32_t>(patchValue));
    }
}

inline void patchIncrement(void *memoryToBePatched, uint32_t patchSize, uint64_t patchIncrementValue) {
    patchWithRequiredSize<PatchIncrementOperation>(memoryToBePatched, patchSize, patchIncrementValue);
}

inline uint64_t castToUint64(const void *address) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(const_cast<void *>(address)));
}
