/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe3_core/hw_info_xe3_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "neo_aot_platforms.h"

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
}

using namespace NEO;

using PtlProductHelper = ProductHelperTest;

PTLTEST_F(PtlProductHelper, whenGettingPreferredAllocationMethodThenAllocateByKmdIsReturned) {
    for (auto i = 0; i < static_cast<int>(AllocationType::count); i++) {
        auto allocationType = static_cast<AllocationType>(i);
        auto preferredAllocationMethod = productHelper->getPreferredAllocationMethod(allocationType);
        EXPECT_TRUE(preferredAllocationMethod.has_value());
        EXPECT_EQ(GfxMemoryAllocationMethod::allocateByKmd, preferredAllocationMethod.value());
    }
}

PTLTEST_F(PtlProductHelper, givenCompilerProductHelperWhenGetDefaultHwIpVersionThenCorrectValueIsSet) {
    EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), AOT::PTL_H_B0);
}

HWTEST_EXCLUDE_PRODUCT(CompilerProductHelperFixture, WhenIsMidThreadPreemptionIsSupportedIsCalledThenCorrectResultIsReturned, IGFX_PTL);
PTLTEST_F(PtlProductHelper, givenCompilerProductHelperWhenGetMidThreadPreemptionSupportThenCorrectValueIsSet) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrWalkerMTP = false;
    EXPECT_FALSE(compilerProductHelper->isMidThreadPreemptionSupported(hwInfo));
    hwInfo.featureTable.flags.ftrWalkerMTP = true;
    EXPECT_TRUE(compilerProductHelper->isMidThreadPreemptionSupported(hwInfo));
}

PTLTEST_F(PtlProductHelper, givenProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isDirectSubmissionSupported(releaseHelper));
}

PTLTEST_F(PtlProductHelper, givenProductHelperWhenCheckoverrideAllocationCpuCacheableThenTrueIsReturnedForCommandBuffer) {
    AllocationData allocationData{};
    allocationData.type = AllocationType::commandBuffer;
    EXPECT_TRUE(productHelper->overrideAllocationCpuCacheable(allocationData));

    allocationData.type = AllocationType::buffer;
    EXPECT_FALSE(productHelper->overrideAllocationCpuCacheable(allocationData));
}

PTLTEST_F(PtlProductHelper, givenProductHelperWhenCallIsCachingOnCpuAvailableThenFalseIsReturned) {
    EXPECT_FALSE(productHelper->isCachingOnCpuAvailable());
}

PTLTEST_F(PtlProductHelper, givenProductHelperWhenIsInitBuiltinAsyncSupportedThenReturnTrue) {
    EXPECT_TRUE(productHelper->isInitBuiltinAsyncSupported(*defaultHwInfo));
}

PTLTEST_F(PtlProductHelper, givenProductHelperWhenCallIsStagingBuffersEnabledThenReturnTrue) {
    EXPECT_TRUE(productHelper->isStagingBuffersEnabled());
}

PTLTEST_F(PtlProductHelper, givenProductHelperWhenCheckingIsBufferPoolAllocatorSupportedThenCorrectValueIsReturned) {
    EXPECT_TRUE(productHelper->isBufferPoolAllocatorSupported());
}

PTLTEST_F(PtlProductHelper, givenDebugFlagWhenCheckingIsResolveDependenciesByPipeControlsSupportedThenTheFlagDerivedValueIsReturned) {
    DebugManagerStateRestore restorer;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiverWithDirectSubmissionRelaxedOrdering<false> csr(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    MockCommandStreamReceiverWithDirectSubmissionRelaxedOrdering<true> csrRelaxed(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr.taskCount = 2;
    csrRelaxed.taskCount = 2;
    auto productHelper = &mockDevice->getProductHelper();

    debugManager.flags.ResolveDependenciesViaPipeControls.set(0);
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csrRelaxed));

    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csrRelaxed));
}

PTLTEST_F(PtlProductHelper, givenResolveDependenciesByPipeControllsSupportedWhenCheckedThenReturnsTrue) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiverWithDirectSubmissionRelaxedOrdering<false> csr(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr.taskCount = 2;
    auto productHelper = &mockDevice->getProductHelper();

    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
}

PTLTEST_F(PtlProductHelper, givenResolveDependenciesByPipeControllsNotSupportedWhenCheckedThenReturnsFalse) {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiverWithDirectSubmissionRelaxedOrdering<false> csr(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    MockCommandStreamReceiverWithDirectSubmissionRelaxedOrdering<true> csrRelaxed(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr.taskCount = 2;
    csrRelaxed.taskCount = 2;
    auto productHelper = &mockDevice->getProductHelper();

    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csrRelaxed));
}

PTLTEST_F(PtlProductHelper, givenProductHelperWhenCheckingIsHostDeviceUsmPoolAllocatorSupportedThenCorrectValueIsReturned) {
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
        EXPECT_TRUE(productHelper->isHostUsmPoolAllocatorSupported());
        EXPECT_TRUE(productHelper->isDeviceUsmPoolAllocatorSupported());
    }
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
        EXPECT_TRUE(productHelper->isHostUsmPoolAllocatorSupported());
        EXPECT_TRUE(productHelper->isDeviceUsmPoolAllocatorSupported());
    }
}