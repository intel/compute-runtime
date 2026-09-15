/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/source/release_helpers/compiler_release_helper/compiler_release_helper.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
CompilerProductHelperHw<gfxProduct>::CompilerProductHelperHw() = default;

template <PRODUCT_FAMILY gfxProduct>
uint64_t CompilerProductHelperHw<gfxProduct>::getHwInfoConfig(const HardwareInfo &hwInfo) const {
    return 0x0;
}

template <PRODUCT_FAMILY gfxProduct>
const char *CompilerProductHelperHw<gfxProduct>::getCachingPolicyOptions(bool isDebuggerActive) const {
    return L1CachePolicyHelper<gfxProduct>::getCachingPolicyOptions(isDebuggerActive);
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::failBuildProgramWithBufferStatefulAccessPreference() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isHeaplessModeEnabled(const HardwareInfo &hwInfo) const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
uint32_t CompilerProductHelperHw<gfxProduct>::matchRevisionIdWithProductConfig(HardwareIpVersion ipVersion, uint32_t revisionID) const {
    return ipVersion.value;
}

template <PRODUCT_FAMILY gfxProduct>
void CompilerProductHelperHw<gfxProduct>::adjustHwInfoForIgc(HardwareInfo &hwInfo) const {
}

template <PRODUCT_FAMILY gfxProduct>
const char *CompilerProductHelperHw<gfxProduct>::getCustomIgcLibraryName() const {
    if (debugManager.flags.IgcLibraryName.get() != "unk") {
        return debugManager.flags.IgcLibraryName.getRef().c_str();
    }
    return nullptr;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::useIgcAsFcl() const {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
IGC::CodeType::CodeType_t CompilerProductHelperHw<gfxProduct>::getPreferredIntermediateRepresentation() const {
    return IGC::CodeType::spirV;
}

} // namespace NEO
