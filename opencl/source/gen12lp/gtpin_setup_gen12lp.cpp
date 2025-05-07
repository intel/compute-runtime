/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/implicit_args_helper.h"

#include "opencl/source/gtpin/gtpin_gfx_core_helper.h"
#include "opencl/source/gtpin/gtpin_gfx_core_helper.inl"

#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace NEO {

template <typename GfxFamily>
bool GTPinGfxCoreHelperHw<GfxFamily>::canUseSharedAllocation(const HardwareInfo &hwInfo) const {
    bool canUseSharedAllocation = false;
    if (debugManager.flags.GTPinAllocateBufferInSharedMemory.get() != -1) {
        canUseSharedAllocation = !!debugManager.flags.GTPinAllocateBufferInSharedMemory.get();
    }
    return canUseSharedAllocation;
}

extern GTPinGfxCoreHelperCreateFunctionType gtpinGfxCoreHelperFactory[IGFX_MAX_CORE];

typedef Gen12LpFamily Family;
static const auto gfxFamily = IGFX_GEN12LP_CORE;

template <>
uint32_t GTPinGfxCoreHelperHw<Family>::getGenVersion() const {
    return gtpin::GTPIN_GEN_12_1;
}

template class GTPinGfxCoreHelperHw<Family>;

struct GTPinEnableGen12LP {
    GTPinEnableGen12LP() {
        gtpinGfxCoreHelperFactory[gfxFamily] = GTPinGfxCoreHelperHw<Family>::create;
    }
};

static GTPinEnableGen12LP gtpinEnable;

} // namespace NEO
