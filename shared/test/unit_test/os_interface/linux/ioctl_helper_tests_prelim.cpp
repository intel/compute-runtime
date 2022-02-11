/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "third_party/uapi/prelim/drm/i915_drm.h"

using namespace NEO;

extern std::map<unsigned long, const char *> ioctlCodeStringMap;
extern std::map<int, const char *> ioctlParamCodeStringMap;
extern std::vector<uint8_t> getRegionInfo(const std::vector<MemoryRegion> &inputRegions);
extern std::vector<uint8_t> getEngineInfo(const std::vector<EngineCapabilities> &inputEngines);

struct IoctlPrelimHelperTests : ::testing::Test {
    IoctlHelperPrelim20 ioctlHelper{};
};

TEST_F(IoctlPrelimHelperTests, givenIoctlWhenParseToStringThenProperStringIsReturned) {
    for (auto &ioctlCodeString : ioctlCodeStringMap) {
        EXPECT_STREQ(ioctlHelper.getIoctlString(ioctlCodeString.first).c_str(), ioctlCodeString.second);
    }
    EXPECT_STREQ(ioctlHelper.getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_VM_BIND).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_BIND");
    EXPECT_STREQ(ioctlHelper.getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND");
    EXPECT_STREQ(ioctlHelper.getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH");
    EXPECT_STREQ(ioctlHelper.getIoctlString(PRELIM_DRM_IOCTL_I915_UUID_REGISTER).c_str(), "PRELIM_DRM_IOCTL_I915_UUID_REGISTER");
    EXPECT_STREQ(ioctlHelper.getIoctlString(PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER).c_str(), "PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER");
    EXPECT_STREQ(ioctlHelper.getIoctlString(PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN).c_str(), "PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN");
    EXPECT_STREQ(ioctlHelper.getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE).c_str(), "PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DRM_IOCTL_I915_GEM_MMAP_GTT).c_str(), "DRM_IOCTL_I915_GEM_MMAP_GTT");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DRM_IOCTL_I915_GEM_MMAP_OFFSET).c_str(), "DRM_IOCTL_I915_GEM_MMAP_OFFSET");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DRM_IOCTL_I915_GEM_VM_CREATE).c_str(), "DRM_IOCTL_I915_GEM_VM_CREATE");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DRM_IOCTL_I915_GEM_VM_DESTROY).c_str(), "DRM_IOCTL_I915_GEM_VM_DESTROY");
    EXPECT_STREQ(ioctlHelper.getIoctlString(DRM_IOCTL_I915_GEM_VM_DESTROY).c_str(), "DRM_IOCTL_I915_GEM_VM_DESTROY");
}

TEST_F(IoctlPrelimHelperTests, givenIoctlParamWhenParseToStringThenProperStringIsReturned) {
    for (auto &ioctlParamCodeString : ioctlParamCodeStringMap) {
        EXPECT_STREQ(ioctlHelper.getIoctlParamString(ioctlParamCodeString.first).c_str(), ioctlParamCodeString.second);
    }
    EXPECT_STREQ(ioctlHelper.getIoctlParamString(PRELIM_I915_PARAM_HAS_VM_BIND).c_str(), "PRELIM_I915_PARAM_HAS_VM_BIND");
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenTranslateToMemoryRegionsThenReturnSameData) {
    std::vector<MemoryRegion> expectedMemRegions(2);
    expectedMemRegions[0].region.memoryClass = PRELIM_I915_MEMORY_CLASS_SYSTEM;
    expectedMemRegions[0].region.memoryInstance = 0;
    expectedMemRegions[0].probedSize = 1024;
    expectedMemRegions[1].region.memoryClass = PRELIM_I915_MEMORY_CLASS_DEVICE;
    expectedMemRegions[1].region.memoryInstance = 0;
    expectedMemRegions[1].probedSize = 1024;

    auto regionInfo = getRegionInfo(expectedMemRegions);

    auto memRegions = ioctlHelper.translateToMemoryRegions(regionInfo);
    EXPECT_EQ(2u, memRegions.size());
    for (uint32_t i = 0; i < memRegions.size(); i++) {
        EXPECT_EQ(expectedMemRegions[i].region.memoryClass, memRegions[i].region.memoryClass);
        EXPECT_EQ(expectedMemRegions[i].region.memoryInstance, memRegions[i].region.memoryInstance);
        EXPECT_EQ(expectedMemRegions[i].probedSize, memRegions[i].probedSize);
        EXPECT_EQ(expectedMemRegions[i].unallocatedSize, memRegions[i].unallocatedSize);
    }
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenGetHwConfigIoctlValThenCorrectValueReturned) {
    EXPECT_EQ(static_cast<uint32_t>(PRELIM_DRM_I915_QUERY_HWCONFIG_TABLE), ioctlHelper.getHwConfigIoctlVal());
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenGetDirectSubmissionFlagThenCorrectValueReturned) {
    EXPECT_EQ(PRELIM_I915_CONTEXT_CREATE_FLAGS_ULLS, ioctlHelper.getDirectSubmissionFlag());
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenGetMemRegionsIoctlValThenCorrectValueReturned) {
    EXPECT_EQ(PRELIM_DRM_I915_QUERY_MEMORY_REGIONS, ioctlHelper.getMemRegionsIoctlVal());
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenGetEngineInfoIoctlValThenCorrectValueReturned) {
    EXPECT_EQ(PRELIM_DRM_I915_QUERY_ENGINE_INFO, ioctlHelper.getEngineInfoIoctlVal());
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenTranslateToEngineCapsThenReturnSameData) {
    std::vector<EngineCapabilities> expectedEngines(2);
    expectedEngines[0] = {{I915_ENGINE_CLASS_RENDER, 0}, 0};
    expectedEngines[1] = {{I915_ENGINE_CLASS_COPY, 1}, 0};

    auto engineInfo = getEngineInfo(expectedEngines);

    auto engines = ioctlHelper.translateToEngineCaps(engineInfo);
    EXPECT_EQ(2u, engines.size());
    for (uint32_t i = 0; i < engines.size(); i++) {
        EXPECT_EQ(expectedEngines[i].engine.engineClass, engines[i].engine.engineClass);
        EXPECT_EQ(expectedEngines[i].engine.engineInstance, engines[i].engine.engineInstance);
        EXPECT_EQ(expectedEngines[i].capabilities, engines[i].capabilities);
    }
}

TEST_F(IoctlPrelimHelperTests, givenPrelimsWhenGettingFlagForWaitUserFenceSoftThenProperFlagIsReturned) {
    EXPECT_EQ(PRELIM_I915_UFENCE_WAIT_SOFT, ioctlHelper.getWaitUserFenceSoftFlag());
}
