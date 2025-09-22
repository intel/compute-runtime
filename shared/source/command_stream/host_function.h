/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace NEO {

class LinearStream;

struct HostFunctionData {
    volatile uint64_t *entry = nullptr;
    volatile uint64_t *userData = nullptr;
    volatile uint32_t *internalTag = nullptr;
};

enum class HostFunctionTagStatus : uint32_t {
    completed = 0,
    pending = 1
};

struct HostFunctionHelper {

    constexpr static size_t entryOffset = offsetof(HostFunctionData, entry);
    constexpr static size_t userDataOffset = offsetof(HostFunctionData, userData);
    constexpr static size_t internalTagOffset = offsetof(HostFunctionData, internalTag);

    template <typename GfxFamily>
    static void programHostFunction(LinearStream &commandStream, const HostFunctionData &hostFunctionData, uint64_t userHostFunctionAddress, uint64_t userDataAddress);

    template <typename GfxFamily>
    static void programHostFunctionAddress(LinearStream *commandStream, void *cmdBuffer, const HostFunctionData &hostFunctionData, uint64_t userHostFunctionAddress);

    template <typename GfxFamily>
    static void programHostFunctionUserData(LinearStream *commandStream, void *cmdBuffer, const HostFunctionData &hostFunctionData, uint64_t userDataAddress);

    template <typename GfxFamily>
    static void programSignalHostFunctionStart(LinearStream *commandStream, void *cmdBuffer, const HostFunctionData &hostFunctionData);

    template <typename GfxFamily>
    static void programWaitForHostFunctionCompletion(LinearStream *commandStream, void *cmdBuffer, const HostFunctionData &hostFunctionData);
};

} // namespace NEO
