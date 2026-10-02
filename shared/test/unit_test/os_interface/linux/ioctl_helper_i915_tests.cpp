/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_os_time_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/os_interface/linux/drm_mock_impl.h"

namespace NEO {
bool getGpuTimeSplit(Drm &drm, uint64_t *timestamp);
bool getGpuTime32(Drm &drm, uint64_t *timestamp);
bool getGpuTime36(Drm &drm, uint64_t *timestamp);
} // namespace NEO

using namespace NEO;

struct MockIoctlHelperI915 : IoctlHelperUpstream {
    using IoctlHelperUpstream::getGpuTime;
    using IoctlHelperUpstream::initializeGetGpuTimeFunction;
    using IoctlHelperUpstream::IoctlHelperUpstream;
    using IoctlHelperUpstream::translateToMemoryRegions;
};

namespace {
std::vector<uint64_t> getRegionInfo(const std::vector<MemoryRegion> &inputRegions) {
    auto inputSize = static_cast<uint32_t>(inputRegions.size());
    int length = sizeof(drm_i915_query_memory_regions) + inputSize * sizeof(drm_i915_memory_region_info);
    auto data = std::vector<uint64_t>(Math::divideAndRoundUp(length, sizeof(uint64_t)));
    auto memoryRegions = reinterpret_cast<drm_i915_query_memory_regions *>(data.data());
    memoryRegions->num_regions = inputSize;

    for (uint32_t i = 0; i < inputSize; i++) {
        memoryRegions->regions[i].region.memory_class = inputRegions[i].region.memoryClass;
        memoryRegions->regions[i].region.memory_instance = inputRegions[i].region.memoryInstance;
        memoryRegions->regions[i].probed_size = inputRegions[i].probedSize;
        memoryRegions->regions[i].unallocated_size = inputRegions[i].unallocatedSize;
        memoryRegions->regions[i].probed_cpu_visible_size = inputRegions[i].cpuVisibleSize;
    }
    return data;
}
} // namespace

struct IoctlHelperI915WithUnsupportedDistances : IoctlHelperUpstream {
    using IoctlHelperUpstream::IoctlHelperUpstream;

    int queryDistances(std::vector<QueryItem> &queryItems, std::vector<DistanceInfo> &distanceInfos) override {
        for (auto &query : queryItems) {
            query.length = -EINVAL;
        }
        return 0;
    }
};

struct IoctlHelperI915Test : ::testing::Test {
    MockExecutionEnvironment executionEnvironment{};
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>(*executionEnvironment.rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drm};
};

HWTEST2_F(IoctlHelperI915Test, whenGettingIfImmediateVmBindIsRequiredThenFalseIsReturned, IsAtMostXeCore) {
    EXPECT_FALSE(ioctlHelper.isImmediateVmBindRequired());
}

HWTEST2_F(IoctlHelperI915Test, whenGettingIfSmallBarConfigIsAllowedThenTrueIsReturned, IsAtMostXeCore) {
    EXPECT_TRUE(ioctlHelper.isSmallBarConfigAllowed());
}

HWTEST2_F(IoctlHelperI915Test, whenGettingEuStallMaxReportsThenZeroIsReturned, IsAtMostXeCore) {
    EXPECT_EQ(0, ioctlHelper.getEuStallMaxReportsPerXeCore());
}

HWTEST2_F(IoctlHelperI915Test, whenQueryDeviceParamsIsCalledThenFalseIsReturned, IsAtMostXeCore) {
    uint32_t moduleId = 0;
    uint16_t serverType = 0;
    EXPECT_FALSE(ioctlHelper.queryDeviceParams(&moduleId, &serverType));
}

HWTEST2_F(IoctlHelperI915Test, whenQueryDeviceCapsIsCalledThenNulloptIsReturned, IsAtMostXeCore) {
    EXPECT_EQ(ioctlHelper.queryDeviceCaps(), std::nullopt);
}

HWTEST2_F(IoctlHelperI915Test, whenGetFdFromVmExportIsCalledThenFalseIsReturned, IsAtMostXeCore) {
    uint32_t vmId = 0, flags = 0;
    int32_t fd = 0;
    EXPECT_FALSE(ioctlHelper.getFdFromVmExport(vmId, flags, &fd));
}

HWTEST2_F(IoctlHelperI915Test, whenSetupIpVersionIsCalledThenIpVersionIsCorrect, IsAtMostXeCore) {
    auto &hwInfo = *drm->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &compilerProductHelper = drm->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    auto config = compilerProductHelper.getHwIpVersion(hwInfo);

    ioctlHelper.setupIpVersion();
    EXPECT_EQ(config, hwInfo.ipVersion.value);
}

HWTEST2_F(IoctlHelperI915Test, whenGettingGpuTimeThenSucceeds, IsAtMostXeCore) {
    auto drmTime = std::make_unique<DrmMockTime>(mockFd, *executionEnvironment.rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drmTime};
    ASSERT_TRUE(ioctlHelper.initialize());

    uint64_t time = 0;
    EXPECT_TRUE(getGpuTime32(*drmTime, &time));
    EXPECT_NE(0ULL, time);
    EXPECT_TRUE(getGpuTime36(*drmTime, &time));
    EXPECT_NE(0ULL, time);
    EXPECT_TRUE(getGpuTimeSplit(*drmTime, &time));
    EXPECT_NE(0ULL, time);
}

HWTEST2_F(IoctlHelperI915Test, givenInvalidDrmWhenGettingGpuTimeThenFails, IsAtMostXeCore) {
    auto drmFail = std::make_unique<DrmMockFail>(*executionEnvironment.rootDeviceEnvironments[0]);
    IoctlHelperUpstream ioctlHelper{*drmFail};
    ASSERT_TRUE(ioctlHelper.initialize());

    uint64_t time = 0;
    EXPECT_FALSE(getGpuTime32(*drmFail, &time));
    EXPECT_FALSE(getGpuTime36(*drmFail, &time));
    EXPECT_FALSE(getGpuTimeSplit(*drmFail, &time));
}

HWTEST2_F(IoctlHelperI915Test, whenInitializingGetGpuTimeFunctionThenFunctionMatchingSupportedTimestampReadIsSelected, IsAtMostXeCore) {
    auto drmCustom = DrmMockCustom::create(*executionEnvironment.rootDeviceEnvironments[0]);
    MockIoctlHelperI915 ioctlHelper{*drmCustom};
    ASSERT_TRUE(ioctlHelper.initialize());
    EXPECT_EQ(ioctlHelper.getGpuTime, &getGpuTime36);

    drmCustom->ioctlRes = -1;
    ioctlHelper.initializeGetGpuTimeFunction();
    EXPECT_EQ(ioctlHelper.getGpuTime, &getGpuTime32);

    DrmMockCustom::IoctlResExt ioctlToPass = {1, 0};
    drmCustom->reset();
    drmCustom->ioctlRes = -1;
    drmCustom->ioctlResExt = &ioctlToPass; // 2nd ioctl is successful
    ioctlHelper.initializeGetGpuTimeFunction();
    EXPECT_EQ(ioctlHelper.getGpuTime, &getGpuTimeSplit);
    drmCustom->ioctlResExt = &drmCustom->none;
}

HWTEST2_F(IoctlHelperI915Test, givenInitializeGetGpuTimeFunctionNotCalledWhenSetGpuCpuTimesIsCalledThenFalseIsReturned, IsAtMostXeCore) {
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    auto drmCustom = DrmMockCustom::create(rootDeviceEnvironment);
    IoctlHelperUpstream ioctlHelper{*drmCustom};

    drmCustom->ioctlRes = -1;
    TimeStampData gpuCpuTime{};
    std::unique_ptr<MockOSTimeLinux> osTime = MockOSTimeLinux::create(*rootDeviceEnvironment.osInterface);
    EXPECT_FALSE(ioctlHelper.setGpuCpuTimes(&gpuCpuTime, osTime.get()));
}

HWTEST2_F(IoctlHelperI915Test, givenMemoryInfoWithoutDeviceMemoryWhenCreatingEngineInfoThenMultitileIsNotUsed, IsAtMostXeCore) {
    auto drmEngine = std::make_unique<DrmMockEngine>(*executionEnvironment.rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drmEngine);
    drmEngine->ioctlCallsCount = 0;
    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0}};
    drmEngine->memoryInfo.reset(new MemoryInfo(memRegions, *drmEngine));
    drmEngine->memoryInfoQueried = true;
    EXPECT_TRUE(drmEngine->queryEngineInfo());
    EXPECT_EQ(2u, drmEngine->ioctlCallsCount);

    auto engineInfo = drmEngine->getEngineInfo();
    ASSERT_NE(nullptr, engineInfo);
    std::vector<EngineClassInstance> engines;
    engineInfo->getListOfEnginesOnATile(0, engines);
    auto totalEnginesCount = engineInfo->getEngineInfos().size();
    EXPECT_TRUE(engineInfo->hasEngines());
    EXPECT_EQ(totalEnginesCount, engines.size());
}

HWTEST2_F(IoctlHelperI915Test, givenDeviceMemoryAndUnsupportedDistancesQueryWhenCreatingEngineInfoThenMultitileIsNotUsed, IsAtMostXeCore) {
    auto drmEngine = std::make_unique<DrmMockEngine>(*executionEnvironment.rootDeviceEnvironments[0]);
    ASSERT_NE(nullptr, drmEngine);
    drmEngine->ioctlHelper = std::make_unique<IoctlHelperI915WithUnsupportedDistances>(*drmEngine);
    drmEngine->ioctlCallsCount = 0;
    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0}};
    drmEngine->memoryInfo.reset(new MemoryInfo(memRegions, *drmEngine));
    drmEngine->memoryInfoQueried = true;
    EXPECT_TRUE(drmEngine->queryEngineInfo());
    EXPECT_EQ(2u, drmEngine->ioctlCallsCount);

    auto engineInfo = drmEngine->getEngineInfo();
    ASSERT_NE(nullptr, engineInfo);
    std::vector<EngineClassInstance> engines;
    engineInfo->getListOfEnginesOnATile(0, engines);
    auto totalEnginesCount = engineInfo->getEngineInfos().size();
    EXPECT_EQ(totalEnginesCount, engines.size());
}

HWTEST2_F(IoctlHelperI915Test, givenMemoryRegionQuerySupportedWhenQueryingMemoryInfoThenMemoryInfoIsCreated, IsAtMostXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    auto drmTip = std::make_unique<DrmTipMock>(*executionEnvironment.rootDeviceEnvironments[0]);
    drmTip->ioctlCallsCount = 0;
    drmTip->memoryInfoQueried = false;
    drmTip->queryMemoryInfo();

    EXPECT_EQ(2u, drmTip->ioctlCallsCount);
    EXPECT_NE(nullptr, drmTip->getMemoryInfo());
}

HWTEST2_F(IoctlHelperI915Test, givenMemoryRegionQueryNotSupportedWhenQueryingMemoryInfoThenMemoryInfoIsNotCreated, IsAtMostXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    auto drmTip = std::make_unique<DrmTipMock>(*executionEnvironment.rootDeviceEnvironments[0]);
    drmTip->ioctlCallsCount = 0;
    drmTip->i915QuerySuccessCount = 0;
    drmTip->memoryInfoQueried = false;
    drmTip->queryMemoryInfo();

    EXPECT_EQ(nullptr, drmTip->getMemoryInfo());
    EXPECT_EQ(1u, drmTip->ioctlCallsCount);
}

HWTEST2_F(IoctlHelperI915Test, givenMemoryRegionQueryWhenQueryingFailsThenMemoryInfoIsNotCreated, IsAtMostXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableLocalMemory.set(1);
    auto drmTip = std::make_unique<DrmTipMock>(*executionEnvironment.rootDeviceEnvironments[0]);
    drmTip->ioctlCallsCount = 0;
    drmTip->queryMemoryRegionInfoSuccessCount = 0;
    drmTip->memoryInfoQueried = false;
    drmTip->queryMemoryInfo();
    EXPECT_EQ(nullptr, drmTip->getMemoryInfo());
    EXPECT_EQ(1u, drmTip->ioctlCallsCount);

    drmTip = std::make_unique<DrmTipMock>(*executionEnvironment.rootDeviceEnvironments[0]);
    drmTip->ioctlCallsCount = 0;
    drmTip->i915QuerySuccessCount = 1;
    drmTip->memoryInfoQueried = false;
    drmTip->queryMemoryInfo();
    EXPECT_EQ(nullptr, drmTip->getMemoryInfo());
    EXPECT_EQ(2u, drmTip->ioctlCallsCount);

    drmTip = std::make_unique<DrmTipMock>(*executionEnvironment.rootDeviceEnvironments[0]);
    drmTip->ioctlCallsCount = 0;
    drmTip->queryMemoryRegionInfoSuccessCount = 1;
    drmTip->memoryInfoQueried = false;
    drmTip->queryMemoryInfo();
    EXPECT_EQ(nullptr, drmTip->getMemoryInfo());
    EXPECT_EQ(2u, drmTip->ioctlCallsCount);
}

HWTEST2_F(IoctlHelperI915Test, givenMemoryRegionsQueryDataWhenTranslatingToMemoryRegionsThenAllRegionFieldsAreDecoded, IsAtMostXeCore) {
    std::vector<MemoryRegion> expectedMemRegions(2);
    expectedMemRegions[0].region.memoryClass = drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM;
    expectedMemRegions[0].region.memoryInstance = 0;
    expectedMemRegions[0].probedSize = 1024;
    expectedMemRegions[0].unallocatedSize = 512;
    expectedMemRegions[0].cpuVisibleSize = 1024;
    expectedMemRegions[1].region.memoryClass = drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE;
    expectedMemRegions[1].region.memoryInstance = 1;
    expectedMemRegions[1].probedSize = 2048;
    expectedMemRegions[1].unallocatedSize = 1536;
    expectedMemRegions[1].cpuVisibleSize = 256;

    MockIoctlHelperI915 mockIoctlHelper{*drm};
    auto memRegions = mockIoctlHelper.translateToMemoryRegions(getRegionInfo(expectedMemRegions));

    ASSERT_EQ(expectedMemRegions.size(), memRegions.size());
    for (uint32_t i = 0; i < memRegions.size(); i++) {
        EXPECT_EQ(expectedMemRegions[i].region.memoryClass, memRegions[i].region.memoryClass);
        EXPECT_EQ(expectedMemRegions[i].region.memoryInstance, memRegions[i].region.memoryInstance);
        EXPECT_EQ(expectedMemRegions[i].probedSize, memRegions[i].probedSize);
        EXPECT_EQ(expectedMemRegions[i].unallocatedSize, memRegions[i].unallocatedSize);
        EXPECT_EQ(expectedMemRegions[i].cpuVisibleSize, memRegions[i].cpuVisibleSize);
    }
}
