/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aux_translation.h"
#include "shared/source/helpers/hw_cmds.h"

#include "opencl/source/helpers/properties_helper.h"

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

    static bool isSynchronizationWArequired(const HardwareInfo &hwInfo);

    static bool isPipeControlWArequired(const HardwareInfo &hwInfo);

    static bool isAdditionalMiSemaphoreWaitRequired(const HardwareInfo &hwInfo);

    static bool isAdditionalMiSemaphoreWait(const typename GfxFamily::MI_SEMAPHORE_WAIT &semaphoreWait);

    static uint64_t getMemoryAddress(const typename GfxFamily::MI_ATOMIC &atomic);

    static const bool tiledImagesSupported;

    static const uint32_t smallestTestableSimdSize;

    static const AuxTranslationMode requiredAuxTranslationMode;

    static const bool useFullRowForLocalIdsGeneration;
};

} // namespace NEO
