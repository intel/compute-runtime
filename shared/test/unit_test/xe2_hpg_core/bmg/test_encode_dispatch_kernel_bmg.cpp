/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds_bmg.h"
#include "shared/source/xe2_hpg_core/hw_info_bmg.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"

using namespace NEO;

using CommandEncodeThreadGroupDispatchTestBmg = Test<CommandEncodeStatesFixture>;

BMGTEST_F(CommandEncodeThreadGroupDispatchTestBmg, givenDispatchAllRequiredWhenEncodeThreadGroupDispatchIsCalledThenThreadGroupDispatchSizeOneIsSelectedForDispatchBelowMaxFrontEndThreads) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using InterfaceDescriptorType = typename DefaultWalkerType::InterfaceDescriptorType;

    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironmentRef();
    VariableBackup<uint32_t> sliceCountBackup(&rootDeviceEnvironment.getMutableHardwareInfo()->gtSystemInfo.SliceCount, 5u);
    VariableBackup<uint32_t> maxFrontEndThreadsBackup(&pDevice->deviceInfo.maxFrontEndThreads, 1280u);
    rootDeviceEnvironment.setNumberOfCcsExposedByKmd(1u);
    ASSERT_TRUE(pDevice->getProductHelper().adjustDispatchAllRequired(rootDeviceEnvironment));

    auto hwInfo = pDevice->getHardwareInfo();
    hwInfo.gtSystemInfo.SubSliceCount = 20;
    hwInfo.gtSystemInfo.MaxSubSlicesSupported = 20;
    hwInfo.gtSystemInfo.ThreadCount = 1280;

    DefaultWalkerType walkerCmd{};
    auto encodeThreadGroupDispatch = [&](uint32_t threadGroupCount, uint32_t requiredThreadGroupDispatchSize) {
        auto idd = FamilyType::template getInitInterfaceDescriptor<InterfaceDescriptorType>();
        uint32_t threadGroups[] = {threadGroupCount, 1, 1};
        EncodeDispatchKernel<FamilyType>::encodeThreadGroupDispatch(idd, *pDevice, hwInfo, threadGroups, threadGroupCount, requiredThreadGroupDispatchSize, GrfConfig::defaultGrfNumber, 2u, walkerCmd);
        return idd.getThreadGroupDispatchSize();
    };

    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, encodeThreadGroupDispatch(128u, 0u));
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1, encodeThreadGroupDispatch(639u, 0u));
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, encodeThreadGroupDispatch(640u, 0u));
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2, encodeThreadGroupDispatch(128u, InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2));

    rootDeviceEnvironment.setNumberOfCcsExposedByKmd(2u);
    ASSERT_FALSE(pDevice->getProductHelper().adjustDispatchAllRequired(rootDeviceEnvironment));
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_4, encodeThreadGroupDispatch(128u, 0u));
    EXPECT_EQ(InterfaceDescriptorType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_8, encodeThreadGroupDispatch(639u, 0u));
}
