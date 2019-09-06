/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "hw_cmds.h"

namespace NEO {

class Kernel;
struct HardwareInfo;

template <typename GfxFamily>
struct UnitTestHelper {
    static bool isL3ConfigProgrammable();

    static bool evaluateDshUsage(size_t sizeBeforeEnqueue, size_t sizeAfterEnqueue, Kernel *kernel);

    static bool isPageTableManagerSupported(const HardwareInfo &hwInfo);

    static bool isTimestampPacketWriteSupported();

    static bool isExpectMemoryNotEqualSupported();

    static uint32_t getDefaultSshUsage();

    static bool evaluateGshAddressForScratchSpace(uint64_t usedScratchGpuAddress, uint64_t retrievedGshAddress);

    static bool isPipeControlWArequired(const HardwareInfo &hwInfo);

    static uint64_t getMemoryAddress(const typename GfxFamily::MI_ATOMIC &atomic);

    static const bool tiledImagesSupported;
};
} // namespace NEO
