/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/libult/linux/drm_query_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

extern int handlePrelimRequests(DrmIoctl request, void *arg, int ioctlRetVal, int queryDistanceIoctlRetVal);

class DrmPrelimMock : public DrmMock {
  public:
    DrmPrelimMock(RootDeviceEnvironment &rootDeviceEnvironment) : DrmPrelimMock(rootDeviceEnvironment, defaultHwInfo.get()) {}
    DrmPrelimMock(RootDeviceEnvironment &rootDeviceEnvironment, HardwareInfo *inputHwInfo) : DrmMock(rootDeviceEnvironment) {
        rootDeviceEnvironment.setHwInfo(inputHwInfo);
        rootDeviceEnvironment.getMutableHardwareInfo()->platform.eProductFamily = IGFX_UNKNOWN;
    }

    int ioctlRetVal = 0;
    int queryDistanceIoctlRetVal = 0;

    void getPrelimVersion(std::string &prelimVersion) override {
        prelimVersion = "2.0";
    }

    int handleRemainingRequests(DrmIoctl request, void *arg) override {
        return handlePrelimRequests(request, arg, ioctlRetVal, queryDistanceIoctlRetVal);
    }
};

class IoctlHelperPrelimFixture : public ::testing::Test {
  public:
    void SetUp() override {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        drm = std::make_unique<DrmPrelimMock>(*executionEnvironment->rootDeviceEnvironments[0]);
        drm->ioctlHelper = std::make_unique<IoctlHelperPrelim20>(*drm);
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<DrmPrelimMock> drm;
};

TEST(IoctlHelperPrelimTest, whenGettingVmBindAvailabilityThenProperValueIsReturnedBasedOnIoctlResult) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    IoctlHelperPrelim20 ioctlHelper{drm};

    for (auto &ioctlValue : {0, EINVAL}) {
        drm.context.vmBindQueryReturn = ioctlValue;
        for (auto &hasVmBind : ::testing::Bool()) {
            drm.context.vmBindQueryValue = hasVmBind;
            drm.context.vmBindQueryCalled = 0u;

            if (ioctlValue == 0) {
                EXPECT_EQ(hasVmBind, ioctlHelper.isVmBindAvailable());
            } else {
                EXPECT_FALSE(ioctlHelper.isVmBindAvailable());
            }
            EXPECT_EQ(1u, drm.context.vmBindQueryCalled);
        }
    }
}

TEST(IoctlHelperPrelimTest, whenVmBindIsCalledThenProperValueIsReturnedBasedOnIoctlResult) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    IoctlHelperPrelim20 ioctlHelper{drm};

    VmBindParams vmBindParams{};

    for (auto &ioctlValue : {0, EINVAL}) {
        drm.context.vmBindReturn = ioctlValue;
        drm.context.vmBindCalled = 0u;
        EXPECT_EQ(ioctlValue, ioctlHelper.vmBind(vmBindParams));
        EXPECT_EQ(1u, drm.context.vmBindCalled);
    }
}

TEST(IoctlHelperPrelimTest, whenVmUnbindIsCalledThenProperValueIsReturnedBasedOnIoctlResult) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmQueryMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    IoctlHelperPrelim20 ioctlHelper{drm};

    VmBindParams vmBindParams{};

    for (auto &ioctlValue : {0, EINVAL}) {
        drm.context.vmUnbindReturn = ioctlValue;
        drm.context.vmUnbindCalled = 0u;
        EXPECT_EQ(ioctlValue, ioctlHelper.vmUnbind(vmBindParams));
        EXPECT_EQ(1u, drm.context.vmUnbindCalled);
    }
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenCreateGemExtThenReturnSuccess) {
    drm->ioctlCallsCount = 0;
    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}};
    auto ret = ioctlHelper->createGemExt(memClassInstance, 1024, handle, {});

    EXPECT_EQ(1u, handle);
    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenCreateGemExtWithDebugFlagThenPrintDebugInfo) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.PrintBOCreateDestroyResult.set(true);

    testing::internal::CaptureStdout();
    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t handle = 0;
    MemRegionsVec memClassInstance = {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}};
    ioctlHelper->createGemExt(memClassInstance, 1024, handle, {});

    std::string output = testing::internal::GetCapturedStdout();
    std::string expectedOutput("Performing GEM_CREATE_EXT with { size: 1024, param: 0x1000000010001, memory class: 1, memory instance: 0 }\nGEM_CREATE_EXT has returned: 0 BO-1 with size: 1024\n");
    EXPECT_EQ(expectedOutput, output);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenCallIoctlThenProperIoctlRegistered) {
    GemContextCreateExt arg{};
    drm->ioctlCallsCount = 0;
    auto ret = drm->ioctlHelper->ioctl(DrmIoctl::GemContextCreateExt, &arg);
    EXPECT_EQ(0u, ret);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenClosAllocThenReturnCorrectRegion) {
    drm->ioctlCallsCount = 0;
    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closAlloc();

    EXPECT_EQ(CacheRegion::Region1, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsAndInvalidIoctlReturnValWhenClosAllocThenReturnNone) {
    drm->ioctlRetVal = -1;
    drm->ioctlCallsCount = 0;
    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closAlloc();

    EXPECT_EQ(CacheRegion::None, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenClosFreeThenReturnCorrectRegion) {
    auto ioctlHelper = drm->getIoctlHelper();
    drm->ioctlCallsCount = 0;
    auto cacheRegion = ioctlHelper->closFree(CacheRegion::Region2);

    EXPECT_EQ(CacheRegion::Region2, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsAndInvalidIoctlReturnValWhenClosFreeThenReturnNone) {
    drm->ioctlRetVal = -1;
    drm->ioctlCallsCount = 0;

    auto ioctlHelper = drm->getIoctlHelper();
    auto cacheRegion = ioctlHelper->closFree(CacheRegion::Region2);

    EXPECT_EQ(CacheRegion::None, cacheRegion);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenClosAllocWaysThenReturnCorrectRegion) {
    drm->ioctlCallsCount = 0;
    auto ioctlHelper = drm->getIoctlHelper();
    auto numWays = ioctlHelper->closAllocWays(CacheRegion::Region2, 3, 10);

    EXPECT_EQ(10u, numWays);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsAndInvalidIoctlReturnValWhenClosAllocWaysThenReturnNone) {
    drm->ioctlRetVal = -1;
    drm->ioctlCallsCount = 0;

    auto ioctlHelper = drm->getIoctlHelper();
    auto numWays = ioctlHelper->closAllocWays(CacheRegion::Region2, 3, 10);

    EXPECT_EQ(0u, numWays);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenWaitUserFenceThenCorrectValueReturned) {
    uint64_t gpuAddress = 0x1020304000ull;
    uint64_t value = 0x98765ull;
    auto ioctlHelper = drm->getIoctlHelper();
    for (uint32_t i = 0u; i < 4; i++) {
        auto ret = ioctlHelper->waitUserFence(10u, gpuAddress, value, i, -1, 0u);
        EXPECT_EQ(0, ret);
    }
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemAdviseFailsThenDontUpdateMemAdviceFlags) {
    drm->ioctlCallsCount = 0;
    drm->ioctlRetVal = -1;

    MockBufferObject bo(drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);
    allocation.bufferObjects[0] = &bo;

    MemAdviseFlags memAdviseFlags{};
    memAdviseFlags.non_atomic = 1;

    allocation.setMemAdvise(drm.get(), memAdviseFlags);

    EXPECT_EQ(1u, drm->ioctlCallsCount);
    EXPECT_NE(memAdviseFlags.memadvise_flags, allocation.enabledMemAdviseFlags.memadvise_flags);
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemAdviseWithNonAtomicIsCalledThenUpdateTheCorrespondingVmAdviceForBufferObject) {
    MockBufferObject bo(drm.get(), 3, 0, 0, 1);
    drm->ioctlCallsCount = 0;
    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);
    allocation.bufferObjects[0] = &bo;

    MemAdviseFlags memAdviseFlags{};

    for (auto nonAtomic : {true, false}) {
        memAdviseFlags.non_atomic = nonAtomic;

        EXPECT_TRUE(allocation.setMemAdvise(drm.get(), memAdviseFlags));
        EXPECT_EQ(memAdviseFlags.memadvise_flags, allocation.enabledMemAdviseFlags.memadvise_flags);
    }
    EXPECT_EQ(2u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemAdviseWithDevicePreferredLocationIsCalledThenUpdateTheCorrespondingVmAdviceForBufferObject) {
    drm->ioctlCallsCount = 0;
    MockBufferObject bo(drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);
    allocation.bufferObjects[0] = &bo;

    MemAdviseFlags memAdviseFlags{};

    for (auto devicePreferredLocation : {true, false}) {
        memAdviseFlags.device_preferred_location = devicePreferredLocation;

        EXPECT_TRUE(allocation.setMemAdvise(drm.get(), memAdviseFlags));
        EXPECT_EQ(memAdviseFlags.memadvise_flags, allocation.enabledMemAdviseFlags.memadvise_flags);
    }
    EXPECT_EQ(2u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemPrefetchSucceedsThenReturnTrue) {
    MockBufferObject bo(drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);
    allocation.bufferObjects[0] = &bo;

    drm->ioctlRetVal = 0;
    EXPECT_TRUE(allocation.setMemPrefetch(drm.get(), 0));
}

TEST_F(IoctlHelperPrelimFixture, givenDrmAllocationWhenSetMemPrefetchFailsThenReturnFalse) {
    MockBufferObject bo(drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(AllocationType::BUFFER, MemoryPool::LocalMemory);
    allocation.bufferObjects[0] = &bo;

    drm->ioctlRetVal = EINVAL;
    EXPECT_FALSE(allocation.setMemPrefetch(drm.get(), 0));
}

TEST_F(IoctlHelperPrelimFixture, givenVariousDirectSubmissionFlagSettingWhenCreateDrmContextIsCalledThenCorrectFlagsArePassedToIoctl) {
    DebugManagerStateRestore stateRestore;
    uint32_t vmId = 0u;
    constexpr bool isCooperativeContextRequested = false;
    bool isDirectSubmissionRequested{};
    uint32_t ioctlVal = (1u << 31);

    DebugManager.flags.DirectSubmissionDrmContext.set(-1);
    drm->receivedContextCreateFlags = 0;
    isDirectSubmissionRequested = true;
    drm->createDrmContext(vmId, isDirectSubmissionRequested, isCooperativeContextRequested);
    EXPECT_EQ(ioctlVal, drm->receivedContextCreateFlags);

    DebugManager.flags.DirectSubmissionDrmContext.set(0);
    drm->receivedContextCreateFlags = 0;
    isDirectSubmissionRequested = true;
    drm->createDrmContext(vmId, isDirectSubmissionRequested, isCooperativeContextRequested);
    EXPECT_EQ(0u, drm->receivedContextCreateFlags);

    DebugManager.flags.DirectSubmissionDrmContext.set(1);
    drm->receivedContextCreateFlags = 0;
    isDirectSubmissionRequested = false;
    drm->createDrmContext(vmId, isDirectSubmissionRequested, isCooperativeContextRequested);
    EXPECT_EQ(ioctlVal, drm->receivedContextCreateFlags);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimsWhenQueryDistancesThenCorrectDistanceSet) {
    auto ioctlHelper = drm->getIoctlHelper();
    std::vector<DistanceInfo> distances(3);
    distances[0].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassRender)), 0};
    distances[0].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0};
    distances[1].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassRender)), 1};
    distances[1].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1};
    distances[2].engine = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassCopy)), 4};
    distances[2].region = {drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2};
    std::vector<QueryItem> queryItems(distances.size());
    auto ret = ioctlHelper->queryDistances(queryItems, distances);
    EXPECT_EQ(0u, ret);
    EXPECT_EQ(0, distances[0].distance);
    EXPECT_EQ(0, distances[1].distance);
    EXPECT_EQ(100, distances[2].distance);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimWhenQueryEngineInfoWithDeviceMemoryThenDistancesUsedAndMultileValuesSet) {
    drm->ioctlCallsCount = 0;
    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    EXPECT_TRUE(drm->queryEngineInfo());
    EXPECT_EQ(3u, drm->ioctlCallsCount);
    auto hwInfo = drm->getRootDeviceEnvironment().getHardwareInfo();
    auto engineInfo = drm->getEngineInfo();

    auto &multiTileArchInfo = const_cast<GT_MULTI_TILE_ARCH_INFO &>(hwInfo->gtSystemInfo.MultiTileArchInfo);
    EXPECT_TRUE(multiTileArchInfo.IsValid);
    EXPECT_EQ(3, multiTileArchInfo.TileCount);
    EXPECT_EQ(7, multiTileArchInfo.TileMask);

    EXPECT_EQ(1024u, drm->memoryInfo->getMemoryRegionSize(1));
    EXPECT_EQ(1024u, drm->memoryInfo->getMemoryRegionSize(2));
    EXPECT_EQ(0u, drm->memoryInfo->getMemoryRegionSize(4));

    std::vector<EngineClassInstance> engines;
    engineInfo->getListOfEnginesOnATile(0u, engines);
    EXPECT_EQ(3u, engines.size());

    engines.clear();
    engineInfo->getListOfEnginesOnATile(1u, engines);
    EXPECT_EQ(3u, engines.size());
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimWhenQueryEngineInfoThenCorrectCCSFlagsSet) {
    drm->ioctlCallsCount = 0;
    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    EXPECT_TRUE(drm->queryEngineInfo());
    EXPECT_EQ(3u, drm->ioctlCallsCount);
    auto hwInfo = drm->getRootDeviceEnvironment().getHardwareInfo();
    auto ccsInfo = hwInfo->gtSystemInfo.CCSInfo;
    EXPECT_TRUE(ccsInfo.IsValid);
    EXPECT_EQ(1u, ccsInfo.NumberOfCCSEnabled);
    EXPECT_EQ(1u, ccsInfo.Instances.CCSEnableMask);
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimWhenSysmanQueryEngineInfoThenAdditionalEnginesUsed) {
    drm->ioctlCallsCount = 0;
    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    EXPECT_TRUE(drm->sysmanQueryEngineInfo());
    EXPECT_EQ(3u, drm->ioctlCallsCount);
    auto engineInfo = drm->getEngineInfo();

    std::vector<EngineClassInstance> engines;
    engineInfo->getListOfEnginesOnATile(0u, engines);
    EXPECT_EQ(5u, engines.size());

    engines.clear();
    engineInfo->getListOfEnginesOnATile(1u, engines);
    EXPECT_EQ(5u, engines.size());
}

TEST_F(IoctlHelperPrelimFixture, givenPrelimWhenQueryEngineInfoAndFailIoctlThenFalseReturned) {
    drm->ioctlCallsCount = 0;
    drm->queryDistanceIoctlRetVal = -1;

    std::vector<MemoryRegion> memRegions{
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0},
        {{drm_i915_gem_memory_class::I915_MEMORY_CLASS_DEVICE, 2}, 1024, 0}};
    drm->memoryInfo.reset(new MemoryInfo(memRegions, *drm));
    EXPECT_FALSE(drm->queryEngineInfo());

    EXPECT_EQ(3u, drm->ioctlCallsCount);
    auto engineInfo = drm->getEngineInfo();

    EXPECT_EQ(nullptr, engineInfo);
}

TEST_F(IoctlHelperPrelimFixture, givenIoctlFailureWhenCreateContextWithAccessCountersIsCalledThenErrorIsReturned) {
    drm->ioctlRetVal = EINVAL;
    drm->ioctlCallsCount = 0;

    auto ioctlHelper = drm->getIoctlHelper();
    GemContextCreateExt gcc{};
    EXPECT_THROW(ioctlHelper->createContextWithAccessCounters(gcc), std::runtime_error);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenIoctlSuccessWhenCreateContextWithAccessCountersIsCalledThenSuccessIsReturned) {
    drm->ioctlRetVal = 0;
    drm->ioctlCallsCount = 0;

    auto ioctlHelper = drm->getIoctlHelper();
    GemContextCreateExt gcc{};
    EXPECT_EQ(0u, ioctlHelper->createContextWithAccessCounters(gcc));
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenIoctlFailureWhenCreateCooperativeContexIsCalledThenErrorIsReturned) {
    drm->ioctlRetVal = EINVAL;
    drm->ioctlCallsCount = 0;

    auto ioctlHelper = drm->getIoctlHelper();
    GemContextCreateExt gcc{};
    EXPECT_THROW(ioctlHelper->createCooperativeContext(gcc), std::runtime_error);
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, givenIoctlSuccessWhenCreateCooperativeContexIsCalledThenSuccessIsReturned) {
    drm->ioctlRetVal = 0u;
    drm->ioctlCallsCount = 0;

    auto ioctlHelper = drm->getIoctlHelper();
    GemContextCreateExt gcc{};
    EXPECT_EQ(0u, ioctlHelper->createCooperativeContext(gcc));
    EXPECT_EQ(1u, drm->ioctlCallsCount);
}

TEST_F(IoctlHelperPrelimFixture, whenCreateDrmContextIsCalledThenIoctlIsCalledOnlyOnce) {
    drm->ioctlRetVal = 0u;

    DebugManagerStateRestore stateRestore;
    constexpr bool isCooperativeContextRequested = true;
    constexpr bool isDirectSubmissionRequested = false;

    for (auto &cooperativeContextRequested : {-1, 0, 1}) {
        DebugManager.flags.ForceRunAloneContext.set(cooperativeContextRequested);
        for (auto &accessCountersRequested : {-1, 0, 1}) {
            DebugManager.flags.CreateContextWithAccessCounters.set(accessCountersRequested);
            for (auto vmId = 0u; vmId < 3; vmId++) {
                drm->ioctlCallsCount = 0u;
                drm->createDrmContext(vmId, isDirectSubmissionRequested, isCooperativeContextRequested);

                EXPECT_EQ(1u, drm->ioctlCallsCount);
            }
        }
    }
}

TEST_F(IoctlHelperPrelimFixture, givenProgramDebuggingAndContextDebugSupportedWhenCreatingContextThenCooperativeFlagIsPassedToCreateDrmContextOnlyIfCCSEnginesArePresent) {
    executionEnvironment->setDebuggingEnabled();
    drm->contextDebugSupported = true;
    drm->callBaseCreateDrmContext = false;

    executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->platform.eProductFamily = defaultHwInfo->platform.eProductFamily;

    OsContextLinux osContext(*drm, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular}));
    osContext.ensureContextInitialized();

    EXPECT_NE(static_cast<uint32_t>(-1), drm->passedContextDebugId);
    if (executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->gtSystemInfo.CCSInfo.NumberOfCCSEnabled > 0) {
        EXPECT_TRUE(drm->capturedCooperativeContextRequest);
    } else {
        EXPECT_FALSE(drm->capturedCooperativeContextRequest);
    }

    OsContextLinux osContext2(*drm, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Cooperative}));
    osContext2.ensureContextInitialized();

    EXPECT_NE(static_cast<uint32_t>(-1), drm->passedContextDebugId);
    EXPECT_TRUE(drm->capturedCooperativeContextRequest);
}
