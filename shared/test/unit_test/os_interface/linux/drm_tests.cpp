/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_os_context_linux.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"

#include <fstream>
#include <memory>

using namespace NEO;

std::string getLinuxDevicesPath(const char *file) {
    std::string resultString(Os::sysFsPciPathPrefix);
    resultString += file;

    return resultString;
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
    EXPECT_EQ(PhysicalDevicePciBusInfo::invalidValue, pciInfo.pciDomain);
    EXPECT_EQ(PhysicalDevicePciBusInfo::invalidValue, pciInfo.pciBus);
    EXPECT_EQ(PhysicalDevicePciBusInfo::invalidValue, pciInfo.pciDevice);
    EXPECT_EQ(PhysicalDevicePciBusInfo::invalidValue, pciInfo.pciFunction);
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

TEST(DrmTest, whenGetBcsTypeToBindThenReturnOptimalSequence) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    {
        auto includeMain = false;
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, drm.getBcsTypeToBind(0, includeMain));
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS3, drm.getBcsTypeToBind(1, includeMain));
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS4, drm.getBcsTypeToBind(2, includeMain));
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS7, drm.getBcsTypeToBind(3, includeMain));
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, drm.getBcsTypeToBind(4, includeMain));
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS5, drm.getBcsTypeToBind(5, includeMain));
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS6, drm.getBcsTypeToBind(6, includeMain));
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS8, drm.getBcsTypeToBind(7, includeMain));
    }
    {
        auto includeMain = true;
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, drm.getBcsTypeToBind(0, includeMain));
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS2, drm.getBcsTypeToBind(1, includeMain));
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS3, drm.getBcsTypeToBind(2, includeMain));
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS4, drm.getBcsTypeToBind(3, includeMain));
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS7, drm.getBcsTypeToBind(4, includeMain));
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, drm.getBcsTypeToBind(5, includeMain));
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS5, drm.getBcsTypeToBind(6, includeMain));
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS6, drm.getBcsTypeToBind(7, includeMain));
        EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS8, drm.getBcsTypeToBind(8, includeMain));
    }
}

TEST(DrmTest, WhenGettingRevisionIdThenCorrectIdIsReturned) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_NE(nullptr, pDrm);

    pDrm->storedDeviceID = 0x1234;
    pDrm->storedDeviceRevID = 0xB;
    pDrm->deviceId = 0;
    pDrm->revisionId = 0;

    EXPECT_TRUE(pDrm->queryDeviceIdAndRevision());

    EXPECT_EQ(pDrm->storedDeviceID, pDrm->deviceId);
    EXPECT_EQ(pDrm->storedDeviceRevID, pDrm->revisionId);

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
    EXPECT_THROW(pDrm->createDrmContext(1, false, false), std::exception);
    pDrm->storedRetVal = 0;
    delete pDrm;
}

TEST(DrmTest, givenDrmWhenOsContextIsCreatedThenCreateAndDestroyNewDrmOsContext) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    {
        OsContextLinux osContext1(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
        osContext1.ensureContextInitialized();

        EXPECT_EQ(1u, osContext1.getDrmContextIds().size());
        EXPECT_EQ(drmMock.storedDrmContextId, osContext1.getDrmContextIds()[0]);
        EXPECT_EQ(0u, drmMock.receivedDestroyContextId);

        {
            OsContextLinux osContext2(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
            osContext2.ensureContextInitialized();
            EXPECT_EQ(1u, osContext2.getDrmContextIds().size());
            EXPECT_EQ(drmMock.storedDrmContextId, osContext2.getDrmContextIds()[0]);
            EXPECT_EQ(0u, drmMock.receivedDestroyContextId);
        }
    }

    EXPECT_EQ(2u, drmMock.receivedContextParamRequestCount);
}

TEST(DrmTest, whenCreatingDrmContextWithVirtualMemoryAddressSpaceThenProperVmIdIsSet) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

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
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.destroyVirtualMemoryAddressSpace();

    ASSERT_EQ(0u, drmMock.virtualMemoryIds.size());

    OsContextLinux osContext(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();

    EXPECT_EQ(0u, drmMock.receivedContextParamRequestCount);
}

TEST(DrmTest, givenDrmAndNegativeCheckNonPersistentContextsSupportWhenOsContextIsCreatedThenReceivedContextParamRequestCountReturnsCorrectValue) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

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
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

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
    EXPECT_EQ(drmMock.storedDrmContextId, drmMock.receivedContextParamRequest.contextId);
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
    GemContextParamSseu sseu = {};
    EXPECT_EQ(0, drm->getQueueSliceCount(&sseu));
    EXPECT_EQ(drm->getSliceMask(newSliceCount), sseu.sliceMask);
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
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    OsContextLinux osContext(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();

    drmMock.latestCreatedVmId = 0u;
    auto expectedVmId = drmMock.latestCreatedVmId + 1;
    EXPECT_EQ(expectedVmId, drmMock.getVirtualMemoryAddressSpace(0));

    auto &contextIds = osContext.getDrmContextIds();
    EXPECT_EQ(1u, contextIds.size());
}

TEST(DrmTest, givenDrmWithPerContextVMRequiredWhenCreatingOsContextsThenImplicitVmIdPerContextIsUsed) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootEnv = *executionEnvironment.rootDeviceEnvironments[0];
    rootEnv.executionEnvironment.setDebuggingEnabled();

    DrmMock drmMock(rootEnv);
    EXPECT_TRUE(drmMock.requirePerContextVM);

    OsContextLinux osContext1(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext1.ensureContextInitialized();

    OsContextLinux osContext2(drmMock, 5u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext2.ensureContextInitialized();
}

TEST(DrmTest, givenPerContextVMRequiredWhenCreatingOsContextsThenImplicitVmIdPerContextIsQueriedAndStored) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootEnv = *executionEnvironment.rootDeviceEnvironments[0];
    rootEnv.executionEnvironment.setDebuggingEnabled();

    DrmMock drmMock(rootEnv);
    EXPECT_TRUE(drmMock.requirePerContextVM);

    drmMock.storedRetValForVmId = 20;

    OsContextLinux osContext(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    EXPECT_EQ(2u, drmMock.receivedContextParamRequestCount);

    auto &drmVmIds = osContext.getDrmVmIds();
    EXPECT_EQ(32u, drmVmIds.size());

    EXPECT_EQ(20u, drmVmIds[0]);
}

TEST(DrmTest, givenPerContextVMRequiredWhenCreatingOsContextForSubDeviceThenImplicitVmIdPerContextIsQueriedAndStoredAtSubDeviceIndex) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootEnv = *executionEnvironment.rootDeviceEnvironments[0];
    rootEnv.executionEnvironment.setDebuggingEnabled();

    DrmMock drmMock(rootEnv);
    EXPECT_TRUE(drmMock.requirePerContextVM);

    drmMock.storedRetValForVmId = 20;
    DeviceBitfield deviceBitfield(1 << 3);

    OsContextLinux osContext(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));
    osContext.ensureContextInitialized();
    EXPECT_EQ(2u, drmMock.receivedContextParamRequestCount);

    auto &drmVmIds = osContext.getDrmVmIds();
    EXPECT_EQ(32u, drmVmIds.size());

    EXPECT_EQ(20u, drmVmIds[3]);

    EXPECT_EQ(0u, drmVmIds[0]);
    EXPECT_EQ(0u, drmVmIds[2]);
}

TEST(DrmTest, givenPerContextVMRequiredWhenCreatingOsContextsForRootDeviceThenImplicitVmIdsPerContextAreQueriedAndStoredAtSubDeviceIndices) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootEnv = *executionEnvironment.rootDeviceEnvironments[0];
    rootEnv.executionEnvironment.setDebuggingEnabled();

    DrmMock drmMock(rootEnv);
    EXPECT_TRUE(drmMock.requirePerContextVM);

    drmMock.storedRetValForVmId = 20;
    DeviceBitfield deviceBitfield(1 | 1 << 1);

    OsContextLinux osContext(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));
    osContext.ensureContextInitialized();
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
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_FALSE(drmMock.requirePerContextVM);

    drmMock.storedRetValForVmId = 1;

    OsContextLinux osContext(drmMock, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();
    EXPECT_EQ(1u, drmMock.receivedContextParamRequestCount);

    auto &drmVmIds = osContext.getDrmVmIds();
    EXPECT_EQ(0u, drmVmIds.size());
}

TEST(DrmTest, givenProgramDebuggingAndContextDebugAvailableWhenCreatingContextThenSetContextDebugFlagIsCalled) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->setDebuggingEnabled();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    DrmMockNonFailing drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.contextDebugSupported = true;
    drmMock.callBaseCreateDrmContext = false;

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
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    DrmMockNonFailing drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.contextDebugSupported = true;

    OsContextLinux osContext(drmMock, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Internal}));
    osContext.ensureContextInitialized();

    EXPECT_EQ(static_cast<uint32_t>(-1), drmMock.passedContextDebugId);
}

TEST(DrmTest, givenNotEnabledDebuggingOrContextDebugUnsupportedWhenCreatingContextThenCooperativeFlagIsNotPassedToCreateDrmContext) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    DrmMockNonFailing drmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    drmMock.contextDebugSupported = true;
    drmMock.callBaseCreateDrmContext = false;
    drmMock.capturedCooperativeContextRequest = true;

    EXPECT_FALSE(executionEnvironment->isDebuggingEnabled());

    OsContextLinux osContext(drmMock, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular}));
    osContext.ensureContextInitialized();

    EXPECT_FALSE(drmMock.capturedCooperativeContextRequest);

    executionEnvironment->setDebuggingEnabled();
    drmMock.contextDebugSupported = false;
    drmMock.callBaseCreateDrmContext = false;
    drmMock.capturedCooperativeContextRequest = true;

    OsContextLinux osContext2(drmMock, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::Regular}));
    osContext2.ensureContextInitialized();

    EXPECT_FALSE(drmMock.capturedCooperativeContextRequest);
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
    DebugManager.flags.PrintIoctlEntries.set(false);
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
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    OsContextLinux osContext(drm, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized();

    EXPECT_EQ(0u, drm.receivedRecoverableContextValue);
    EXPECT_EQ(2u, drm.receivedContextParamRequestCount);
}

TEST(DrmTest, whenPageFaultIsSupportedThenUseVmBindImmediate) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    for (auto hasPageFaultSupport : {false, true}) {
        drm.pageFaultSupported = hasPageFaultSupport;
        EXPECT_EQ(hasPageFaultSupport, drm.useVMBindImmediate());
    }
}

TEST(DrmTest, whenImmediateVmBindExtIsEnabledThenUseVmBindImmediate) {
    DebugManagerStateRestore restorer;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    for (auto enableImmediateBind : {false, true}) {
        DebugManager.flags.EnableImmediateVmBindExt.set(enableImmediateBind);
        EXPECT_EQ(enableImmediateBind, drm.useVMBindImmediate());
    }
}

TEST(DrmQueryTest, GivenDrmWhenSetupHardwareInfoCalledThenCorrectMaxValuesInGtSystemInfoArePreservedAndIoctlHelperSet) {
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
    DeviceDescriptor device = {0, hwInfo, setupHardwareInfo};

    drm.ioctlHelper.reset();
    drm.setupHardwareInfo(&device, false);
    EXPECT_NE(nullptr, drm.getIoctlHelper());
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
    drm.storedSVal = 4;
    drm.storedSSVal = drm.storedSVal * 7;
    drm.storedEUVal = drm.storedSSVal * 4;

    EXPECT_TRUE(drm.queryTopology(*executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo(), topologyData));

    EXPECT_EQ(2, topologyData.sliceCount);
    EXPECT_EQ(6, topologyData.subSliceCount);
    EXPECT_EQ(12, topologyData.euCount);

    EXPECT_EQ(drm.storedSVal, topologyData.maxSliceCount);
    EXPECT_EQ(7, topologyData.maxSubSliceCount);
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

TEST(DrmQueryTest, GivenSingleSliceConfigWhenQueryingTopologyInfoThenSubsliceIndicesAreStored) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = *NEO::defaultHwInfo.get();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    Drm::QueryTopologyData topologyData = {};
    drm.storedSVal = 1;
    drm.storedSSVal = drm.storedSVal * 7;
    drm.storedEUVal = drm.storedSSVal * 4;

    EXPECT_TRUE(drm.queryTopology(*executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo(), topologyData));

    EXPECT_EQ(1, topologyData.sliceCount);
    EXPECT_EQ(7, topologyData.subSliceCount);
    EXPECT_EQ(28, topologyData.euCount);

    EXPECT_EQ(drm.storedSVal, topologyData.maxSliceCount);
    EXPECT_EQ(7, topologyData.maxSubSliceCount);

    auto topologyMap = drm.getTopologyMap();

    for (uint32_t i = 0; i < topologyMap.size(); i++) {
        EXPECT_EQ(drm.storedSVal, static_cast<int>(topologyMap.at(i).sliceIndices.size()));

        EXPECT_EQ(7u, topologyMap.at(i).subsliceIndices.size());
        for (int subsliceId = 0; subsliceId < static_cast<int>(topologyMap.at(i).subsliceIndices.size()); subsliceId++) {
            EXPECT_EQ(subsliceId, topologyMap.at(i).subsliceIndices[subsliceId]);
        }
    }
}

TEST(DrmQueryTest, GivenMultiSliceConfigWhenQueryingTopologyInfoThenSubsliceIndicesAreNotStored) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = *NEO::defaultHwInfo.get();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    Drm::QueryTopologyData topologyData = {};
    drm.storedSVal = 2;
    drm.storedSSVal = drm.storedSVal * 7;
    drm.storedEUVal = drm.storedSSVal * 4;

    EXPECT_TRUE(drm.queryTopology(*executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo(), topologyData));

    EXPECT_EQ(2, topologyData.sliceCount);
    EXPECT_EQ(14, topologyData.subSliceCount);
    EXPECT_EQ(56, topologyData.euCount);

    EXPECT_EQ(drm.storedSVal, topologyData.maxSliceCount);
    EXPECT_EQ(7, topologyData.maxSubSliceCount);

    auto topologyMap = drm.getTopologyMap();

    for (uint32_t i = 0; i < topologyMap.size(); i++) {
        EXPECT_EQ(drm.storedSVal, static_cast<int>(topologyMap.at(i).sliceIndices.size()));

        EXPECT_EQ(0u, topologyMap.at(i).subsliceIndices.size());
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

    std::string gtMaxFreqFile = getLinuxDevicesPath("device/drm/card1/gt_max_freq_mhz");
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

    std::string gtMaxFreqFile = getLinuxDevicesPath("device/drm/card1/gt_max_freq_mhz");
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

    std::string rpsMaxFreqFile = getLinuxDevicesPath("device/drm/card1/gt/gt0/rps_max_freq_mhz");
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

    std::string rpsMaxFreqFile = getLinuxDevicesPath("device/drm/card1/gt/gt1/rps_max_freq_mhz");
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

    std::string rpsMaxFreqFile = getLinuxDevicesPath("device/drm/card1/gt/gt2/rps_max_freq_mhz");
    EXPECT_FALSE(fileExists(rpsMaxFreqFile));

    std::string gtMaxFreqFile = getLinuxDevicesPath("device/drm/card1/gt_max_freq_mhz");
    EXPECT_TRUE(fileExists(gtMaxFreqFile));

    drm.setPciPath("device");

    int maxFrequency = 0;
    int ret = drm.getMaxGpuFrequency(hwInfo, maxFrequency);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(expectedMaxFrequency, maxFrequency);
}

TEST(DrmTest, whenCheckedIfResourcesCleanupCanBeSkippedThenReturnsFalse) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_TRUE(pDrm->isDriverAvaliable());
    EXPECT_FALSE(pDrm->skipResourceCleanup());
    delete pDrm;
}

TEST(DrmQueryTest, givenUapiPrelimVersionThenReturnCorrectString) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    std::string prelimVersionFile = getLinuxDevicesPath("device/drm/card1/prelim_uapi_version");
    EXPECT_TRUE(fileExists(prelimVersionFile));

    drm.setPciPath("device");

    std::string prelimVersion = "";
    drm.getPrelimVersion(prelimVersion);

    EXPECT_EQ("2.0", prelimVersion);
}

TEST(DrmQueryTest, givenUapiPrelimVersionWithInvalidPathThenReturnEmptyString) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.setPciPath("invalidPath");

    std::string prelimVersion = "2.0";
    drm.getPrelimVersion(prelimVersion);

    EXPECT_TRUE(prelimVersion.empty());
}

TEST(DrmTest, givenInvalidUapiPrelimVersionThenFallbackToBasePrelim) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    const auto productFamily = defaultHwInfo->platform.eProductFamily;
    std::unique_ptr<IoctlHelper> ioctlHelper(IoctlHelper::get(productFamily, "-1", "unk", drm));
    EXPECT_NE(nullptr, ioctlHelper.get());
}

TEST(DrmTest, GivenCompletionFenceDebugFlagWhenCreatingDrmObjectThenExpectCorrectSetting) {
    DebugManagerStateRestore restore;

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    HardwareInfo *hwInfo = defaultHwInfo.get();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drmDefault{*executionEnvironment->rootDeviceEnvironments[0]};
    drmDefault.callBaseIsVmBindAvailable = true;
    if (drmDefault.isVmBindAvailable()) {
        EXPECT_TRUE(drmDefault.completionFenceSupport());
    } else {
        EXPECT_FALSE(drmDefault.completionFenceSupport());
    }

    DebugManager.flags.UseVmBind.set(1);
    DebugManager.flags.EnableDrmCompletionFence.set(1);
    DrmMock drmEnabled{*executionEnvironment->rootDeviceEnvironments[0]};
    drmEnabled.callBaseIsVmBindAvailable = true;
    EXPECT_TRUE(drmEnabled.completionFenceSupport());

    DebugManager.flags.EnableDrmCompletionFence.set(0);
    DrmMock drmDisabled{*executionEnvironment->rootDeviceEnvironments[0]};
    drmDisabled.callBaseIsVmBindAvailable = true;
    EXPECT_FALSE(drmDisabled.completionFenceSupport());
}

TEST(DrmTest, GivenIoctlErrorWhenIsGpuHangIsCalledThenErrorIsThrown) {
    ExecutionEnvironment executionEnvironment{};
    executionEnvironment.prepareRootDeviceEnvironments(1);

    DrmMock drm{*executionEnvironment.rootDeviceEnvironments[0]};
    uint32_t contextId{0};
    EngineDescriptor engineDescriptor{EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular})};

    MockOsContextLinux mockOsContextLinux{drm, contextId, engineDescriptor};
    mockOsContextLinux.drmContextIds.push_back(0);
    mockOsContextLinux.drmContextIds.push_back(3);

    EXPECT_THROW(drm.isGpuHangDetected(mockOsContextLinux), std::runtime_error);
}

TEST(DrmTest, GivenZeroBatchActiveAndZeroBatchPendingResetStatsWhenIsGpuHangIsCalledThenNoHangIsReported) {
    ExecutionEnvironment executionEnvironment{};
    executionEnvironment.prepareRootDeviceEnvironments(1);

    DrmMock drm{*executionEnvironment.rootDeviceEnvironments[0]};
    uint32_t contextId{0};
    EngineDescriptor engineDescriptor{EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular})};

    MockOsContextLinux mockOsContextLinux{drm, contextId, engineDescriptor};
    mockOsContextLinux.drmContextIds.push_back(0);
    mockOsContextLinux.drmContextIds.push_back(3);

    ResetStats resetStats{};
    resetStats.contextId = 0;
    drm.resetStatsToReturn.push_back(resetStats);

    resetStats.contextId = 3;
    drm.resetStatsToReturn.push_back(resetStats);

    bool isGpuHangDetected{};
    EXPECT_NO_THROW(isGpuHangDetected = drm.isGpuHangDetected(mockOsContextLinux));
    EXPECT_FALSE(isGpuHangDetected);
}

TEST(DrmTest, GivenBatchActiveGreaterThanZeroResetStatsWhenIsGpuHangIsCalledThenHangIsReported) {
    ExecutionEnvironment executionEnvironment{};
    executionEnvironment.prepareRootDeviceEnvironments(1);

    DrmMock drm{*executionEnvironment.rootDeviceEnvironments[0]};
    uint32_t contextId{0};
    EngineDescriptor engineDescriptor{EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular})};

    MockOsContextLinux mockOsContextLinux{drm, contextId, engineDescriptor};
    mockOsContextLinux.drmContextIds.push_back(0);
    mockOsContextLinux.drmContextIds.push_back(3);

    ResetStats resetStats{};
    resetStats.contextId = 0;
    drm.resetStatsToReturn.push_back(resetStats);

    resetStats.contextId = 3;
    resetStats.batchActive = 2;
    drm.resetStatsToReturn.push_back(resetStats);

    bool isGpuHangDetected{};
    EXPECT_NO_THROW(isGpuHangDetected = drm.isGpuHangDetected(mockOsContextLinux));
    EXPECT_TRUE(isGpuHangDetected);
}

TEST(DrmTest, GivenBatchPendingGreaterThanZeroResetStatsWhenIsGpuHangIsCalledThenHangIsReported) {
    ExecutionEnvironment executionEnvironment{};
    executionEnvironment.prepareRootDeviceEnvironments(1);

    DrmMock drm{*executionEnvironment.rootDeviceEnvironments[0]};
    uint32_t contextId{0};
    EngineDescriptor engineDescriptor{EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular})};

    MockOsContextLinux mockOsContextLinux{drm, contextId, engineDescriptor};
    mockOsContextLinux.drmContextIds.push_back(8);

    ResetStats resetStats{};
    resetStats.contextId = 8;
    resetStats.batchPending = 7;
    drm.resetStatsToReturn.push_back(resetStats);

    bool isGpuHangDetected{};
    EXPECT_NO_THROW(isGpuHangDetected = drm.isGpuHangDetected(mockOsContextLinux));
    EXPECT_TRUE(isGpuHangDetected);
}

TEST(DrmTest, givenSetupIoctlHelperWhenCalledTwiceThenIoctlHelperIsSetOnlyOnce) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.ioctlHelper.reset(nullptr);

    const auto productFamily = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->platform.eProductFamily;
    drm.setupIoctlHelper(productFamily);

    EXPECT_NE(nullptr, drm.ioctlHelper.get());
    auto ioctlHelper = drm.ioctlHelper.get();
    drm.setupIoctlHelper(productFamily);
    EXPECT_EQ(ioctlHelper, drm.ioctlHelper.get());
}

TEST(DrmWrapperTest, WhenGettingDrmIoctlGetparamValueThenIoctlHelperIsNotNeeded) {
    EXPECT_EQ(getIoctlRequestValue(DrmIoctl::Getparam, nullptr), static_cast<unsigned int>(DRM_IOCTL_I915_GETPARAM));
    EXPECT_THROW(getIoctlRequestValue(DrmIoctl::DG1GemCreateExt, nullptr), std::runtime_error);
}

TEST(DrmWrapperTest, WhenGettingChipsetIdParamValueThenIoctlHelperIsNotNeeded) {
    EXPECT_EQ(getDrmParamValue(DrmParam::ParamChipsetId, nullptr), static_cast<int>(I915_PARAM_CHIPSET_ID));
}

TEST(DrmWrapperTest, WhenGettingRevisionParamValueThenIoctlHelperIsNotNeeded) {
    EXPECT_EQ(getDrmParamValue(DrmParam::ParamRevision, nullptr), static_cast<int>(I915_PARAM_REVISION));
}

class MockIoctlHelper : public IoctlHelperPrelim20 {
  public:
    using IoctlHelperPrelim20::IoctlHelperPrelim20;
    unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const override {
        return ioctlRequestValue;
    };

    int getDrmParamValue(DrmParam drmParam) const override {
        return drmParamValue;
    }

    unsigned int ioctlRequestValue = 1234u;
    int drmParamValue = 1234;
};

TEST(DrmWrapperTest, whenGettingDrmParamOrIoctlRequestValueThenUseIoctlHelperWhenAvailable) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    MockIoctlHelper ioctlHelper{drm};
    EXPECT_EQ(getIoctlRequestValue(DrmIoctl::Getparam, &ioctlHelper), ioctlHelper.ioctlRequestValue);
    EXPECT_NE(getIoctlRequestValue(DrmIoctl::Getparam, nullptr), getIoctlRequestValue(DrmIoctl::Getparam, &ioctlHelper));

    EXPECT_EQ(getDrmParamValue(DrmParam::ParamChipsetId, &ioctlHelper), ioctlHelper.drmParamValue);
    EXPECT_NE(getDrmParamValue(DrmParam::ParamChipsetId, nullptr), getDrmParamValue(DrmParam::ParamChipsetId, &ioctlHelper));
}

TEST(DrmWrapperTest, WhenGettingIoctlStringValueThenProperStringIsReturned) {
    std::map<DrmIoctl, const char *> ioctlCodeStringMap = {
        {DrmIoctl::GemClose, "DRM_IOCTL_GEM_CLOSE"},
        {DrmIoctl::Getparam, "DRM_IOCTL_I915_GETPARAM"},
        {DrmIoctl::GemExecbuffer2, "DRM_IOCTL_I915_GEM_EXECBUFFER2"},
        {DrmIoctl::GemCreate, "DRM_IOCTL_I915_GEM_CREATE"},
        {DrmIoctl::GemSetDomain, "DRM_IOCTL_I915_GEM_SET_DOMAIN"},
        {DrmIoctl::GemSetTiling, "DRM_IOCTL_I915_GEM_SET_TILING"},
        {DrmIoctl::GemGetTiling, "DRM_IOCTL_I915_GEM_GET_TILING"},
        {DrmIoctl::GemWait, "DRM_IOCTL_I915_GEM_WAIT"},
        {DrmIoctl::GemContextCreateExt, "DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT"},
        {DrmIoctl::GemContextDestroy, "DRM_IOCTL_I915_GEM_CONTEXT_DESTROY"},
        {DrmIoctl::RegRead, "DRM_IOCTL_I915_REG_READ"},
        {DrmIoctl::GetResetStats, "DRM_IOCTL_I915_GET_RESET_STATS"},
        {DrmIoctl::GemUserptr, "DRM_IOCTL_I915_GEM_USERPTR"},
        {DrmIoctl::GemContextGetparam, "DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM"},
        {DrmIoctl::GemContextSetparam, "DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM"},
        {DrmIoctl::Query, "DRM_IOCTL_I915_QUERY"},
        {DrmIoctl::PrimeFdToHandle, "DRM_IOCTL_PRIME_FD_TO_HANDLE"},
        {DrmIoctl::GemVmBind, "PRELIM_DRM_IOCTL_I915_GEM_VM_BIND"},
        {DrmIoctl::GemVmUnbind, "PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND"},
        {DrmIoctl::GemWaitUserFence, "PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE"},
        {DrmIoctl::GemCreateExt, "DRM_IOCTL_I915_GEM_CREATE_EXT"},
        {DrmIoctl::DG1GemCreateExt, "DG1_DRM_IOCTL_I915_GEM_CREATE_EXT"},
        {DrmIoctl::GemVmAdvise, "PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE"},
        {DrmIoctl::GemVmPrefetch, "PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH"},
        {DrmIoctl::UuidRegister, "PRELIM_DRM_IOCTL_I915_UUID_REGISTER"},
        {DrmIoctl::UuidUnregister, "PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER"},
        {DrmIoctl::DebuggerOpen, "PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN"},
        {DrmIoctl::GemClosReserve, "PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE"},
        {DrmIoctl::GemClosFree, "PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE"},
        {DrmIoctl::GemCacheReserve, "PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE"},
        {DrmIoctl::GemMmapOffset, "DRM_IOCTL_I915_GEM_MMAP_OFFSET"},
        {DrmIoctl::GemVmCreate, "DRM_IOCTL_I915_GEM_VM_CREATE"},
        {DrmIoctl::GemVmDestroy, "DRM_IOCTL_I915_GEM_VM_DESTROY"},
        {DrmIoctl::PrimeHandleToFd, "DRM_IOCTL_PRIME_HANDLE_TO_FD"}};
    for (auto &ioctlCodeString : ioctlCodeStringMap) {
        EXPECT_STREQ(getIoctlString(ioctlCodeString.first).c_str(), ioctlCodeString.second);
    }
}
TEST(DrmWrapperTest, WhenGettingDrmParamValueStringThenProperStringIsReturned) {
    std::map<DrmParam, const char *> ioctlCodeStringMap = {
        {DrmParam::ParamChipsetId, "I915_PARAM_CHIPSET_ID"},
        {DrmParam::ParamRevision, "I915_PARAM_REVISION"},
        {DrmParam::ParamHasExecSoftpin, "I915_PARAM_HAS_EXEC_SOFTPIN"},
        {DrmParam::ParamHasPooledEu, "I915_PARAM_HAS_POOLED_EU"},
        {DrmParam::ParamHasScheduler, "I915_PARAM_HAS_SCHEDULER"},
        {DrmParam::ParamEuTotal, "I915_PARAM_EU_TOTAL"},
        {DrmParam::ParamSubsliceTotal, "I915_PARAM_SUBSLICE_TOTAL"},
        {DrmParam::ParamMinEuInPool, "I915_PARAM_MIN_EU_IN_POOL"},
        {DrmParam::ParamCsTimestampFrequency, "I915_PARAM_CS_TIMESTAMP_FREQUENCY"},
        {DrmParam::ParamHasVmBind, "PRELIM_I915_PARAM_HAS_VM_BIND"},
        {DrmParam::ParamHasPageFault, "PRELIM_I915_PARAM_HAS_PAGE_FAULT"}};
    for (auto &ioctlCodeString : ioctlCodeStringMap) {
        EXPECT_STREQ(getDrmParamString(ioctlCodeString.first).c_str(), ioctlCodeString.second);
    }

    EXPECT_THROW(getDrmParamString(DrmParam::EngineClassRender), std::runtime_error);
}

TEST(IoctlHelperTest, whenGettingDrmParamValueThenProperValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto ioctlHelper = drm.getIoctlHelper();
    EXPECT_EQ(static_cast<int>(I915_ENGINE_CLASS_RENDER), ioctlHelper->getDrmParamValue(DrmParam::EngineClassRender));
    EXPECT_EQ(static_cast<int>(I915_ENGINE_CLASS_COPY), ioctlHelper->getDrmParamValue(DrmParam::EngineClassCopy));
    EXPECT_EQ(static_cast<int>(I915_ENGINE_CLASS_VIDEO), ioctlHelper->getDrmParamValue(DrmParam::EngineClassVideo));
    EXPECT_EQ(static_cast<int>(I915_ENGINE_CLASS_VIDEO_ENHANCE), ioctlHelper->getDrmParamValue(DrmParam::EngineClassVideoEnhance));
    EXPECT_EQ(static_cast<int>(I915_ENGINE_CLASS_INVALID), ioctlHelper->getDrmParamValue(DrmParam::EngineClassInvalid));
    EXPECT_EQ(static_cast<int>(I915_ENGINE_CLASS_INVALID_NONE), ioctlHelper->getDrmParamValue(DrmParam::EngineClassInvalidNone));

    EXPECT_THROW(ioctlHelper->getDrmParamValueBase(DrmParam::EngineClassCompute), std::runtime_error);
}

TEST(DistanceInfoTest, givenDistanceInfosWhenAssignRegionsFromDistancesThenCorrectRegionsSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto ioctlHelper = drm.getIoctlHelper();

    std::vector<MemoryRegion> memRegions(4);
    memRegions[0] = {{I915_MEMORY_CLASS_SYSTEM, 0}, 1024, 0};
    memRegions[1] = {{I915_MEMORY_CLASS_DEVICE, 0}, 1024, 0};
    memRegions[2] = {{I915_MEMORY_CLASS_DEVICE, 1}, 1024, 0};
    memRegions[3] = {{I915_MEMORY_CLASS_DEVICE, 2}, 1024, 0};
    auto memoryInfo = std::make_unique<MemoryInfo>(memRegions);

    std::vector<EngineClassInstance> engines(3);
    engines[0] = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassRender)), 0};
    engines[1] = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassCopy)), 0};
    engines[2] = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::EngineClassCopy)), 2};

    auto distances = std::vector<DistanceInfo>();
    for (const auto &region : memRegions) {
        if (region.region.memoryClass == I915_MEMORY_CLASS_SYSTEM) {
            continue;
        }
        for (const auto &engine : engines) {
            DistanceInfo dist{};
            dist.engine = engine;
            dist.region = {region.region.memoryClass, region.region.memoryInstance};
            dist.distance = (region.region.memoryInstance == engine.engineInstance) ? 0 : 100;
            distances.push_back(dist);
        }
    }
    memoryInfo->assignRegionsFromDistances(distances);
    EXPECT_EQ(1024u, memoryInfo->getMemoryRegionSize(1));
    EXPECT_EQ(1024u, memoryInfo->getMemoryRegionSize(2));
    EXPECT_EQ(0u, memoryInfo->getMemoryRegionSize(4));
}
