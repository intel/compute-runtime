/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/file_io.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/os_interface/linux/os_context_linux.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/os_interface/linux/drm_mock.h"

#include "gtest/gtest.h"

#include <fstream>
#include <memory>

using namespace NEO;
using namespace std;

TEST(DrmTest, GetDeviceID) {
    DrmMock *pDrm = new DrmMock;
    EXPECT_NE(nullptr, pDrm);

    pDrm->StoredDeviceID = 0x1234;
    int deviceID = 0;
    int ret = pDrm->getDeviceID(deviceID);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(pDrm->StoredDeviceID, deviceID);
    delete pDrm;
}

TEST(DrmTest, GivenConfigFileWithWrongDeviceIDWhenFrequencyIsQueriedThenReturnZero) {
    DrmMock *pDrm = new DrmMock;
    EXPECT_NE(nullptr, pDrm);

    pDrm->StoredDeviceID = 0x4321;
    int maxFrequency = 0;
    int ret = pDrm->getMaxGpuFrequency(maxFrequency);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(0, maxFrequency);

    delete pDrm;
}

TEST(DrmTest, GivenConfigFileWithWrongDeviceIDFailIoctlWhenFrequencyIsQueriedThenReturnZero) {
    DrmMock *pDrm = new DrmMock;
    EXPECT_NE(nullptr, pDrm);

    pDrm->StoredDeviceID = 0x4321;
    pDrm->StoredRetValForDeviceID = -1;
    int maxFrequency = 0;
    int ret = pDrm->getMaxGpuFrequency(maxFrequency);
    EXPECT_EQ(-1, ret);

    EXPECT_EQ(0, maxFrequency);

    delete pDrm;
}

TEST(DrmTest, GivenValidConfigFileWhenFrequencyIsQueriedThenValidValueIsReturned) {

    int expectedMaxFrequency = 1000;

    DrmMock *pDrm = new DrmMock;
    EXPECT_NE(nullptr, pDrm);

    pDrm->StoredDeviceID = 0x1234;

    string gpuFile = "test_files/devices/config";
    string gtMaxFreqFile = "test_files/devices/drm/card0/gt_max_freq_mhz";

    EXPECT_TRUE(fileExists(gpuFile));
    EXPECT_TRUE(fileExists(gtMaxFreqFile));

    int maxFrequency = 0;
    int ret = pDrm->getMaxGpuFrequency(maxFrequency);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(expectedMaxFrequency, maxFrequency);
    delete pDrm;
}

TEST(DrmTest, GivenNoConfigFileWhenFrequencyIsQueriedThenReturnZero) {
    DrmMock *pDrm = new DrmMock;
    EXPECT_NE(nullptr, pDrm);

    pDrm->StoredDeviceID = 0x1234;
    // change directory
    pDrm->setSysFsDefaultGpuPath("./");
    int maxFrequency = 0;
    int ret = pDrm->getMaxGpuFrequency(maxFrequency);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(0, maxFrequency);
    delete pDrm;
}

TEST(DrmTest, GetRevisionID) {
    DrmMock *pDrm = new DrmMock;
    EXPECT_NE(nullptr, pDrm);

    pDrm->StoredDeviceID = 0x1234;
    pDrm->StoredDeviceRevID = 0xB;
    int deviceID = 0;
    int ret = pDrm->getDeviceID(deviceID);
    EXPECT_EQ(0, ret);
    int revID = 0;
    ret = pDrm->getDeviceRevID(revID);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(pDrm->StoredDeviceID, deviceID);
    EXPECT_EQ(pDrm->StoredDeviceRevID, revID);

    delete pDrm;
}

TEST(DrmTest, GivenDrmWhenAskedFor48BitAddressCorrectValueReturned) {
    auto drm = make_unique<DrmMock>();
    EXPECT_TRUE(drm->is48BitAddressRangeSupported());
    drm->StoredRetVal = -1;
    EXPECT_TRUE(drm->is48BitAddressRangeSupported());
    drm->StoredRetVal = 0;
    drm->storedGTTSize = 1ull << 31;
    EXPECT_FALSE(drm->is48BitAddressRangeSupported());
    drm->storedGTTSize = MemoryConstants::max64BitAppAddress + 1;
    EXPECT_TRUE(drm->is48BitAddressRangeSupported());
    drm->storedGTTSize = MemoryConstants::max64BitAppAddress;
    EXPECT_FALSE(drm->is48BitAddressRangeSupported());
}

TEST(DrmTest, GivenDrmWhenAskedForPreemptionCorrectValueReturned) {
    DrmMock *pDrm = new DrmMock;
    pDrm->StoredRetVal = 0;
    pDrm->StoredPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    pDrm->checkPreemptionSupport();
    EXPECT_TRUE(pDrm->isPreemptionSupported());

    pDrm->StoredPreemptionSupport = 0;
    pDrm->checkPreemptionSupport();
    EXPECT_FALSE(pDrm->isPreemptionSupported());

    pDrm->StoredRetVal = -1;
    pDrm->StoredPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    pDrm->checkPreemptionSupport();
    EXPECT_FALSE(pDrm->isPreemptionSupported());

    pDrm->StoredPreemptionSupport = 0;
    pDrm->checkPreemptionSupport();
    EXPECT_FALSE(pDrm->isPreemptionSupported());

    delete pDrm;
}

TEST(DrmTest, GivenDrmWhenAskedForContextThatFailsThenFalseIsReturned) {
    DrmMock *pDrm = new DrmMock;
    pDrm->StoredRetVal = -1;
    EXPECT_THROW(pDrm->createDrmContext(), std::exception);
    pDrm->StoredRetVal = 0;
    delete pDrm;
}

TEST(DrmTest, givenDrmWhenOsContextIsCreatedThenCreateAndDestroyNewDrmOsContext) {
    DrmMock drmMock;

    uint32_t drmContextId1 = 123;
    uint32_t drmContextId2 = 456;

    {
        drmMock.StoredCtxId = drmContextId1;
        OsContextLinux osContext1(drmMock, 0u, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);

        EXPECT_EQ(1u, osContext1.getDrmContextIds().size());
        EXPECT_EQ(drmContextId1, osContext1.getDrmContextIds()[0]);
        EXPECT_EQ(0u, drmMock.receivedDestroyContextId);

        {
            drmMock.StoredCtxId = drmContextId2;
            OsContextLinux osContext2(drmMock, 0u, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
            EXPECT_EQ(1u, osContext2.getDrmContextIds().size());
            EXPECT_EQ(drmContextId2, osContext2.getDrmContextIds()[0]);
            EXPECT_EQ(0u, drmMock.receivedDestroyContextId);
        }
        EXPECT_EQ(drmContextId2, drmMock.receivedDestroyContextId);
    }

    EXPECT_EQ(drmContextId1, drmMock.receivedDestroyContextId);
    EXPECT_EQ(0u, drmMock.receivedContextParamRequestCount);
}

TEST(DrmTest, givenDrmPreemptionEnabledAndLowPriorityEngineWhenCreatingOsContextThenCallSetContextPriorityIoctl) {
    DrmMock drmMock;
    drmMock.StoredCtxId = 123;
    drmMock.preemptionSupported = false;

    OsContextLinux osContext1(drmMock, 0u, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    OsContextLinux osContext2(drmMock, 0u, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, true);

    EXPECT_EQ(0u, drmMock.receivedContextParamRequestCount);

    drmMock.preemptionSupported = true;

    OsContextLinux osContext3(drmMock, 0u, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, false);
    EXPECT_EQ(0u, drmMock.receivedContextParamRequestCount);

    OsContextLinux osContext4(drmMock, 0u, 1, aub_stream::ENGINE_RCS, PreemptionMode::Disabled, true);
    EXPECT_EQ(1u, drmMock.receivedContextParamRequestCount);
    EXPECT_EQ(drmMock.StoredCtxId, drmMock.receivedContextParamRequest.ctx_id);
    EXPECT_EQ(static_cast<uint64_t>(I915_CONTEXT_PARAM_PRIORITY), drmMock.receivedContextParamRequest.param);
    EXPECT_EQ(static_cast<uint64_t>(-1023), drmMock.receivedContextParamRequest.value);
    EXPECT_EQ(0u, drmMock.receivedContextParamRequest.size);
}

TEST(DrmTest, getExecSoftPin) {
    DrmMock *pDrm = new DrmMock;
    int execSoftPin = 0;

    int ret = pDrm->getExecSoftPin(execSoftPin);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, execSoftPin);

    pDrm->StoredExecSoftPin = 1;
    ret = pDrm->getExecSoftPin(execSoftPin);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, execSoftPin);

    delete pDrm;
}

TEST(DrmTest, enableTurboBoost) {
    DrmMock *pDrm = new DrmMock;

    int ret = pDrm->enableTurboBoost();
    EXPECT_EQ(0, ret);

    delete pDrm;
}

TEST(DrmTest, getEnabledPooledEu) {
    DrmMock *pDrm = new DrmMock;

    int enabled = 0;
    int ret = 0;
    pDrm->StoredHasPooledEU = -1;
#if defined(I915_PARAM_HAS_POOLED_EU)
    ret = pDrm->getEnabledPooledEu(enabled);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(-1, enabled);

    pDrm->StoredHasPooledEU = 0;
    ret = pDrm->getEnabledPooledEu(enabled);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, enabled);

    pDrm->StoredHasPooledEU = 1;
    ret = pDrm->getEnabledPooledEu(enabled);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, enabled);

    pDrm->StoredRetValForPooledEU = -1;
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

TEST(DrmTest, getMinEuInPool) {
    DrmMock *pDrm = new DrmMock;

    pDrm->StoredMinEUinPool = -1;
    int minEUinPool = 0;
    int ret = 0;
#if defined(I915_PARAM_MIN_EU_IN_POOL)
    ret = pDrm->getMinEuInPool(minEUinPool);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(-1, minEUinPool);

    pDrm->StoredMinEUinPool = 0;
    ret = pDrm->getMinEuInPool(minEUinPool);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(0, minEUinPool);

    pDrm->StoredMinEUinPool = 1;
    ret = pDrm->getMinEuInPool(minEUinPool);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1, minEUinPool);

    pDrm->StoredRetValForMinEUinPool = -1;
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
    DrmMock *pDrm = new DrmMock;
    EXPECT_NE(nullptr, pDrm);

    auto errnoFromDrm = pDrm->getErrno();
    EXPECT_EQ(errno, errnoFromDrm);
    delete pDrm;
}
TEST(DrmTest, givenPlatformWhereGetSseuRetFailureWhenCallSetQueueSliceCountThenSliceCountIsNotSet) {
    uint64_t newSliceCount = 1;
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>();
    drm->StoredRetValForGetSSEU = -1;
    drm->checkQueueSliceSupport();

    EXPECT_FALSE(drm->sliceCountChangeSupported);
    EXPECT_FALSE(drm->setQueueSliceCount(newSliceCount));
    EXPECT_NE(drm->getSliceMask(newSliceCount), drm->storedParamSseu);
}

TEST(DrmTest, givenPlatformWhereSetSseuRetFailureWhenCallSetQueueSliceCountThenReturnFalse) {
    uint64_t newSliceCount = 1;
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>();
    drm->StoredRetValForSetSSEU = -1;
    drm->StoredRetValForGetSSEU = 0;
    drm->checkQueueSliceSupport();

    EXPECT_TRUE(drm->sliceCountChangeSupported);
    EXPECT_FALSE(drm->setQueueSliceCount(newSliceCount));
}

TEST(DrmTest, givenPlatformWithSupportToChangeSliceCountWhenCallSetQueueSliceCountThenReturnTrue) {
    uint64_t newSliceCount = 1;
    std::unique_ptr<DrmMock> drm = std::make_unique<DrmMock>();
    drm->StoredRetValForSetSSEU = 0;
    drm->StoredRetValForSetSSEU = 0;
    drm->checkQueueSliceSupport();

    EXPECT_TRUE(drm->sliceCountChangeSupported);
    EXPECT_TRUE(drm->setQueueSliceCount(newSliceCount));
    drm_i915_gem_context_param_sseu sseu = {};
    EXPECT_EQ(0, drm->getQueueSliceCount(&sseu));
    EXPECT_EQ(drm->getSliceMask(newSliceCount), sseu.slice_mask);
}
