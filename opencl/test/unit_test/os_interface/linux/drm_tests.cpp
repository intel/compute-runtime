/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/linux/drm_mock.h"

#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

#include <fstream>
#include <memory>

using namespace NEO;

TEST(DrmTest, WhenGettingDeviceIdThenCorrectIdReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_NE(nullptr, pDrm);

    pDrm->storedDeviceID = 0x1234;
    int deviceID = 0;
    int ret = pDrm->getDeviceID(deviceID);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(pDrm->storedDeviceID, deviceID);
    delete pDrm;
}

TEST(DrmTest, GivenValidPciPathWhenGettingAdapterBdfThenCorrectValuesAreReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    {
        drm.setPciPath("0000:ab:cd.e");
        EXPECT_EQ(0, drm.queryAdapterBDF());
        auto adapterBdf = drm.getAdapterBDF();
        EXPECT_EQ(0xabu, adapterBdf.Bus);
        EXPECT_EQ(0xcdu, adapterBdf.Device);
        EXPECT_EQ(0xeu, adapterBdf.Function);

        auto pciInfo = drm.getPciBusInfo();
        EXPECT_EQ(0x0u, pciInfo.pciDomain);
        EXPECT_EQ(0xabu, pciInfo.pciBus);
        EXPECT_EQ(0xcdu, pciInfo.pciDevice);
        EXPECT_EQ(0xeu, pciInfo.pciFunction);
    }

    {
        drm.setPciPath("0000:01:23.4");
        EXPECT_EQ(0, drm.queryAdapterBDF());
        auto adapterBdf = drm.getAdapterBDF();
        EXPECT_EQ(0x1u, adapterBdf.Bus);
        EXPECT_EQ(0x23u, adapterBdf.Device);
        EXPECT_EQ(0x4u, adapterBdf.Function);

        auto pciInfo = drm.getPciBusInfo();
        EXPECT_EQ(0x0u, pciInfo.pciDomain);
        EXPECT_EQ(0x1u, pciInfo.pciBus);
        EXPECT_EQ(0x23u, pciInfo.pciDevice);
        EXPECT_EQ(0x4u, pciInfo.pciFunction);
    }
}

TEST(DrmTest, GivenInvalidPciPathWhenGettingAdapterBdfThenInvalidPciInfoIsReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.setPciPath("invalidPci");
    EXPECT_EQ(1, drm.queryAdapterBDF());
    auto adapterBdf = drm.getAdapterBDF();
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), adapterBdf.Data);

    auto pciInfo = drm.getPciBusInfo();
    EXPECT_EQ(PhysicalDevicePciBusInfo::InvalidValue, pciInfo.pciDomain);
    EXPECT_EQ(PhysicalDevicePciBusInfo::InvalidValue, pciInfo.pciBus);
    EXPECT_EQ(PhysicalDevicePciBusInfo::InvalidValue, pciInfo.pciDevice);
    EXPECT_EQ(PhysicalDevicePciBusInfo::InvalidValue, pciInfo.pciFunction);
}

TEST(DrmTest, GivenInvalidPciPathWhenFrequencyIsQueriedThenReturnError) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto hwInfo = *defaultHwInfo;

    int maxFrequency = 0;

    drm.setPciPath("invalidPci");
    int ret = drm.getMaxGpuFrequency(hwInfo, maxFrequency);
    EXPECT_NE(0, ret);

    EXPECT_EQ(0, maxFrequency);
}

TEST(DrmTest, WhenGettingRevisionIdThenCorrectIdIsReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_NE(nullptr, pDrm);

    pDrm->storedDeviceID = 0x1234;
    pDrm->storedDeviceRevID = 0xB;
    int deviceID = 0;
    int ret = pDrm->getDeviceID(deviceID);
    EXPECT_EQ(0, ret);
    int revID = 0;
    ret = pDrm->getDeviceRevID(revID);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(pDrm->storedDeviceID, deviceID);
    EXPECT_EQ(pDrm->storedDeviceRevID, revID);

    delete pDrm;
}

TEST(DrmTest, GivenDrmWhenAskedForGttSizeThenReturnCorrectValue) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    uint64_t queryGttSize = 0;

    drm->storedRetValForGetGttSize = 0;
    drm->storedGTTSize = 1ull << 31;
    EXPECT_EQ(0, drm->queryGttSize(queryGttSize));
    EXPECT_EQ(drm->storedGTTSize, queryGttSize);

    queryGttSize = 0;
    drm->storedRetValForGetGttSize = -1;
    EXPECT_NE(0, drm->queryGttSize(queryGttSize));
    EXPECT_EQ(0u, queryGttSize);
}

TEST(DrmTest, GivenDrmWhenAskedForPreemptionThenCorrectValueReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
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

TEST(DrmTest, GivenDrmWhenAskedForContextThatFailsThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    pDrm->storedRetVal = -1;
    EXPECT_THROW(pDrm->createDrmContext(1, false), std::exception);
    pDrm->storedRetVal = 0;
    delete pDrm;
}

TEST(DrmTest, givenDrmWhenOsContextIsCreatedThenCreateAndDestroyNewDrmOsContext) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());

    {
        OsContextLinux osContext1(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
        osContext1.ensureContextInitialized();

        EXPECT_EQ(1u, osContext1.getDrmContextIds().size());
        EXPECT_EQ(drmMock.receivedCreateContextId, osContext1.getDrmContextIds()[0]);
        EXPECT_EQ(0u, drmMock.receivedDestroyContextId);

        {
            OsContextLinux osContext2(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
            osContext2.ensureContextInitialized();
            EXPECT_EQ(1u, osContext2.getDrmContextIds().size());
            EXPECT_EQ(drmMock.receivedCreateContextId, osContext2.getDrmContextIds()[0]);
            EXPECT_EQ(0u, drmMock.receivedDestroyContextId);
        }
    }

    EXPECT_EQ(2u, drmMock.receivedContextParamRequestCount);
}

TEST(DrmTest, whenCreatingDrmContextWithVirtualMemoryAddressSpaceThenProperVmIdIsSet) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    ASSERT_EQ(1u, drmMock.virtualMemoryIds.size());

    OsContextLinux osContext(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();

    EXPECT_EQ(drmMock.receivedContextParamRequest.value, drmMock.getVirtualMemoryAddressSpace(0u));
}

TEST(DrmTest, whenCreatingDrmContextWithNoVirtualMemoryAddressSpaceThenProperContextIdIsSet) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.destroyVirtualMemoryAddressSpace();

    ASSERT_EQ(0u, drmMock.virtualMemoryIds.size());

    OsContextLinux osContext(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();

    EXPECT_EQ(0u, drmMock.receivedCreateContextId);
    EXPECT_EQ(0u, drmMock.receivedContextParamRequestCount);
}

TEST(DrmTest, givenDrmAndNegativeCheckNonPersistentContextsSupportWhenOsContextIsCreatedThenReceivedContextParamRequestCountReturnsCorrectValue) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    auto expectedCount = 0u;

    {
        drmMock.storedRetValForPersistant = -1;
        drmMock.checkNonPersistentContextsSupport();
        expectedCount += 2;
        OsContextLinux osContext(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
        osContext.ensureContextInitialized();
        EXPECT_EQ(expectedCount, drmMock.receivedContextParamRequestCount);
    }
    {
        drmMock.storedRetValForPersistant = 0;
        drmMock.checkNonPersistentContextsSupport();
        ++expectedCount;
        OsContextLinux osContext(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
        osContext.ensureContextInitialized();
        expectedCount += 2;
        EXPECT_EQ(expectedCount, drmMock.receivedContextParamRequestCount);
    }
}

TEST(DrmTest, givenDrmPreemptionEnabledAndLowPriorityEngineWhenCreatingOsContextThenCallSetContextPriorityIoctl) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.preemptionSupported = false;

    OsContextLinux osContext1(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext1.ensureContextInitialized();
    OsContextLinux osContext2(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::LowPriority}));
    osContext2.ensureContextInitialized();

    EXPECT_EQ(2u, drmMock.receivedContextParamRequestCount);

    drmMock.preemptionSupported = true;

    OsContextLinux osContext3(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext3.ensureContextInitialized();
    EXPECT_EQ(3u, drmMock.receivedContextParamRequestCount);

    OsContextLinux osContext4(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::LowPriority}));
    osContext4.ensureContextInitialized();
    EXPECT_EQ(5u, drmMock.receivedContextParamRequestCount);
    EXPECT_EQ(drmMock.receivedCreateContextId, drmMock.receivedContextParamRequest.ctx_id);
    EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_PRIORITY), drmMock.receivedContextParamRequest.param);
    EXPECT_EQ(static_cast<uint64_t>(-1023), drmMock.receivedContextParamRequest.value);
    EXPECT_EQ(0u, drmMock.receivedContextParamRequest.size);
}

TEST(DrmTest, WhenGettingExecSoftPinThenCorrectValueIsReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    int execSoftPin = 0;

    int ret = pDrm->getExecSoftPin(execSoftPin);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, execSoftPin);

    pDrm->storedExecSoftPin = 1;
    ret = pDrm->getExecSoftPin(execSoftPin);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, execSoftPin);

    delete pDrm;
}

TEST(DrmTest, WhenEnablingTurboBoostThenSucceeds) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    int ret = pDrm->enableTurboBoost();
    EXPECT_EQ(0, ret);

    delete pDrm;
}

TEST(DrmTest, WhenGettingEnabledPooledEuThenCorrectValueIsReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    int enabled = 0;
    int ret = 0;
    pDrm->storedHasPooledEU = -1;
#if defined(I915_PARAM_HAS_POOLED_EU)
    ret = pDrm->getEnabledPooledEu(enabled);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(-1, enabled);

    pDrm->storedHasPooledEU = 0;
    ret = pDrm->getEnabledPooledEu(enabled);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, enabled);

    pDrm->storedHasPooledEU = 1;
    ret = pDrm->getEnabledPooledEu(enabled);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, enabled);

    pDrm->storedRetValForPooledEU = -1;
    ret = pDrm->getEnabledPooledEu(enabled);
    EXPECT_EQ(-1, ret);
    EXPECT_EQ(1, enabled);
#else
    ret = pDrm->getEnabledPooledEu(enabled);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, enabled);
#endif
    delete pDrm;
}

TEST(DrmTest, WhenGettingMinEuInPoolThenCorrectValueIsReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    pDrm->storedMinEUinPool = -1;
    int minEUinPool = 0;
    int ret = 0;
#if defined(I915_PARAM_MIN_EU_IN_POOL)
    ret = pDrm->getMinEuInPool(minEUinPool);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(-1, minEUinPool);

    pDrm->storedMinEUinPool = 0;
    ret = pDrm->getMinEuInPool(minEUinPool);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, minEUinPool);

    pDrm->storedMinEUinPool = 1;
    ret = pDrm->getMinEuInPool(minEUinPool);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, minEUinPool);

    pDrm->storedRetValForMinEUinPool = -1;
    ret = pDrm->getMinEuInPool(minEUinPool);
    EXPECT_EQ(-1, ret);
    EXPECT_EQ(1, minEUinPool);
#else
    ret = pDrm->getMinEuInPool(minEUinPool);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, minEUinPool);
#endif
    delete pDrm;
}

TEST(DrmTest, givenDrmWhenGetErrnoIsCalledThenErrnoValueIsReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_NE(nullptr, pDrm);

    auto errnoFromDrm = pDrm->getErrno();
    EXPECT_EQ(errno, errnoFromDrm);
    delete pDrm;
}
TEST(DrmTest, givenPlatformWhereGetSseuRetFailureWhenCallSetQueueSliceCountThenSliceCountIsNotSet) {
    uint64_t newSliceCount = 1;
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->storedRetValForGetSSEU = -1;
    drm->checkQueueSliceSupport();

    EXPECT_FALSE(drm->sliceCountChangeSupported);
    EXPECT_FALSE(drm->setQueueSliceCount(newSliceCount));
    EXPECT_NE(drm->getSliceMask(newSliceCount), drm->storedParamSseu);
}

TEST(DrmTest, whenCheckNonPeristentSupportIsCalledThenAreNonPersistentContextsSupportedReturnsCorrectValues) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->storedRetValForPersistant = -1;
    drm->checkNonPersistentContextsSupport();
    EXPECT_FALSE(drm->areNonPersistentContextsSupported());
    drm->storedRetValForPersistant = 0;
    drm->checkNonPersistentContextsSupport();
    EXPECT_TRUE(drm->areNonPersistentContextsSupported());
}

TEST(DrmTest, givenPlatformWhereSetSseuRetFailureWhenCallSetQueueSliceCountThenReturnFalse) {
    uint64_t newSliceCount = 1;
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->storedRetValForSetSSEU = -1;
    drm->storedRetValForGetSSEU = 0;
    drm->checkQueueSliceSupport();

    EXPECT_TRUE(drm->sliceCountChangeSupported);
    EXPECT_FALSE(drm->setQueueSliceCount(newSliceCount));
}

TEST(DrmTest, givenPlatformWithSupportToChangeSliceCountWhenCallSetQueueSliceCountThenReturnTrue) {
    uint64_t newSliceCount = 1;
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->storedRetValForSetSSEU = 0;
    drm->storedRetValForSetSSEU = 0;
    drm->checkQueueSliceSupport();

    EXPECT_TRUE(drm->sliceCountChangeSupported);
    EXPECT_TRUE(drm->setQueueSliceCount(newSliceCount));
    drm_i915_gem_context_param_sseu sseu = {};
    EXPECT_EQ(0, drm->getQueueSliceCount(&sseu));
    EXPECT_EQ(drm->getSliceMask(newSliceCount), sseu.slice_mask);
}

namespace NEO {
namespace SysCalls {
extern uint32_t closeFuncCalled;
extern int closeFuncArgPassed;
extern uint32_t vmId;
} // namespace SysCalls
} // namespace NEO

TEST(HwDeviceId, whenHwDeviceIdIsDestroyedThenFileDescriptorIsClosed) {
    SysCalls::closeFuncCalled = 0;
    int fileDescriptor = 0x1234;
    {
        HwDeviceIdDrm hwDeviceId(fileDescriptor, "");
    }
    EXPECT_EQ(1u, SysCalls::closeFuncCalled);
    EXPECT_EQ(fileDescriptor, SysCalls::closeFuncArgPassed);
}

TEST(DrmTest, givenDrmWhenCreatingOsContextThenCreateDrmContextWithVmId) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    OsContextLinux osContext(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();

    EXPECT_EQ(SysCalls::vmId, drmMock.getVirtualMemoryAddressSpace(0));

    auto &contextIds = osContext.getDrmContextIds();
    EXPECT_EQ(1u, contextIds.size());
}

TEST(DrmTest, givenDrmWithPerContextVMRequiredWhenCreatingOsContextsThenImplicitVmIdPerContextIsUsed) {
    auto &rootEnv = *platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0];
    rootEnv.executionEnvironment.setDebuggingEnabled();

    DrmMock drmMock(rootEnv);
    EXPECT_TRUE(drmMock.requirePerContextVM);

    OsContextLinux osContext1(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext1.ensureContextInitialized();
    EXPECT_EQ(0u, drmMock.receivedCreateContextId);

    OsContextLinux osContext2(drmMock, 5u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext2.ensureContextInitialized();
    EXPECT_EQ(0u, drmMock.receivedCreateContextId);
}

TEST(DrmTest, givenPerContextVMRequiredWhenCreatingOsContextsThenImplicitVmIdPerContextIsQueriedAndStored) {
    auto &rootEnv = *platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0];
    rootEnv.executionEnvironment.setDebuggingEnabled();

    DrmMock drmMock(rootEnv);
    EXPECT_TRUE(drmMock.requirePerContextVM);

    drmMock.storedRetValForVmId = 20;

    OsContextLinux osContext(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    EXPECT_EQ(0u, drmMock.receivedCreateContextId);
    EXPECT_EQ(2u, drmMock.receivedContextParamRequestCount);

    auto &drmVmIds = osContext.getDrmVmIds();
    EXPECT_EQ(32u, drmVmIds.size());

    EXPECT_EQ(20u, drmVmIds[0]);
}

TEST(DrmTest, givenPerContextVMRequiredWhenCreatingOsContextForSubDeviceThenImplicitVmIdPerContextIsQueriedAndStoredAtSubDeviceIndex) {
    auto &rootEnv = *platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0];
    rootEnv.executionEnvironment.setDebuggingEnabled();

    DrmMock drmMock(rootEnv);
    EXPECT_TRUE(drmMock.requirePerContextVM);

    drmMock.storedRetValForVmId = 20;
    DeviceBitfield deviceBitfield(1 << 3);

    OsContextLinux osContext(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));
    osContext.ensureContextInitialized();
    EXPECT_EQ(0u, drmMock.receivedCreateContextId);
    EXPECT_EQ(2u, drmMock.receivedContextParamRequestCount);

    auto &drmVmIds = osContext.getDrmVmIds();
    EXPECT_EQ(32u, drmVmIds.size());

    EXPECT_EQ(20u, drmVmIds[3]);

    EXPECT_EQ(0u, drmVmIds[0]);
    EXPECT_EQ(0u, drmVmIds[2]);
}

TEST(DrmTest, givenPerContextVMRequiredWhenCreatingOsContextsForRootDeviceThenImplicitVmIdsPerContextAreQueriedAndStoredAtSubDeviceIndices) {
    auto &rootEnv = *platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0];
    rootEnv.executionEnvironment.setDebuggingEnabled();

    DrmMock drmMock(rootEnv);
    EXPECT_TRUE(drmMock.requirePerContextVM);

    drmMock.storedRetValForVmId = 20;
    DeviceBitfield deviceBitfield(1 | 1 << 1);

    OsContextLinux osContext(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));
    osContext.ensureContextInitialized();
    EXPECT_EQ(0u, drmMock.receivedCreateContextId);
    EXPECT_EQ(2 * 2u, drmMock.receivedContextParamRequestCount);

    auto &drmVmIds = osContext.getDrmVmIds();
    EXPECT_EQ(32u, drmVmIds.size());

    EXPECT_EQ(20u, drmVmIds[0]);
    EXPECT_EQ(20u, drmVmIds[1]);

    EXPECT_EQ(0u, drmVmIds[2]);
    EXPECT_EQ(0u, drmVmIds[31]);
}

TEST(DrmTest, givenNoPerContextVmsDrmWhenCreatingOsContextsThenVmIdIsNotQueriedAndStored) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_FALSE(drmMock.requirePerContextVM);

    drmMock.storedRetValForVmId = 1;

    OsContextLinux osContext(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    EXPECT_EQ(0u, drmMock.receivedCreateContextId);
    EXPECT_EQ(1u, drmMock.receivedContextParamRequestCount);

    auto &drmVmIds = osContext.getDrmVmIds();
    EXPECT_EQ(0u, drmVmIds.size());
}

TEST(DrmTest, givenProgramDebuggingAndContextDebugAvailableWhenCreatingContextThenSetContextDebugFlagIsCalled) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->setDebuggingEnabled();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    DrmMockNonFailing drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.contextDebugSupported = true;

    OsContextLinux osContext(drmMock, 5u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();

    // drmMock returns ctxId == 0
    EXPECT_EQ(0u, drmMock.passedContextDebugId);
}

TEST(DrmTest, givenProgramDebuggingAndContextDebugAvailableWhenCreatingContextForInternalEngineThenSetContextDebugFlagIsNotCalled) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->setDebuggingEnabled();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    DrmMockNonFailing drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.contextDebugSupported = true;

    OsContextLinux osContext(drmMock, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Internal}));
    osContext.ensureContextInitialized();

    EXPECT_EQ(static_cast<uint32_t>(-1), drmMock.passedContextDebugId);
}

TEST(DrmTest, givenPrintIoctlDebugFlagSetWhenGettingTimestampFrequencyThenCaptureExpectedOutput) {
    DebugManagerStateRestore restore;
    DebugManager.flags.PrintIoctlEntries.set(true);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    int frequency = 0;

    testing::internal::CaptureStdout(); // start capturing

    int ret = drm.getTimestampFrequency(frequency);
    std::string outputString = testing::internal::GetCapturedStdout(); // stop capturing

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1000, frequency);

    std::string expectedString = "DRM_IOCTL_I915_GETPARAM: param: I915_PARAM_CS_TIMESTAMP_FREQUENCY, output value: 1000, retCode: 0";
    EXPECT_NE(std::string::npos, outputString.find(expectedString));
}

TEST(DrmTest, givenPrintIoctlDebugFlagNotSetWhenGettingTimestampFrequencyThenCaptureExpectedOutput) {
    DebugManagerStateRestore restore;
    DebugManager.flags.PrintIoctlEntries.set(false);

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    int frequency = 0;

    testing::internal::CaptureStdout(); // start capturing

    int ret = drm.getTimestampFrequency(frequency);
    std::string outputString = testing::internal::GetCapturedStdout(); // stop capturing

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1000, frequency);

    std::string expectedString = "DRM_IOCTL_I915_GETPARAM: param: I915_PARAM_CS_TIMESTAMP_FREQUENCY, output value: 1000, retCode: 0";
    EXPECT_EQ(std::string::npos, outputString.find(expectedString));
}

TEST(DrmTest, givenProgramDebuggingWhenCreatingContextThenUnrecoverableContextIsSet) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->setDebuggingEnabled();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    OsContextLinux osContext(drm, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();

    EXPECT_EQ(0u, drm.receivedRecoverableContextValue);
    EXPECT_EQ(2u, drm.receivedContextParamRequestCount);
}

TEST(DrmQueryTest, GivenDrmWhenSetupHardwareInfoCalledThenCorrectMaxValuesInGtSystemInfoArePreserved) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = *NEO::defaultHwInfo.get();
    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.failRetTopology = true;

    drm.storedEUVal = 48;
    drm.storedSSVal = 6;
    hwInfo->gtSystemInfo.SliceCount = 2;

    auto setupHardwareInfo = [](HardwareInfo *, bool) {};
    DeviceDescriptor device = {0, hwInfo, setupHardwareInfo, GTTYPE_UNDEFINED};

    drm.setupHardwareInfo(&device, false);

    EXPECT_EQ(NEO::defaultHwInfo->gtSystemInfo.MaxSlicesSupported, hwInfo->gtSystemInfo.MaxSlicesSupported);
    EXPECT_EQ(NEO::defaultHwInfo->gtSystemInfo.MaxSubSlicesSupported, hwInfo->gtSystemInfo.MaxSubSlicesSupported);
    EXPECT_EQ(NEO::defaultHwInfo->gtSystemInfo.MaxEuPerSubSlice, hwInfo->gtSystemInfo.MaxEuPerSubSlice);
}

TEST(DrmQueryTest, GivenLessAvailableSubSlicesThanMaxSubSlicesWhenQueryingTopologyInfoThenCorrectMaxSubSliceCountIsSet) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = *NEO::defaultHwInfo.get();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.disableSomeTopology = true;

    Drm::QueryTopologyData topologyData = {};
    drm.storedSVal = 2;
    drm.storedSSVal = 6;
    drm.storedEUVal = 16;

    EXPECT_TRUE(drm.queryTopology(*executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo(), topologyData));

    EXPECT_EQ(1, topologyData.sliceCount);
    EXPECT_EQ(1, topologyData.subSliceCount);
    EXPECT_EQ(1, topologyData.euCount);

    EXPECT_EQ(drm.storedSVal, topologyData.maxSliceCount);
    EXPECT_EQ(2, topologyData.maxSubSliceCount);
}

TEST(DrmQueryTest, givenDrmWhenGettingTopologyMapThenCorrectMapIsReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = *NEO::defaultHwInfo.get();
    DrmMock drmMock{*executionEnvironment->rootDeviceEnvironments[0]};

    Drm::QueryTopologyData topologyData = {};

    EXPECT_TRUE(drmMock.queryTopology(*executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo(), topologyData));

    auto topologyMap = drmMock.getTopologyMap();

    EXPECT_LE(1u, topologyMap.size());

    for (uint32_t i = 0; i < topologyMap.size(); i++) {
        EXPECT_EQ(drmMock.storedSVal, static_cast<int>(topologyMap.at(i).sliceIndices.size()));
    }
}

TEST(DrmQueryTest, GivenNonTileArchitectureWhenFrequencyIsQueriedThenFallbackToLegacyInterface) {
    int expectedMaxFrequency = 2000;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 0;
    hwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = true;

    std::string gtMaxFreqFile = "test_files/linux/devices/device/drm/card1/gt_max_freq_mhz";
    EXPECT_TRUE(fileExists(gtMaxFreqFile));

    drm.setPciPath("device");

    int maxFrequency = 0;
    int ret = drm.getMaxGpuFrequency(hwInfo, maxFrequency);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(expectedMaxFrequency, maxFrequency);
}

TEST(DrmQueryTest, GivenTileArchitectureIsInvalidWhenFrequencyIsQueriedThenFallbackToLegacyInterface) {
    int expectedMaxFrequency = 2000;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 2;
    hwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = false;

    std::string gtMaxFreqFile = "test_files/linux/devices/device/drm/card1/gt_max_freq_mhz";
    EXPECT_TRUE(fileExists(gtMaxFreqFile));

    drm.setPciPath("device");

    int maxFrequency = 0;
    int ret = drm.getMaxGpuFrequency(hwInfo, maxFrequency);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(expectedMaxFrequency, maxFrequency);
}

TEST(DrmQueryTest, GivenRpsMaxFreqFileExistsWhenFrequencyIsQueriedThenValidValueIsReturned) {
    int expectedMaxFrequency = 3000;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 1;
    hwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = true;

    std::string rpsMaxFreqFile = "test_files/linux/devices/device/drm/card1/gt/gt0/rps_max_freq_mhz";
    EXPECT_TRUE(fileExists(rpsMaxFreqFile));

    drm.setPciPath("device");

    int maxFrequency = 0;
    int ret = drm.getMaxGpuFrequency(hwInfo, maxFrequency);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(expectedMaxFrequency, maxFrequency);
}

TEST(DrmQueryTest, GivenRpsMaxFreqFilesExistWhenFrequenciesAreQueriedThenValidValueIsReturned) {
    int expectedMaxFrequency = 4000;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 2;
    hwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = true;

    std::string rpsMaxFreqFile = "test_files/linux/devices/device/drm/card1/gt/gt1/rps_max_freq_mhz";
    EXPECT_TRUE(fileExists(rpsMaxFreqFile));

    drm.setPciPath("device");

    int maxFrequency = 0;
    int ret = drm.getMaxGpuFrequency(hwInfo, maxFrequency);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(expectedMaxFrequency, maxFrequency);
}

TEST(DrmQueryTest, GivenRpsMaxFreqFileDoesntExistWhenFrequencyIsQueriedThenFallbackToLegacyInterface) {
    int expectedMaxFrequency = 2000;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = 3;
    hwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = true;

    std::string rpsMaxFreqFile = "test_files/linux/devices/device/drm/card1/gt/gt2/rps_max_freq_mhz";
    EXPECT_FALSE(fileExists(rpsMaxFreqFile));

    std::string gtMaxFreqFile = "test_files/linux/devices/device/drm/card1/gt_max_freq_mhz";
    EXPECT_TRUE(fileExists(gtMaxFreqFile));

    drm.setPciPath("device");

    int maxFrequency = 0;
    int ret = drm.getMaxGpuFrequency(hwInfo, maxFrequency);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(expectedMaxFrequency, maxFrequency);
}
