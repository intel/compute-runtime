/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gen12lp/hw_cmds_dg1.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/linux/drm_mock_prod_dg1.h"

using namespace NEO;

using IoctlHelperTestsDg1 = ::testing::Test;

DG1TEST_F(IoctlHelperTestsDg1, givenDg1WhenCreateGemExtThenReturnCorrectValue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}};
    auto ret = ioctlHelper->createGemExt(memClassInstance, 1024, handle, {}, -1);

    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, handle);
    EXPECT_EQ(1u, drm->numRegions);
    EXPECT_EQ(1024u, drm->createExt.size);
    EXPECT_EQ(drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, drm->memRegions.memoryClass);
}

DG1TEST_F(IoctlHelperTestsDg1, givenDg1WithDrmTipWhenCreateGemExtWithDebugFlagThenPrintDebugInfo) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.PrintBOCreateDestroyResult.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmTipMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->ioctlCallsCount = 0;

    testing::internal::CaptureStdout();
    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}};
    auto ret = ioctlHelper->createGemExt(memClassInstance, 1024, handle, {}, -1);

    std::string output = testing::internal::GetCapturedStdout();
    std::string expectedOutput("Performing GEM_CREATE_EXT with { size: 1024, memory class: 1, memory instance: 0 }\nGEM_CREATE_EXT with EXT_MEMORY_REGIONS has returned: 0 BO-1 with size: 1024\n");
    EXPECT_EQ(expectedOutput, output);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
    EXPECT_EQ(0u, ret);
}

DG1TEST_F(IoctlHelperTestsDg1, givenDg1WhenCreateGemExtWithDebugFlagThenPrintDebugInfo) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.PrintBOCreateDestroyResult.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockProdDg1>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->ioctlCallsCount = 0;

    testing::internal::CaptureStdout();
    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}};
    auto ret = ioctlHelper->createGemExt(memClassInstance, 1024, handle, {}, -1);

    std::string output = testing::internal::GetCapturedStdout();
    std::string expectedOutput("Performing GEM_CREATE_EXT with { size: 1024, memory class: 1, memory instance: 0 }\nGEM_CREATE_EXT with EXT_MEMORY_REGIONS has returned: -1 BO-0 with size: 1024\nGEM_CREATE_EXT with EXT_SETPARAM has returned: 0 BO-1 with size: 1024\n");
    EXPECT_EQ(expectedOutput, output);
    EXPECT_EQ(2u, drm->ioctlCallsCount);
    EXPECT_EQ(0u, ret);
}

DG1TEST_F(IoctlHelperTestsDg1, givenDg1AndMemoryRegionQuerySupportedWhenQueryingMemoryInfoThenMemoryInfoIsCreatedWithRegions) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(1);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    auto drm = std::make_unique<DrmMockProdDg1>(*executionEnvironment->rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drm);
    drm->ioctlCallsCount = 0;

    drm->queryMemoryInfo();
    EXPECT_EQ(2u, drm->ioctlCallsCount);

    auto memoryInfo = drm->getMemoryInfo();

    ASSERT_NE(nullptr, memoryInfo);
    EXPECT_EQ(2u, memoryInfo->getDrmRegionInfos().size());
}

DG1TEST_F(IoctlHelperTestsDg1, whenGettingIoctlRequestStringThenProperStringIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockProdDg1>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto &ioctlHelper = *drm->getIoctlHelper();
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::Getparam).c_str(), "DRM_IOCTL_I915_GETPARAM");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemExecbuffer2).c_str(), "DRM_IOCTL_I915_GEM_EXECBUFFER2");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemWait).c_str(), "DRM_IOCTL_I915_GEM_WAIT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemClose).c_str(), "DRM_IOCTL_GEM_CLOSE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemUserptr).c_str(), "DRM_IOCTL_I915_GEM_USERPTR");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemCreate).c_str(), "DRM_IOCTL_I915_GEM_CREATE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemSetDomain).c_str(), "DRM_IOCTL_I915_GEM_SET_DOMAIN");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemSetTiling).c_str(), "DRM_IOCTL_I915_GEM_SET_TILING");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemGetTiling).c_str(), "DRM_IOCTL_I915_GEM_GET_TILING");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemContextDestroy).c_str(), "DRM_IOCTL_I915_GEM_CONTEXT_DESTROY");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::RegRead).c_str(), "DRM_IOCTL_I915_REG_READ");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GetResetStats).c_str(), "DRM_IOCTL_I915_GET_RESET_STATS");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemContextGetparam).c_str(), "DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemContextSetparam).c_str(), "DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::Query).c_str(), "DRM_IOCTL_I915_QUERY");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::PrimeFdToHandle).c_str(), "DRM_IOCTL_PRIME_FD_TO_HANDLE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::PrimeHandleToFd).c_str(), "DRM_IOCTL_PRIME_HANDLE_TO_FD");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemContextCreateExt).c_str(), "DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemMmapOffset).c_str(), "DRM_IOCTL_I915_GEM_MMAP_OFFSET");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemVmCreate).c_str(), "DRM_IOCTL_I915_GEM_VM_CREATE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::GemVmDestroy).c_str(), "DRM_IOCTL_I915_GEM_VM_DESTROY");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::DG1GemCreateExt).c_str(), "DRM_IOCTL_I915_GEM_CREATE_EXT");
}

DG1TEST_F(IoctlHelperTestsDg1, whenGettingIoctlRequestValueThenPropertValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockProdDg1>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto &ioctlHelper = *drm->getIoctlHelper();
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::Getparam), static_cast<unsigned int>(DRM_IOCTL_I915_GETPARAM));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemExecbuffer2), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_EXECBUFFER2));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemWait), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_WAIT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemClose), static_cast<unsigned int>(DRM_IOCTL_GEM_CLOSE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemUserptr), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_USERPTR));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemCreate), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CREATE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemSetDomain), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_SET_DOMAIN));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemSetTiling), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_SET_TILING));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemGetTiling), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_GET_TILING));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemContextDestroy), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CONTEXT_DESTROY));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::RegRead), static_cast<unsigned int>(DRM_IOCTL_I915_REG_READ));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GetResetStats), static_cast<unsigned int>(DRM_IOCTL_I915_GET_RESET_STATS));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemContextGetparam), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemContextSetparam), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::Query), static_cast<unsigned int>(DRM_IOCTL_I915_QUERY));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::PrimeFdToHandle), static_cast<unsigned int>(DRM_IOCTL_PRIME_FD_TO_HANDLE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::PrimeHandleToFd), static_cast<unsigned int>(DRM_IOCTL_PRIME_HANDLE_TO_FD));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemContextCreateExt), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemMmapOffset), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_MMAP_OFFSET));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemVmCreate), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_VM_CREATE));
    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemVmDestroy), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_VM_DESTROY));

    EXPECT_EQ(ioctlHelper.getIoctlRequestValue(DrmIoctl::DG1GemCreateExt), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CREATE_EXT));
    EXPECT_NE(ioctlHelper.getIoctlRequestValue(DrmIoctl::GemCreateExt), static_cast<unsigned int>(DRM_IOCTL_I915_GEM_CREATE_EXT));
}

DG1TEST_F(IoctlHelperTestsDg1, whenGettingDrmParamStringThenProperStringIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMockProdDg1>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto &ioctlHelper = *drm->getIoctlHelper();
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamChipsetId).c_str(), "I915_PARAM_CHIPSET_ID");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamRevision).c_str(), "I915_PARAM_REVISION");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamHasExecSoftpin).c_str(), "I915_PARAM_HAS_EXEC_SOFTPIN");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamHasPooledEu).c_str(), "I915_PARAM_HAS_POOLED_EU");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamHasScheduler).c_str(), "I915_PARAM_HAS_SCHEDULER");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamEuTotal).c_str(), "I915_PARAM_EU_TOTAL");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamSubsliceTotal).c_str(), "I915_PARAM_SUBSLICE_TOTAL");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamMinEuInPool).c_str(), "I915_PARAM_MIN_EU_IN_POOL");
    EXPECT_STREQ(ioctlHelper.getDrmParamString(DrmParam::ParamCsTimestampFrequency).c_str(), "I915_PARAM_CS_TIMESTAMP_FREQUENCY");
}
