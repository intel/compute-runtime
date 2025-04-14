/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/memory_manager/allocation_type.h"

#include "aubstream/product_family.h"

namespace NEO {

template <>
bool ProductHelperHw<gfxProduct>::isResolveDependenciesByPipeControlsSupported(const HardwareInfo &hwInfo, bool isOOQ, TaskCountType queueTaskCount, const CommandStreamReceiver &queueCsr) const {
    const bool enabled = false;
    if (debugManager.flags.ResolveDependenciesViaPipeControls.get() != -1) {
        return debugManager.flags.ResolveDependenciesViaPipeControls.get() == 1;
    }
    return enabled;
}

template <>
std::optional<aub_stream::ProductFamily> ProductHelperHw<gfxProduct>::getAubStreamProductFamily() const {
    return aub_stream::ProductFamily::Bmg;
};

template <>
std::optional<GfxMemoryAllocationMethod> ProductHelperHw<gfxProduct>::getPreferredAllocationMethod(AllocationType allocationType) const {
    return GfxMemoryAllocationMethod::allocateByKmd;
}

template <>
void ProductHelperHw<gfxProduct>::adjustNumberOfCcs(HardwareInfo &hwInfo) const {
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;
}

template <>
bool ProductHelperHw<gfxProduct>::isDirectSubmissionSupported(ReleaseHelper *releaseHelper) const {
    return true;
}

template <>
bool ProductHelperHw<gfxProduct>::adjustDispatchAllRequired(const HardwareInfo &hwInfo) const {
    return hwInfo.gtSystemInfo.SliceCount > 2u;
}

template <>
void ProductHelperHw<gfxProduct>::adjustScratchSize(size_t &requiredScratchSize) const {
    requiredScratchSize *= 2;
}

} // namespace NEO
