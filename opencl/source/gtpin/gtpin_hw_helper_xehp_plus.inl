/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "opencl/source/gtpin/gtpin_hw_helper.h"

namespace NEO {

template <typename GfxFamily>
bool GTPinHwHelperHw<GfxFamily>::canUseSharedAllocation(const HardwareInfo &hwInfo) const {
    bool canUseSharedAllocation = true;
    if (DebugManager.flags.GTPinAllocateBufferInSharedMemory.get() != -1) {
        canUseSharedAllocation = !!DebugManager.flags.GTPinAllocateBufferInSharedMemory.get();
    }
    return canUseSharedAllocation;
}

} // namespace NEO
