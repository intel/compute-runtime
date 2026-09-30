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
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper_with_capture.h"
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

using DrmTest = ::testing::Test;

HWTEST2_F(DrmTest, GivenDrmWhenAskedForPreemptionThenCorrectValueReturned, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    pDrm->storedRetVal = 0;
    pDrm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    pDrm->checkPreemptionSupport();
    EXPECT_TRUE(pDrm->isPreemptionSupported());

    pDrm->storedPreemptionSupport = 0;
    pDrm->checkPreemptionSupport();
    EXPECT_FALSE(pDrm->isPreemptionSupported());

    pDrm->storedRetVal = -1;
    pDrm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    pDrm->checkPreemptionSupport();
    EXPECT_FALSE(pDrm->isPreemptionSupported());

    pDrm->storedPreemptionSupport = 0;
    pDrm->checkPreemptionSupport();
    EXPECT_FALSE(pDrm->isPreemptionSupported());

    delete pDrm;
}

HWTEST2_F(DrmTest, givenDrmPreemptionEnabledAndLowPriorityEngineWhenCreatingOsContextThenCallSetContextPriorityIoctl, IsAtMostXeCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.preemptionSupported = false;

    OsContextLinux osContext1(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext1.ensureContextInitialized();
    OsContextLinux osContext2(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::lowPriority}));
    osContext2.ensureContextInitialized();

    EXPECT_EQ(4u, drmMock.receivedContextParamRequestCount);

    drmMock.preemptionSupported = true;

    OsContextLinux osContext3(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext3.ensureContextInitialized();
    EXPECT_EQ(6u, drmMock.receivedContextParamRequestCount);

    OsContextLinux osContext4(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::lowPriority}));
    osContext4.ensureContextInitialized();
    EXPECT_EQ(9u, drmMock.receivedContextParamRequestCount);
    EXPECT_EQ(drmMock.storedDrmContextId, drmMock.receivedContextParamRequest.contextId);
    EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_PRIORITY), drmMock.receivedContextParamRequest.param);
    EXPECT_EQ(static_cast<uint64_t>(-1023), drmMock.receivedContextParamRequest.value);
    EXPECT_EQ(0u, drmMock.receivedContextParamRequest.size);
}

class DrmI915CreateContextTest : public ::testing::Test {
  public:
    void SetUp() override {
        executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        drm = std::make_unique<DrmMockWithCaptureHelper>(*executionEnvironment->rootDeviceEnvironments[0]);
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<MockExecutionEnvironment> executionEnvironment;
    std::unique_ptr<DrmMockWithCaptureHelper> drm;
};

HWTEST2_F(DrmI915CreateContextTest, givenVariousDirectSubmissionFlagSettingWhenCreateDrmContextIsCalledThenCorrectFlagsArePassedToIoctl, IsAtMostXeCore) {
    constexpr uint32_t directSubmissionFlag = (1u << 31);
    drm->getMockIoctlHelper()->directSubmissionFlag = directSubmissionFlag;
    uint32_t vmId = 0u;
    constexpr bool isCooperativeContextRequested = false;

    debugManager.flags.DirectSubmissionDrmContext.set(-1);
    drm->receivedContextCreateFlags = 0;
    drm->createDrmContext(vmId, true, isCooperativeContextRequested);
    EXPECT_EQ(directSubmissionFlag, drm->receivedContextCreateFlags);

    debugManager.flags.DirectSubmissionDrmContext.set(0);
    drm->receivedContextCreateFlags = 0;
    drm->createDrmContext(vmId, true, isCooperativeContextRequested);
    EXPECT_EQ(0u, drm->receivedContextCreateFlags);

    debugManager.flags.DirectSubmissionDrmContext.set(1);
    drm->receivedContextCreateFlags = 0;
    drm->createDrmContext(vmId, false, isCooperativeContextRequested);
    EXPECT_EQ(directSubmissionFlag, drm->receivedContextCreateFlags);
}

HWTEST2_F(DrmI915CreateContextTest, whenCreateDrmContextIsCalledThenExactlyOneContextCreationPathIsUsed, IsAtMostXeCore) {
    auto ioctlHelper = drm->getMockIoctlHelper();
    constexpr bool isDirectSubmissionRequested = false;

    for (auto isCooperativeContextRequested : {false, true}) {
        for (auto forceRunAloneContext : {-1, 0, 1}) {
            debugManager.flags.ForceRunAloneContext.set(forceRunAloneContext);
            for (auto createContextWithAccessCounters : {-1, 0, 1}) {
                debugManager.flags.CreateContextWithAccessCounters.set(createContextWithAccessCounters);
                for (auto vmId = 0u; vmId < 3; vmId++) {
                    ioctlHelper->createContextWithAccessCountersCalled = 0u;
                    ioctlHelper->createCooperativeContextCalled = 0u;
                    drm->ioctlCount.contextCreate = 0;

                    drm->createDrmContext(vmId, isDirectSubmissionRequested, isCooperativeContextRequested);

                    const bool expectAccessCounters = createContextWithAccessCounters > 0;
                    const bool cooperativeRequested = forceRunAloneContext == -1 ? isCooperativeContextRequested : forceRunAloneContext != 0;
                    const bool expectCooperative = !expectAccessCounters && cooperativeRequested;
                    EXPECT_EQ(expectAccessCounters ? 1u : 0u, ioctlHelper->createContextWithAccessCountersCalled);
                    EXPECT_EQ(expectCooperative ? 1u : 0u, ioctlHelper->createCooperativeContextCalled);
                    EXPECT_EQ((expectAccessCounters || expectCooperative) ? 0 : 1, drm->ioctlCount.contextCreate.load());
                }
            }
        }
    }
}

HWTEST2_F(DrmI915CreateContextTest, givenProgramDebuggingAndContextDebugSupportedWhenCreatingContextThenCooperativeFlagIsPassedToCreateDrmContextOnlyIfCCSEnginesArePresent, IsAtMostXeCore) {
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    drm->contextDebugSupported = true;
    drm->callBaseCreateDrmContext = false;

    executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->platform.eProductFamily = defaultHwInfo->platform.eProductFamily;

    OsContextLinux osContext(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}));
    osContext.ensureContextInitialized();

    EXPECT_NE(static_cast<uint32_t>(-1), drm->passedContextDebugId);
    if (executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo.CCSInfo.NumberOfCCSEnabled > 0) {
        EXPECT_TRUE(drm->capturedCooperativeContextRequest);
    } else {
    }

    OsContextLinux osContext2(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::cooperative}));
    osContext2.ensureContextInitialized();

    EXPECT_NE(static_cast<uint32_t>(-1), drm->passedContextDebugId);
    EXPECT_TRUE(drm->capturedCooperativeContextRequest);
}

HWTEST2_F(DrmI915CreateContextTest, givenProgramDebuggingModeAndContextDebugSupportedAndRegularEngineUsageWhenCreatingContextThenCooperativeFlagIsNotPassedInOfflineDebuggingMode, IsAtMostXeCore) {
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    drm->contextDebugSupported = true;
    drm->callBaseCreateDrmContext = false;

    OsContextLinux osContext(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}));
    osContext.ensureContextInitialized();

    EXPECT_NE(static_cast<uint32_t>(-1), drm->passedContextDebugId);

    if (executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo.CCSInfo.NumberOfCCSEnabled > 0) {
        EXPECT_TRUE(drm->capturedCooperativeContextRequest);
    } else {
        EXPECT_FALSE(drm->capturedCooperativeContextRequest);
    }

    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::offline);

    OsContextLinux osContext2(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}));
    osContext2.ensureContextInitialized();

    EXPECT_NE(static_cast<uint32_t>(-1), drm->passedContextDebugId);
    EXPECT_FALSE(drm->capturedCooperativeContextRequest);

    OsContextLinux osContext3(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::cooperative}));
    osContext3.ensureContextInitialized();

    EXPECT_NE(static_cast<uint32_t>(-1), drm->passedContextDebugId);
    EXPECT_TRUE(drm->capturedCooperativeContextRequest);
}
