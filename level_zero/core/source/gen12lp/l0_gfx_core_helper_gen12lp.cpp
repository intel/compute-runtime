/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_base.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_tgllp_to_dg2.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_tgllp_to_pvc.inl"
#include "level_zero/core/source/helpers/l0_populate_factory.h"
#include "level_zero/tools/source/debug/eu_thread.h"

namespace L0 {

using Family = NEO::Gen12LpFamily;
static auto gfxCore = IGFX_GEN12LP_CORE;

template <>
bool L0GfxCoreHelperHw<Family>::platformSupportsCmdListHeapSharing() const {
    return true;
}

template <>
bool L0GfxCoreHelperHw<Family>::platformSupportsStateComputeModeTracking() const {
    return false;
}

template <>
bool L0GfxCoreHelperHw<Family>::platformSupportsFrontEndTracking() const {
    return false;
}

template <>
bool L0GfxCoreHelperHw<Family>::platformSupportsPipelineSelectTracking() const {
    return false;
}

template <>
bool L0GfxCoreHelperHw<Family>::platformSupportsStateBaseAddressTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const {
    return false;
}

template <>
uint32_t L0GfxCoreHelperHw<Family>::getEventMaxKernelCount(const NEO::HardwareInfo &hwInfo) const {
    return 1;
}

template <>
uint32_t L0GfxCoreHelperHw<Family>::getEventBaseMaxPacketCount(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const {
    return 1u;
}

template <>
NEO::HeapAddressModel L0GfxCoreHelperHw<Family>::getPlatformHeapAddressModel(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const {
    return NEO::HeapAddressModel::privateHeaps;
}

template <>
ze_rtas_format_exp_t L0GfxCoreHelperHw<Family>::getSupportedRTASFormatExp() const {
    return ZE_RTAS_FORMAT_EXP_INVALID;
}

template <>
ze_rtas_format_ext_t L0GfxCoreHelperHw<Family>::getSupportedRTASFormatExt() const {
    return ZE_RTAS_FORMAT_EXT_INVALID;
}

template <>
bool L0GfxCoreHelperHw<Family>::platformSupportsPrimaryBatchBufferCmdList() const {
    return true;
}

template <>
bool L0GfxCoreHelperHw<Family>::platformSupportsImmediateComputeFlushTask() const {
    return false;
}

template <>
ze_mutable_command_exp_flags_t L0GfxCoreHelperHw<Family>::getPlatformCmdListUpdateCapabilities() const {
    return 0;
}

template <>
zet_debug_regset_type_intel_gpu_t L0GfxCoreHelperHw<Family>::getRegsetTypeForLargeGrfDetection() const {
    return ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU;
}

template <>
uint32_t L0GfxCoreHelperHw<Family>::getGrfRegisterCount(uint32_t *regPtr) const {
    return 128;
}

#include "level_zero/core/source/helpers/l0_gfx_core_helper_factory_init.inl"

template class L0GfxCoreHelperHw<Family>;

} // namespace L0
