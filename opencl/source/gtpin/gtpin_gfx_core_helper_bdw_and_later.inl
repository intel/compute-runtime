/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"

#include "opencl/source/gtpin/gtpin_gfx_core_helper.h"

namespace NEO {

template <typename GfxFamily>
bool GTPinGfxCoreHelperHw<GfxFamily>::canUseSharedAllocation(const HardwareInfo &hwInfo) const {
    bool canUseSharedAllocation = false;
    if (debugManager.flags.GTPinAllocateBufferInSharedMemory.get() != -1) {
        canUseSharedAllocation = !!debugManager.flags.GTPinAllocateBufferInSharedMemory.get();
    }
    canUseSharedAllocation &= hwInfo.capabilityTable.ftrSvm;
    return canUseSharedAllocation;
}

} // namespace NEO
