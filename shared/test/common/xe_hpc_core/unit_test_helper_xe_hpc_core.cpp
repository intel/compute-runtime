/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/source/xe_hpc_core/hw_info.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/unit_test_helper.inl"
#include "shared/test/common/helpers/unit_test_helper_xe_hpc_and_later.inl"
#include "shared/test/common/helpers/unit_test_helper_xe_hpg_and_xe_hpc.inl"
#include "shared/test/common/helpers/unit_test_helper_xehp_and_later.inl"

using Family = NEO::XeHpcCoreFamily;

#include "unit_test_helper_xe_hpc_core_extra.inl"

namespace NEO {

template <>
bool UnitTestHelper<Family>::requiresTimestampPacketsInSystemMemory(HardwareInfo &hwInfo) {
    return false;
}

template <>
bool UnitTestHelper<Family>::isAdditionalMiSemaphoreWaitRequired(const RootDeviceEnvironment &rootDeviceEnvironment) {
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto programGlobalFenceAsMiMemFenceCommandInCommandStream = productHelper.isReleaseGlobalFenceInCommandStreamRequired(hwInfo);
    if (debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() != -1) {
        programGlobalFenceAsMiMemFenceCommandInCommandStream = !!debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get();
    }
    return !programGlobalFenceAsMiMemFenceCommandInCommandStream;
}

template <>
bool UnitTestHelperWithHeap<Family>::getComputeDispatchAllWalkerFromFrontEndCommand(const typename Family::FrontEndStateCommand &feCmd) {
    return feCmd.getComputeDispatchAllWalkerEnable();
}

template <>
void UnitTestHelper<Family>::verifyDummyBlitWa(const RootDeviceEnvironment *rootDeviceEnvironment, GenCmdList::iterator &cmdIterator) {
    const auto releaseHelper = rootDeviceEnvironment->getReleaseHelper();
    if (releaseHelper->isDummyBlitWaRequired()) {
        auto dummyBltCmd = genCmdCast<typename Family::MEM_SET *>(*(cmdIterator++));
        EXPECT_NE(nullptr, dummyBltCmd);

        uint32_t expectedSize = 32 * MemoryConstants::kiloByte;
        auto expectedGpuBaseAddress = rootDeviceEnvironment->getDummyAllocation()->getGpuAddress();

        EXPECT_EQ(expectedGpuBaseAddress, dummyBltCmd->getDestinationStartAddress());
        EXPECT_EQ(expectedSize, dummyBltCmd->getDestinationPitch());
        EXPECT_EQ(expectedSize, dummyBltCmd->getFillWidth());
    }
}
template struct UnitTestHelper<Family>;
template struct UnitTestHelperWithHeap<Family>;

template uint64_t UnitTestHelper<Family>::getWalkerActivePostSyncAddress<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER *walkerCmd);
} // namespace NEO
