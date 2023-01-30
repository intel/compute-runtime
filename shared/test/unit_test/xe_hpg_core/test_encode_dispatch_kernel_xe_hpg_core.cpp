/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "hw_cmds_xe_hpg_core_base.h"

using namespace NEO;

using CommandEncodeStatesTestXeHpgCore = ::testing::Test;

HWTEST2_F(CommandEncodeStatesTestXeHpgCore, givenVariousValuesWhenCallingSetBarrierEnableThenCorrectValuesAreSet, IsXeHpgCore) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;

    uint32_t barrierCounts[] = {0, 1};

    for (auto barrierCount : barrierCounts) {
        EncodeDispatchKernel<FamilyType>::programBarrierEnable(idd, barrierCount, *defaultHwInfo);

        EXPECT_EQ(barrierCount, idd.getNumberOfBarriers());
    }
}
template <PRODUCT_FAMILY productFamily>
struct MockProductHelper : NEO::ProductHelperHw<productFamily> {
    bool isAdjustWalkOrderAvailable(const HardwareInfo &hwInfo) const override { return true; }
};

HWTEST2_F(CommandEncodeStatesTestXeHpgCore, givenRequiredWorkGroupOrderAndIsAdjustWalkOrderAvailableReturnTrueWhenCallAdjustWalkOrderThenWalkerIsProgrammedCorrectly, IsXeHpgCore) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    MockProductHelper<productFamily> productHelper{};
    VariableBackup<ProductHelper *> productHelperFactoryBackup{&NEO::productHelperFactory[static_cast<size_t>(defaultHwInfo->platform.eProductFamily)]};
    productHelperFactoryBackup = &productHelper;

    WALKER_TYPE walkerCmd{};
    WALKER_TYPE walkerOnStart{};

    uint32_t fakeOrder = 5u;
    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, fakeOrder, rootDeviceEnvironment);
    EXPECT_EQ(0, memcmp(&walkerOnStart, &walkerCmd, sizeof(WALKER_TYPE))); // no change

    uint32_t yOrder = 2u;
    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, yOrder, rootDeviceEnvironment);
    EXPECT_EQ(WALKER_TYPE::DISPATCH_WALK_ORDER::Y_ORDER_WALKER, walkerCmd.getDispatchWalkOrder());

    uint32_t linearOrder = 0u;
    EncodeDispatchKernel<FamilyType>::adjustWalkOrder(walkerCmd, linearOrder, rootDeviceEnvironment);
    EXPECT_EQ(WALKER_TYPE::DISPATCH_WALK_ORDER::LINERAR_WALKER, walkerCmd.getDispatchWalkOrder());
}