/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>

inline const int ptrGarbageContent[16] = {
    0x0131, 0x133, 0xA, 0xEF,
    0x0131, 0x133, 0xA, 0xEF,
    0x0131, 0x133, 0xA, 0xEF,
    0x0131, 0x133, 0xA, 0xEF};
inline const auto ptrGarbage = (void *)ptrGarbageContent;

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

template <typename T, typename P>
constexpr bool byteRangeContains(T rangeBase, size_t rangeSizeInBytes, P ptr) noexcept {
    const auto begin = reinterpret_cast<uintptr_t>(rangeBase);
    const auto end = begin + rangeSizeInBytes;
    const auto p = reinterpret_cast<uintptr_t>(ptr);

    return (p >= begin) && (p < end);
}

template <typename IntegerAddressType>
inline void *addrToPtr(IntegerAddressType addr) {
    uintptr_t correctBitnessAddress = static_cast<uintptr_t>(addr);
    void *ptrReturn = reinterpret_cast<void *>(correctBitnessAddress);
    return ptrReturn;
}

inline uint64_t castToUint64(const void *address) {
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(const_cast<void *>(address)));
}

inline uint32_t getLowPart(uint64_t value) {
    return static_cast<uint32_t>(value);
}

inline uint32_t getHighPart(uint64_t value) {
    return static_cast<uint32_t>(value >> 32);
}