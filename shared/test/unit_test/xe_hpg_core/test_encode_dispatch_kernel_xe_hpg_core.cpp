/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/hw_walk_order.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/test//common/helpers/raii_product_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

using namespace NEO;

using CommandEncodeStatesTestXeHpgCore = ::testing::Test;

HWTEST2_F(CommandEncodeStatesTestXeHpgCore, givenVariousValuesWhenCallingSetBarrierEnableThenCorrectValuesAreSet, IsXeHpgCore) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;

    uint32_t barrierCounts[] = {0, 1};

    KernelDescriptor kd = {};
    for (auto barrierCount : barrierCounts) {
        kd.kernelAttributes.barrierCount = barrierCount;
        EncodeDispatchKernel<FamilyType>::programBarrierEnable(idd, kd, *defaultHwInfo);

        EXPECT_EQ(barrierCount, idd.getNumberOfBarriers());
    }
}
template <PRODUCT_FAMILY productFamily>
struct TempMockProductHelper : NEO::ProductHelperHw<productFamily> {
    bool isAdjustWalkOrderAvailable(const ReleaseHelper *releaseHelper) const override { return true; }
};

HWTEST2_F(CommandEncodeStatesTestXeHpgCore, givenRequiredWorkGroupOrderAndIsAdjustWalkOrderAvailableReturnTrueWhenCallAdjustWalkOrderThenWalkerIsProgrammedCorrectly, IsXeHpgCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    RAIIProductHelperFactory<TempMockProductHelper<productFamily>> raii(rootDeviceEnvironment);

    DefaultWalkerType walkerCmd{};
    DefaultWalkerType walkerOnStart{};

    uint32_t fakeOrder = 5u;
    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, fakeOrder, rootDeviceEnvironment);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(DefaultWalkerType))); // no change

    uint32_t yOrder = 2u;
    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, yOrder, rootDeviceEnvironment);
    EXPECT_EQ(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_Y_ORDER_WALK, walkerCmd.getDispatchWalkOrder());

    uint32_t linearOrder = 0u;
    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, linearOrder, rootDeviceEnvironment);
    EXPECT_EQ(DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_LINEAR_WALK, walkerCmd.getDispatchWalkOrder());
}

using EncodeKernelXeHpgCoreTest = Test<CommandEncodeStatesFixture>;

XE_HPG_CORETEST_F(EncodeKernelXeHpgCoreTest, givenRequiredWorkGroupOrderWhenCallAdjustWalkOrderThenDispatchWalkOrderIsProgrammedCorrectly) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DefaultWalkerType walkerCmd{};
    uint32_t yOrder = 2u;

    auto &productHelper = getHelper<ProductHelper>();
    auto releaseHelper = getReleaseHelper();
    auto &rootDeviceEnvironment = this->pDevice->getRootDeviceEnvironment();
    auto isExpectedNewWalkOrderApplied = productHelper.isAdjustWalkOrderAvailable(releaseHelper);

    EXPECT_EQ(HwWalkOrderHelper::compatibleDimensionOrders[yOrder], HwWalkOrderHelper::yOrderWalk);

    auto dispatchWalkOrderBeforeAdjust = walkerCmd.getDispatchWalkOrder();

    uint32_t fakeOrder = 5u;
    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, fakeOrder, rootDeviceEnvironment);
    EXPECT_EQ(dispatchWalkOrderBeforeAdjust, walkerCmd.getDispatchWalkOrder()); // no change

    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, yOrder, rootDeviceEnvironment);
    auto expectedWalkOrder = isExpectedNewWalkOrderApplied ? DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_Y_ORDER_WALK : dispatchWalkOrderBeforeAdjust;
    EXPECT_EQ(expectedWalkOrder, walkerCmd.getDispatchWalkOrder());

    uint32_t linearOrder = 0u;
    EXPECT_EQ(HwWalkOrderHelper::compatibleDimensionOrders[linearOrder], HwWalkOrderHelper::linearWalk);

    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, linearOrder, rootDeviceEnvironment);
    expectedWalkOrder = isExpectedNewWalkOrderApplied ? DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_LINEAR_WALK : dispatchWalkOrderBeforeAdjust;
    EXPECT_EQ(expectedWalkOrder, walkerCmd.getDispatchWalkOrder());
}

XE_HPG_CORETEST_F(EncodeKernelXeHpgCoreTest, givenRequiredWorkGroupOrderWhenCallEncodeThreadDataThenDispatchWalkOrderIsProgrammedCorrectly) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DefaultWalkerType walkerCmd = FamilyType::cmdInitGpgpuWalker;

    uint32_t startWorkGroup[3] = {1, 1, 1};
    uint32_t numWorkGroups[3] = {1, 1, 1};
    uint32_t workGroupSizes[3] = {1, 1, 1};

    auto &productHelper = getHelper<ProductHelper>();
    auto releaseHelper = getReleaseHelper();

    auto isExpectedNewWalkOrderApplied = productHelper.isAdjustWalkOrderAvailable(releaseHelper);
    auto dispatchWalkOrderBeforeAdjust = walkerCmd.getDispatchWalkOrder();
    auto &rootDeviceEnvironment = this->pDevice->getRootDeviceEnvironment();

    uint32_t yOrder = 2u;
    EXPECT_EQ(HwWalkOrderHelper::compatibleDimensionOrders[yOrder], HwWalkOrderHelper::yOrderWalk);

    auto expectedWalkOrder = isExpectedNewWalkOrderApplied ? DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_Y_ORDER_WALK : dispatchWalkOrderBeforeAdjust;
    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, 0, 3,
                                                       0, 1, false, false, true, yOrder, rootDeviceEnvironment);
    EXPECT_EQ(expectedWalkOrder, walkerCmd.getDispatchWalkOrder());

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, 0, 3,
                                                       0, 1, true, false, true, yOrder, rootDeviceEnvironment);
    EXPECT_EQ(expectedWalkOrder, walkerCmd.getDispatchWalkOrder());

    uint32_t linearOrder = 0u;
    EXPECT_EQ(HwWalkOrderHelper::compatibleDimensionOrders[linearOrder], HwWalkOrderHelper::linearWalk);

    expectedWalkOrder = isExpectedNewWalkOrderApplied ? DefaultWalkerType::DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_LINEAR_WALK : dispatchWalkOrderBeforeAdjust;
    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, 0, 3,
                                                       0, 1, false, false, true, linearOrder, rootDeviceEnvironment);
    EXPECT_EQ(expectedWalkOrder, walkerCmd.getDispatchWalkOrder());

    EncodeDispatchKernel<FamilyType>::encodeThreadData(walkerCmd, startWorkGroup, numWorkGroups, workGroupSizes, 0, 3,
                                                       0, 1, true, false, true, linearOrder, rootDeviceEnvironment);
    EXPECT_EQ(expectedWalkOrder, walkerCmd.getDispatchWalkOrder());
}
