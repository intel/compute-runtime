/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aux_translation.h"

#include "hw_cmds.h"

namespace NEO {

struct KernelDescriptor;
struct HardwareInfo;

template <typename GfxFamily>
struct UnitTestHelper {
    using COHERENCY_TYPE = typename GfxFamily::RENDER_SURFACE_STATE::COHERENCY_TYPE;

    static bool isL3ConfigProgrammable();

    static bool evaluateDshUsage(size_t sizeBeforeEnqueue, size_t sizeAfterEnqueue, const KernelDescriptor *kernelDescriptor, uint32_t rootDeviceIndex);

    static bool isPageTableManagerSupported(const HardwareInfo &hwInfo);

    static bool isTimestampPacketWriteSupported();

    static bool isExpectMemoryNotEqualSupported();

    static uint32_t getDefaultSshUsage();

    static uint32_t getAppropriateThreadArbitrationPolicy(uint32_t policy);

    static auto getCoherencyTypeSupported(COHERENCY_TYPE coherencyType) -> decltype(coherencyType);

    static bool evaluateGshAddressForScratchSpace(uint64_t usedScratchGpuAddress, uint64_t retrievedGshAddress);

    static bool isPipeControlWArequired(const HardwareInfo &hwInfo);

    static bool isAdditionalSynchronizationRequired();

    static bool isAdditionalMiSemaphoreWaitRequired(const HardwareInfo &hwInfo);

    static bool isAdditionalMiSemaphoreWait(const typename GfxFamily::MI_SEMAPHORE_WAIT &semaphoreWait);

    static uint64_t getAtomicMemoryAddress(const typename GfxFamily::MI_ATOMIC &atomic);

    static bool requiresTimestampPacketsInSystemMemory();

    static const bool tiledImagesSupported;

    static const uint32_t smallestTestableSimdSize;

    static const AuxTranslationMode requiredAuxTranslationMode;

    static const bool useFullRowForLocalIdsGeneration;

    static const bool additionalMiFlushDwRequired;
};

} // namespace NEO
