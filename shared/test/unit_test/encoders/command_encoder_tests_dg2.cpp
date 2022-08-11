/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using DG2CommandEncoderTest = Test<DeviceFixture>;
HWTEST_EXCLUDE_PRODUCT(XeHPAndLaterCommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_IsAtLeastXeHpCore, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(CommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_Platforms, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(XeHPAndLaterCommandEncoderTest, givenCommandContainerWithDirtyHeapWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_IsAtLeastXeHpCore, IGFX_DG2);

HWTEST2_F(DG2CommandEncoderTest, givenDG2WhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsDG2) {
    class MockCommandContainer : public CommandContainer {
      public:
        MockCommandContainer() : CommandContainer() {}

        void clearHeaps() {
            dirtyHeaps = 0;
        }
    };

    auto container = MockCommandContainer();
    container.clearHeaps();
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container);
    EXPECT_EQ(size, 176ul);
}

HWTEST2_F(DG2CommandEncoderTest, givenDG2AndCommandContainerWithDirtyHeapWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsDG2) {
    auto container = CommandContainer();
    container.setHeapDirty(HeapType::SURFACE_STATE);
    size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container);
    EXPECT_EQ(size, 192ul);
}

HWTEST2_F(DG2CommandEncoderTest, givenInterfaceDescriptorDataWhenForceThreadGroupDispatchSizeVariableIsDefaultThenThreadGroupDispatchSizeIsNotChanged, IsDG2) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA iddArg;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;
    const uint32_t forceThreadGroupDispatchSize = -1;
    auto hwInfo = pDevice->getHardwareInfo();
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);

    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceThreadGroupDispatchSize.set(forceThreadGroupDispatchSize);

    uint32_t revisions[] = {REVISION_A0, REVISION_B};
    for (auto revision : revisions) {
        hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(revision, hwInfo);

        for (auto numberOfThreadsInGroup : {1u, 4u, 16u}) {
            iddArg.setNumberOfThreadsInGpgpuThreadGroup(numberOfThreadsInGroup);
            EncodeDispatchKernel<FamilyType>::adjustInterfaceDescriptorData(iddArg, hwInfo, 0, 0);

            if (hwInfoConfig.isDisableOverdispatchAvailable(hwInfo)) {
                if (numberOfThreadsInGroup == 1) {
                    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2, iddArg.getThreadGroupDispatchSize());
                } else {
                    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, iddArg.getThreadGroupDispatchSize());
                }
            } else {
                EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, iddArg.getThreadGroupDispatchSize());
            }
        }
    }
}
