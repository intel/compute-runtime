/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using DG2CommandEncoderTest = Test<DeviceFixture>;
HWTEST_EXCLUDE_PRODUCT(XeHPAndLaterCommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_IsAtLeastXeCore, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(CommandEncoderTest, whenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_Platforms, IGFX_DG2);
HWTEST_EXCLUDE_PRODUCT(XeHPAndLaterCommandEncoderTest, givenCommandContainerWithDirtyHeapWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned_IsHeapfulSupportedAndAtLeastXeCore, IGFX_DG2);

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

    auto &hwInfo = *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    auto &productHelper = pDevice->getProductHelper();
    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
        auto expectedSize = 88ull;
        if (productHelper.isAdditionalStateBaseAddressWARequired(hwInfo)) {
            expectedSize += sizeof(typename FamilyType::STATE_BASE_ADDRESS);
        }
        size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container, false);
        EXPECT_EQ(size, expectedSize);
    }
}

HWTEST2_F(DG2CommandEncoderTest, givenDG2AndCommandContainerWithDirtyHeapWhenGettingRequiredSizeForStateBaseAddressCommandThenCorrectSizeIsReturned, IsDG2) {
    auto container = CommandContainer();
    container.setHeapDirty(HeapType::surfaceState);

    auto &hwInfo = *pDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    auto &productHelper = pDevice->getProductHelper();
    for (uint8_t revision : {REVISION_A0, REVISION_A1, REVISION_B, REVISION_C}) {
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
        auto expectedSize = 104ull;
        if (productHelper.isAdditionalStateBaseAddressWARequired(hwInfo)) {
            expectedSize += sizeof(typename FamilyType::STATE_BASE_ADDRESS);
        }
        size_t size = EncodeStateBaseAddress<FamilyType>::getRequiredSizeForStateBaseAddress(*pDevice, container, false);
        EXPECT_EQ(size, expectedSize);
    }
}

HWTEST2_F(DG2CommandEncoderTest, givenInterfaceDescriptorDataWhenForceThreadGroupDispatchSizeVariableIsDefaultThenThreadGroupDispatchSizeIsNotChanged, IsDG2) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    INTERFACE_DESCRIPTOR_DATA iddArg;
    DefaultWalkerType walkerCmd{};
    uint32_t threadGroups[] = {walkerCmd.getThreadGroupIdXDimension(), walkerCmd.getThreadGroupIdYDimension(), walkerCmd.getThreadGroupIdZDimension()};
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;
    const uint32_t forceThreadGroupDispatchSize = -1;
    auto hwInfo = pDevice->getHardwareInfo();
    const auto &productHelper = pDevice->getProductHelper();

    DebugManagerStateRestore restorer;
    debugManager.flags.ForceThreadGroupDispatchSize.set(forceThreadGroupDispatchSize);

    uint32_t revisions[] = {REVISION_A0, REVISION_B};
    for (auto revision : revisions) {
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);

        for (auto numberOfThreadsInGroup : {1u, 4u, 16u}) {
            iddArg.setNumberOfThreadsInGpgpuThreadGroup(numberOfThreadsInGroup);
            EncodeDispatchKernel<FamilyType>::encodeThreadGroupDispatch(iddArg, *pDevice, hwInfo, threadGroups, 0, 0, 0, numberOfThreadsInGroup, walkerCmd);

            if (productHelper.isDisableOverdispatchAvailable(hwInfo)) {
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
