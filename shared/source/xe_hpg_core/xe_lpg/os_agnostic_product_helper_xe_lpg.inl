/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper.inl"
#include "shared/source/os_interface/product_helper_dg2_and_later.inl"
#include "shared/source/os_interface/product_helper_xehp_and_later.inl"

#include "aubstream/product_family.h"
#include "platforms.h"

namespace NEO {

template <>
uint64_t ProductHelperHw<gfxProduct>::getHostMemCapabilitiesValue() const {
    return (UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS);
}

template <>
bool ProductHelperHw<gfxProduct>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return hwInfo.capabilityTable.ftrRenderCompressedBuffers || hwInfo.capabilityTable.ftrRenderCompressedImages;
}

template <>
bool ProductHelperHw<gfxProduct>::isDirectSubmissionConstantCacheInvalidationNeeded(const HardwareInfo &hwInfo) const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isInitBuiltinAsyncSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <>
bool ProductHelperHw<gfxProduct>::isEvictionIfNecessaryFlagSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::blitEnqueueAllowed() const {
    return false;
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Mtl;
};

template <>
bool ProductHelperHw<gfxProduct>::isDummyBlitWaRequired() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isResolveDependenciesByPipeControlsSupported(const HardwareInfo &hwInfo, bool isOOQ, TaskCountType queueTaskCount, const CommandStreamReceiver &queueCsr) const {
    const bool enabled = !isOOQ && queueTaskCount == queueCsr.peekTaskCount();
    if (DebugManager.flags.ResolveDependenciesViaPipeControls.get() != -1) {
        return DebugManager.flags.ResolveDependenciesViaPipeControls.get() == 1;
    }
    return enabled;
}

template <>
bool ProductHelperHw<gfxProduct>::isBufferPoolAllocatorSupported() const {
    return true;
}

template <>
uint64_t ProductHelperHw<gfxProduct>::overridePatIndex(bool isUncachedType, uint64_t patIndex) const {
    if (isUncachedType) {
        constexpr uint64_t uncached = 2u;
        return uncached;
    }
    return patIndex;
}

template <>
bool ProductHelperHw<gfxProduct>::isPlatformDpasSupported() const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::isPlatformDp4aSupported() const {
    return true;
}

template <>
uint32_t ProductHelperHw<gfxProduct>::getCommandBuffersPreallocatedPerCommandQueue() const {
    return 2u;
}

} // namespace NEO
