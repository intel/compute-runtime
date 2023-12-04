/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/implicit_scaling_xehp_and_later.inl"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

namespace NEO {

using Family = XeHpcCoreFamily;

template <>
bool ImplicitScalingDispatch<Family>::pipeControlStallRequired = false;

template <>
bool ImplicitScalingDispatch<Family>::platformSupportsImplicitScaling(const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (ApiSpecificConfig::getApiType() == ApiSpecificConfig::ApiType::OCL) {
        return true;
    } else {
        auto &productHelper = rootDeviceEnvironment.template getHelper<ProductHelper>();
        auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
        return productHelper.isImplicitScalingSupported(hwInfo);
    }
}

template struct ImplicitScalingDispatch<Family>;
template void ImplicitScalingDispatch<Family>::dispatchCommands<Family::DefaultWalkerType>(LinearStream &commandStream, Family::DefaultWalkerType &walkerCmd, void **outWalkerPtr, const DeviceBitfield &devices, uint32_t &partitionCount, bool useSecondaryBatchBuffer, bool apiSelfCleanup, bool usesImages, bool dcFlush, bool forceExecutionOnSingleTile, uint64_t workPartitionAllocationGpuVa, const HardwareInfo &hwInfo);
template size_t ImplicitScalingDispatch<Family>::getSize<Family::DefaultWalkerType>(bool apiSelfCleanup, bool preferStaticPartitioning, const DeviceBitfield &devices, const Vec3<size_t> &groupStart, const Vec3<size_t> &groupCount);

} // namespace NEO
