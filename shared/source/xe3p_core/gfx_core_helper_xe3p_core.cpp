/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_base.h"
using Family = NEO::Xe3pCoreFamily;
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/gfx_core_helper_base.inl"
#include "shared/source/helpers/gfx_core_helper_dg2_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_pvc_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_tgllp_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xe2_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xe3_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xe3p_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xehp_and_later.inl"

#include "gfx_core_helper_xe3p_core_additional.inl"

namespace ContextGroup {
extern uint32_t maxContextCount;
}

namespace NEO {
template <>
const AuxTranslationMode GfxCoreHelperHw<Family>::defaultAuxTranslationMode = AuxTranslationMode::none;

template <>
uint32_t GfxCoreHelperHw<Family>::getContextGroupContextsCount() const {
    auto contextGroupCount = 64u;
    if (contextGroupCount > ContextGroup::maxContextCount) {
        contextGroupCount = ContextGroup::maxContextCount;
    }
    if (!secondaryContextsEnabled) {
        contextGroupCount = 0;
    }
    if (debugManager.flags.ContextGroupSize.get() != -1) {
        return debugManager.flags.ContextGroupSize.get();
    }
    return contextGroupCount;
}

template <>
uint32_t GfxCoreHelperHw<Family>::getMinimalSIMDSize() const {
    return 16u;
}

template <>
const StackVec<size_t, 3> GfxCoreHelperHw<Family>::getDeviceSubGroupSizes() const {
    return {16, 32};
}

template <>
uint32_t GfxCoreHelperHw<Family>::overrideMaxWorkGroupSize(uint32_t maxWG) const {
    return std::min(maxWG, 2048u);
}

template <>
uint32_t GfxCoreHelperHw<Family>::calculateNumThreadsPerThreadGroup(uint32_t simd, uint32_t totalWorkItems, uint32_t grfCount, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    uint32_t numThreadsPerThreadGroup = getThreadsPerWG(simd, totalWorkItems);
    if (debugManager.flags.RemoveRestrictionsOnNumberOfThreadsInGpgpuThreadGroup.get() == 1) {
        return numThreadsPerThreadGroup;
    }

    const auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    const auto &productHelper = rootDeviceEnvironment.getProductHelper();
    const auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto isHeaplessMode = compilerProductHelper.isHeaplessModeEnabled(hwInfo);

    uint32_t maxThreadsPerThreadGroup = 64u;
    if (grfCount == 512) {
        maxThreadsPerThreadGroup = 16u;
    } else if ((grfCount == 256) || (simd == 32u)) {
        // NEO-11881
        maxThreadsPerThreadGroup = 32u;
    } else if (grfCount == 192) {
        maxThreadsPerThreadGroup = 40u;
    } else if (grfCount == 160) {
        maxThreadsPerThreadGroup = 48u;
    }

    maxThreadsPerThreadGroup = productHelper.adjustMaxThreadsPerThreadGroup(maxThreadsPerThreadGroup, simd, grfCount, isHeaplessMode);

    numThreadsPerThreadGroup = std::min(numThreadsPerThreadGroup, maxThreadsPerThreadGroup);
    DEBUG_BREAK_IF(numThreadsPerThreadGroup * simd > CommonConstants::maxWorkgroupSize);
    return numThreadsPerThreadGroup;
}

template <>
bool GfxCoreHelperHw<Family>::duplicatedInOrderCounterStorageEnabled(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    if (debugManager.flags.InOrderDuplicatedCounterStorageEnabled.get() != -1) {
        return !!debugManager.flags.InOrderDuplicatedCounterStorageEnabled.get();
    }

    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    return rootDeviceEnvironment.getHelper<CompilerProductHelper>().isHeaplessModeEnabled(hwInfo);
}

template <>
bool GfxCoreHelperHw<Family>::inOrderAtomicSignallingEnabled(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    if (debugManager.flags.InOrderAtomicSignallingEnabled.get() != -1) {
        return !!debugManager.flags.InOrderAtomicSignallingEnabled.get();
    }

    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    return rootDeviceEnvironment.getHelper<CompilerProductHelper>().isHeaplessModeEnabled(hwInfo);
}
} // namespace NEO

namespace NEO {
template class GfxCoreHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;
} // namespace NEO
