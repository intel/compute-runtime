/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"

#include "test.h"

using namespace NEO;

extern int handlePrelimRequests(unsigned long request, void *arg, int ioctlRetVal);

class DrmPrelimMock : public DrmMock {
  public:
    DrmPrelimMock(RootDeviceEnvironment &rootDeviceEnvironment) : DrmPrelimMock(rootDeviceEnvironment, defaultHwInfo.get()) {}
    DrmPrelimMock(RootDeviceEnvironment &rootDeviceEnvironment, HardwareInfo *inputHwInfo) : DrmMock(rootDeviceEnvironment) {
        rootDeviceEnvironment.setHwInfo(inputHwInfo);
        rootDeviceEnvironment.getMutableHardwareInfo()->platform.eProductFamily = IGFX_UNKNOWN;
    }

    int ioctlRetVal = 0;

    void getPrelimVersion(std::string &prelimVersion) override {
        prelimVersion = "2.0";
    }

    int handleRemainingRequests(unsigned long request, void *arg) override {
        return handlePrelimRequests(request, arg, ioctlRetVal);
    }
};

TEST(IoctlHelperTestsPrelim, givenPrelimsWhenCreateGemExtThenReturnSuccess) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[0].probed_size = 8 * GB;
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};
    regionInfo[1].probed_size = 16 * GB;

    auto ioctlHelper = IoctlHelper::get(drm.get());
    uint32_t handle = 0;
    auto ret = ioctlHelper->createGemExt(drm.get(), &regionInfo[1], 1, 1024, handle);

    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST(IoctlHelperTestsPrelim, givenPrelimsWhenCreateGemExtWithDebugFlagThenPrintDebugInfo) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.PrintBOCreateDestroyResult.set(true);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    drm_i915_memory_region_info regionInfo[2] = {};
    regionInfo[0].region = {I915_MEMORY_CLASS_SYSTEM, 0};
    regionInfo[1].region = {I915_MEMORY_CLASS_DEVICE, 0};

    testing::internal::CaptureStdout();
    auto ioctlHelper = IoctlHelper::get(drm.get());
    uint32_t handle = 0;
    ioctlHelper->createGemExt(drm.get(), &regionInfo[1], 1, 1024, handle);

    std::string output = testing::internal::GetCapturedStdout();
    std::string expectedOutput("Performing GEM_CREATE_EXT with { size: 1024, param: 0x1000000010001, memory class: 1, memory instance: 0 }\nGEM_CREATE_EXT has returned: 0 BO-1 with size: 1024\n");
    EXPECT_EQ(expectedOutput, output);
}

TEST(IoctlHelperTestsPrelim, givenPrelimsWhenTranslateIfRequiredThenReturnSameData) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    auto *data = new uint8_t{};
    auto ioctlHelper = IoctlHelper::get(drm.get());
    auto ret = ioctlHelper->translateIfRequired(data, 1);
    EXPECT_EQ(ret.get(), data);
}

TEST(IoctlHelperTestsPrelim, givenPrelimsWhenCallIoctlThenProperIoctlRegistered) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm_i915_gem_context_create_ext arg{};
    auto ret = IoctlHelper::ioctl(drm.get(), DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT, &arg);
    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST(IoctlHelperTestsPrelim, givenPrelimsWhenClosAllocThenReturnCorrectRegion) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = IoctlHelper::get(drm.get());
    auto cacheRegion = ioctlHelper->closAlloc(drm.get());

    EXPECT_EQ(CacheRegion::Region1, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST(IoctlHelperTestsPrelim, givenPrelimsAndInvalidIoctlReturnValWhenClosAllocThenReturnNone) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->ioctlRetVal = -1;

    auto ioctlHelper = IoctlHelper::get(drm.get());
    auto cacheRegion = ioctlHelper->closAlloc(drm.get());

    EXPECT_EQ(CacheRegion::None, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST(IoctlHelperTestsPrelim, givenPrelimsWhenClosFreeThenReturnCorrectRegion) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = IoctlHelper::get(drm.get());
    auto cacheRegion = ioctlHelper->closFree(drm.get(), CacheRegion::Region2);

    EXPECT_EQ(CacheRegion::Region2, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST(IoctlHelperTestsPrelim, givenPrelimsAndInvalidIoctlReturnValWhenClosFreeThenReturnNone) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->ioctlRetVal = -1;

    auto ioctlHelper = IoctlHelper::get(drm.get());
    auto cacheRegion = ioctlHelper->closFree(drm.get(), CacheRegion::Region2);

    EXPECT_EQ(CacheRegion::None, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST(IoctlHelperTestsPrelim, givenPrelimsWhenClosAllocWaysThenReturnCorrectRegion) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    auto ioctlHelper = IoctlHelper::get(drm.get());
    auto numWays = ioctlHelper->closAllocWays(drm.get(), CacheRegion::Region2, 3, 10);

    EXPECT_EQ(10u, numWays);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST(IoctlHelperTestsPrelim, givenPrelimsAndInvalidIoctlReturnValWhenClosAllocWaysThenReturnNone) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->ioctlRetVal = -1;

    auto ioctlHelper = IoctlHelper::get(drm.get());
    auto numWays = ioctlHelper->closAllocWays(drm.get(), CacheRegion::Region2, 3, 10);

    EXPECT_EQ(0u, numWays);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST(IoctlHelperTestsPrelim, givenPrelimsWhenWaitUserFenceThenCorrectValueReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    uint64_t gpuAddress = 0x1020304000ull;
    uint64_t value = 0x98765ull;
    auto ioctlHelper = IoctlHelper::get(drm.get());
    for (uint32_t i = 0u; i < 4; i++) {
        auto ret = ioctlHelper->waitUserFence(drm.get(), 10u, gpuAddress, value, i, -1, 0u);
        EXPECT_EQ(0, ret);
    }
}

TEST(IoctlHelperTestsPrelim, givenPrelimsWhenGetHwConfigIoctlValThenCorrectValueReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);

    uint32_t ioctlVal = (1 << 16) | 6;
    EXPECT_EQ(ioctlVal, IoctlHelper::get(drm.get())->getHwConfigIoctlVal());
}
