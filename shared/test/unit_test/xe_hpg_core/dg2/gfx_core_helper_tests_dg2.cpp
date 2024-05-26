/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_ail_configuration.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using GfxCoreHelperTestDg2 = ::testing::Test;

DG2TEST_F(GfxCoreHelperTestDg2, whenGetExtensionsIsCalledThenMatrixMultiplyAccumulateExtensionsAreReturned) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto *releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    auto extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);

    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_intel_subgroup_matrix_multiply_accumulate")));
    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_intel_subgroup_split_matrix_multiply_accumulate")));
}

DG2TEST_F(GfxCoreHelperTestDg2, givenRcsDisabledWhenGetGpgpuEnginesCalledThenDontSetRcs) {
    HardwareInfo hwInfo = *defaultHwInfo;
    MockExecutionEnvironment mockExecutionEnvironment{};

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(8u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(8u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[3].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[4].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[5].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[6].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[7].first);
}

DG2TEST_F(GfxCoreHelperTestDg2, givenRcsDisabledWhenAilForceToUseRcsThenRcsIsSetAndCssIsNot) {
    HardwareInfo hwInfo = *defaultHwInfo;
    MockExecutionEnvironment mockExecutionEnvironment{};

    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto ailConfiguration = std::make_unique<MockAILConfiguration>();
    ailConfiguration->forceRcsValue = true;
    auto &rootDeviceEnvironment = *device->getExecutionEnvironment()->rootDeviceEnvironments[0];
    rootDeviceEnvironment.ailConfiguration.reset(ailConfiguration.release());
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(5u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[0].first);
    EXPECT_EQ(getChosenEngineType(hwInfo), engines[1].first);
    EXPECT_EQ(getChosenEngineType(hwInfo), engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[3].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[4].first);
}

DG2TEST_F(GfxCoreHelperTestDg2, givenRcsDisabledButDebugVariableSetWhenGetGpgpuEnginesCalledThenSetRcs) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;

    DebugManagerStateRestore restore;
    debugManager.flags.NodeOrdinal.set(static_cast<int32_t>(aub_stream::EngineType::ENGINE_RCS));

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    EXPECT_EQ(9u, device->allEngines.size());
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_EQ(9u, engines.size());

    EXPECT_EQ(aub_stream::ENGINE_CCS, engines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS1, engines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS2, engines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_CCS3, engines[3].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[4].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[5].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, engines[6].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[7].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, engines[8].first);
}

using GfxCoreHelperTests = Test<DeviceFixture>;
DG2TEST_F(GfxCoreHelperTests, givenAllocationTypeInternalHeapWhenSetExtraAllocationDataThenUseSystemMemory) {
    HardwareInfo &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();

    hwInfo.platform.usDeviceID = dg2G10DeviceIds[0];
    hwInfo.platform.usRevId = 0;

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    constexpr DeviceBitfield singleTileBitfield = 0b0100;

    const AllocationProperties singleTileAllocProperties(0, 1, AllocationType::internalHeap, singleTileBitfield);

    AllocationData allocData;
    allocData.flags.useSystemMemory = false;

    gfxCoreHelper.setExtraAllocationData(allocData, singleTileAllocProperties, pDevice->getRootDeviceEnvironment());
    EXPECT_TRUE(allocData.flags.useSystemMemory);
}
