/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/linux/local_memory_helper.h"
#include "shared/source/os_interface/linux/memory_info_impl.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/test/unit_test/os_interface/linux/drm_mock_impl.h"
#include "opencl/test/unit_test/os_interface/linux/drm_mock_prod_dg1.h"
#include "test.h"

using namespace NEO;

using LocalMemoryHelperTestsDg1 = ::testing::Test;

DG1TEST_F(LocalMemoryHelperTestsDg1, givenDg1WhenCreateGemExtThenReturnCorrectValue) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probed_size = 8 * GB;
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probed_size = 16 * GB;

    auto localMemHelper = LocalMemoryHelper::get(defaultHwInfo->platform.eProductFamily);
    uint32_t handle = 0;
    auto ret = localMemHelper->createGemExt(drm.get(), &regionInfo[1], 1, 1024, handle);

    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(1u, drm->numRegions);
    EXPECT_EQ(1024u, drm->createExt.size);
    EXPECT_EQ(I915_MEMORY_CLASS_DEVICE, drm->memRegions.memory_class);
}

DG1TEST_F(LocalMemoryHelperTestsDg1, givenDg1WithDrmTipWhenCreateGemExtWithDebugFlagThenPrintDebugInfo) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.PrintBOCreateDestroyResult.set(true);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};

    testing::internal::CaptureStdout();
    auto localMemHelper = LocalMemoryHelper::get(defaultHwInfo->platform.eProductFamily);
    uint32_t handle = 0;

    auto ret = localMemHelper->createGemExt(drm.get(), &regionInfo[1], 1, 1024, handle);

    std::string output = testing::internal::GetCapturedStdout();
    std::string expectedOutput("Performing GEM_CREATE_EXT with { size: 1024, memory class: 1, memory instance: 0 }\nGEM_CREATE_EXT with EXT_MEMORY_REGIONS has returned: 0 BO-1 with size: 1024\n");
    EXPECT_EQ(expectedOutput, output);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
    EXPECT_EQ(0u, ret);
}

DG1TEST_F(LocalMemoryHelperTestsDg1, givenDg1WhenCreateGemExtWithDebugFlagThenPrintDebugInfo) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.PrintBOCreateDestroyResult.set(true);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMockProdDg1>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};

    testing::internal::CaptureStdout();
    auto localMemHelper = LocalMemoryHelper::get(defaultHwInfo->platform.eProductFamily);
    uint32_t handle = 0;

    auto ret = localMemHelper->createGemExt(drm.get(), &regionInfo[1], 1, 1024, handle);

    std::string output = testing::internal::GetCapturedStdout();
    std::string expectedOutput("Performing GEM_CREATE_EXT with { size: 1024, memory class: 1, memory instance: 0 }\nGEM_CREATE_EXT with EXT_SETPARAM has returned: 0 BO-1 with size: 1024\n");
    EXPECT_EQ(expectedOutput, output);
    EXPECT_EQ(2u, drm->ioctlCallsCount);
    EXPECT_EQ(0u, ret);
}

DG1TEST_F(LocalMemoryHelperTestsDg1, givenDg1AndMemoryRegionQuerySupportedWhenQueryingMemoryInfoThenMemoryInfoIsCreatedWithRegions) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    auto drm = std::make_unique<DrmMockProdDg1>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);

    drm->queryMemoryInfo();
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    auto memoryInfo = static_cast<MemoryInfoImpl *>(drm->getMemoryInfo());

    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_EQ(2u, memoryInfo->getDrmRegionInfos().size());
}
