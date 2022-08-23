/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace NEO {
class GmmHelper;
class GraphicsAllocation;

struct EncodeSurfaceStateArgsBase {
    uint64_t graphicsAddress = 0ull;
    size_t size = 0u;

    void *outMemory = nullptr;

    GraphicsAllocation *allocation = nullptr;
    GmmHelper *gmmHelper = nullptr;

    uint32_t numAvailableDevices = 0u;
    uint32_t mocs = 0u;

    bool cpuCoherent = false;
    bool forceNonAuxMode = false;
    bool isReadOnly = false;
    bool useGlobalAtomics = false;
    bool areMultipleSubDevicesInContext = false;
    bool implicitScaling = false;
    bool isDebuggerActive = false;

  protected:
    EncodeSurfaceStateArgsBase() = default;
};
} // namespace NEO
