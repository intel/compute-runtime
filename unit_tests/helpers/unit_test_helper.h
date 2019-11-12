/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/helpers/hw_cmds.h"
#include "runtime/helpers/properties_helper.h"

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

    static uint32_t getAppropriateThreadArbitrationPolicy(uint32_t policy);

    static bool evaluateGshAddressForScratchSpace(uint64_t usedScratchGpuAddress, uint64_t retrievedGshAddress);

    static bool isPipeControlWArequired(const HardwareInfo &hwInfo);

    static uint64_t getMemoryAddress(const typename GfxFamily::MI_ATOMIC &atomic);

    static const bool tiledImagesSupported;

    static const uint32_t smallestTestableSimdSize;

    static const AuxTranslationMode requiredAuxTranslationMode;
};
} // namespace NEO
