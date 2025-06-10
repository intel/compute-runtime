/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_aot_config_tgllp_and_later.inl"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/compiler_product_helper_base.inl"
#include "shared/source/helpers/compiler_product_helper_enable_subgroup_local_block_io.inl"
#include "shared/source/helpers/compiler_product_helper_tgllp_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_xe_hp_and_later.inl"
#include "shared/source/helpers/compiler_product_helper_xe_hpc_and_later.inl"
#include "shared/source/xe_hpc_core/hw_cmds.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"

#include "neo_aot_platforms.h"
namespace NEO {
template <>
uint32_t CompilerProductHelperHw<IGFX_PVC>::getDefaultHwIpVersion() const {
    return AOT::PVC_XT_C0;
}

template <>
uint32_t CompilerProductHelperHw<IGFX_PVC>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    auto deviceId = hwInfo.platform.usDeviceID;
    bool isPvcXl = (std::find(pvcXlDeviceIds.begin(), pvcXlDeviceIds.end(), deviceId) != pvcXlDeviceIds.end());
    bool isPvcXt = (std::find(pvcXtDeviceIds.begin(), pvcXtDeviceIds.end(), deviceId) != pvcXtDeviceIds.end());
    bool isPvcXtVg = (std::find(pvcXtVgDeviceIds.begin(), pvcXtVgDeviceIds.end(), deviceId) != pvcXtVgDeviceIds.end());

    if (isPvcXtVg) {
        if ((hwInfo.platform.usRevId & PVC::pvcSteppingBits) == 7) {
            return AOT::PVC_XT_C0_VG;
        }
    } else if (isPvcXl) {
        switch (hwInfo.platform.usRevId & PVC::pvcSteppingBits) {
        case 0x0:
            return AOT::PVC_XL_A0;
        default:
        case 0x1:
            return AOT::PVC_XL_A0P;
        }
    } else if (isPvcXt) {
        switch (hwInfo.platform.usRevId & PVC::pvcSteppingBits) {
        case 0x3:
            return AOT::PVC_XT_A0;
        case 0x5:
            return AOT::PVC_XT_B0;
        case 0x6:
            return AOT::PVC_XT_B1;
        default:
        case 0x7:
            return AOT::PVC_XT_C0;
        }
    }
    return getDefaultHwIpVersion();
}
template <>
uint32_t CompilerProductHelperHw<IGFX_PVC>::matchRevisionIdWithProductConfig(HardwareIpVersion ipVersion, uint32_t revisionID) const {
    HardwareIpVersion pvcConfig = ipVersion;
    pvcConfig.revision = revisionID & PVC::pvcSteppingBits;
    return pvcConfig.value;
}

template <>
bool CompilerProductHelperHw<IGFX_PVC>::failBuildProgramWithStatefulAccessPreference() const {
    return false;
}

template <>
bool CompilerProductHelperHw<IGFX_PVC>::isMatrixMultiplyAccumulateSupported(const ReleaseHelper *releaseHelper) const {
    if (releaseHelper) {
        return releaseHelper->isMatrixMultiplyAccumulateSupported();
    }
    return true;
}

template <>
bool CompilerProductHelperHw<IGFX_PVC>::isMatrixMultiplyAccumulateTF32Supported(const HardwareInfo &hwInfo) const {
    auto config = getProductConfigFromHwInfo(hwInfo);
    if (config >= AOT::PVC_XT_B0 && config < AOT::PVC_XT_C0_VG)
        return true;
    return false;
}

template <>
bool CompilerProductHelperHw<IGFX_PVC>::isBFloat16ConversionSupported(const ReleaseHelper *releaseHelper) const {
    return true;
}

static EnableCompilerProductHelper<IGFX_PVC> enableCompilerProductHelperPVC;

} // namespace NEO
