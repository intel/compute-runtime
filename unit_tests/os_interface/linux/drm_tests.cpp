/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/options.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/helpers/file_io.h"
#include "unit_tests/os_interface/linux/drm_mock.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "runtime/os_interface/os_interface.h"
#include <fstream>

using namespace OCLRT;
using namespace std;

TEST(DrmTest, GetDeviceID) {
    Drm2 *pDrm = new Drm2;
    EXPECT_NE(nullptr, pDrm);

    pDrm->StoredDeviceID = 0x1234;
    int deviceID = 0;
    int ret = pDrm->getDeviceID(deviceID);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(pDrm->StoredDeviceID, deviceID);
    delete pDrm;
}

TEST(DrmTest, GivenConfigFileWithWrongDeviceIDWhenFrequencyIsQueriedThenReturnZero) {
    Drm2 *pDrm = new Drm2;
    EXPECT_NE(nullptr, pDrm);

    pDrm->StoredDeviceID = 0x4321;
    int maxFrequency = 0;
    int ret = pDrm->getMaxGpuFrequency(maxFrequency);
    EXPECT_EQ(0, ret);

    EXPECT_EQ(0, maxFrequency);

    delete pDrm;
}

TEST(DrmTest, GivenConfigFileWithWrongDeviceIDFailIoctlWhenFrequencyIsQueriedThenReturnZero) {
    Drm2 *pDrm = new Drm2;
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

    Drm2 *pDrm = new Drm2;
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
    Drm2 *pDrm = new Drm2;
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
    Drm2 *pDrm = new Drm2;
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

TEST(DrmTest, GivenMockDrmWhenAskedForCoherencyStatusThenProperBitIsSet) {
    Drm2 *pDrm = new Drm2;
    EXPECT_NE(nullptr, pDrm);

    EXPECT_FALSE(pDrm->peekCoherencyDisablePatchActive());

    pDrm->obtainCoherencyDisablePatchActive();

    EXPECT_TRUE(pDrm->peekCoherencyDisablePatchActive());
    delete pDrm;
}

TEST(DrmTest, GivenMockDrmWhenAskedForCoherencyStatusThatPassThenDisabledIsReturned) {
    Drm2 *pDrm = new Drm2;
    pDrm->StoredDisableCoherencyPatchActive = 0;
    EXPECT_NE(nullptr, pDrm);

    EXPECT_FALSE(pDrm->peekCoherencyDisablePatchActive());

    pDrm->obtainCoherencyDisablePatchActive();

    EXPECT_FALSE(pDrm->peekCoherencyDisablePatchActive());
    pDrm->StoredDisableCoherencyPatchActive = 1;
    delete pDrm;
}

TEST(DrmTest, GivenMockDrmWhenAskedForCoherencyStatusThatFailsThenFalseIsReturned) {
    Drm2 *pDrm = new Drm2;
    pDrm->StoredRetVal = -1;
    EXPECT_NE(nullptr, pDrm);

    EXPECT_FALSE(pDrm->peekCoherencyDisablePatchActive());

    pDrm->obtainCoherencyDisablePatchActive();

    EXPECT_FALSE(pDrm->peekCoherencyDisablePatchActive());
    pDrm->StoredRetVal = 0;
    delete pDrm;
}

TEST(DrmTest, GivenMockDrmWhenAskedFor48BitAddressCorrectValueReturned) {
    Drm2 *pDrm = new Drm2;
    pDrm->StoredPPGTT = 3;
    EXPECT_TRUE(pDrm->is48BitAddressRangeSupported());
    pDrm->StoredPPGTT = 2;
    EXPECT_FALSE(pDrm->is48BitAddressRangeSupported());
    delete pDrm;
}

ACTION_P2(saveGetParamData, saveParamPtr, forceReturnValuePtr) {
    auto getParamArg = static_cast<drm_i915_getparam_t *>(arg1);
    *saveParamPtr = getParamArg->param;
    *getParamArg->value = forceReturnValuePtr;
}
struct DrmDataPortCoherencyTests : public ::testing::Test {
    struct MyMockDrm : public Drm2 {
        MyMockDrm() : Drm2(){};
        MOCK_METHOD2(ioctl, int(unsigned long request, void *arg));
    } drm;

    void setupExpectCall(int expectedRetVal, int expectedGetParamValue) {
        using namespace ::testing;
        auto saveAndReturnAction = DoAll(saveGetParamData(&receivedGetParamType, expectedGetParamValue),
                                         Return(expectedRetVal));
        EXPECT_CALL(drm, ioctl(DRM_IOCTL_I915_GETPARAM, _)).Times(1).WillOnce(saveAndReturnAction);
    }
    int receivedGetParamType = 0;
};

TEST_F(DrmDataPortCoherencyTests, givenDisabledPatchWhenAskedToObtainDataPortCoherencyPatchThenReturnFlase) {
    setupExpectCall(1, 0); // return error == 1, dont care about assigned feature value
    drm.obtainDataPortCoherencyPatchActive();

    EXPECT_EQ(receivedGetParamType, I915_PARAM_HAS_EXEC_DATA_PORT_COHERENCY);
    EXPECT_FALSE(drm.peekDataPortCoherencyPatchActive());
}

TEST_F(DrmDataPortCoherencyTests, givenEnabledPatchAndDisabledFeatureWhenAskedToObtainDataPortCoherencyPatchThenReturnFlase) {
    setupExpectCall(0, 0); // return success(0), set disabled feature (0)
    drm.obtainDataPortCoherencyPatchActive();

    EXPECT_EQ(receivedGetParamType, I915_PARAM_HAS_EXEC_DATA_PORT_COHERENCY);
    EXPECT_FALSE(drm.peekDataPortCoherencyPatchActive());
}

TEST_F(DrmDataPortCoherencyTests, givenEnabledPatchAndEnabledFeatureWhenAskedToObtainDataPortCoherencyPatchThenReturnTrue) {
    setupExpectCall(0, 1); // return success(0), set enabled feature (1)
    drm.obtainDataPortCoherencyPatchActive();

    EXPECT_EQ(receivedGetParamType, I915_PARAM_HAS_EXEC_DATA_PORT_COHERENCY);
    EXPECT_TRUE(drm.peekDataPortCoherencyPatchActive());
}

#if defined(I915_PARAM_HAS_PREEMPTION)
TEST(DrmTest, GivenMockDrmWhenAskedForPreemptionCorrectValueReturned) {
    Drm2 *pDrm = new Drm2;
    pDrm->StoredPreemptionSupport = 1;
    EXPECT_TRUE(pDrm->hasPreemption());
    pDrm->StoredPreemptionSupport = 0;
    EXPECT_FALSE(pDrm->hasPreemption());
    delete pDrm;
}

TEST(DrmTest, GivenMockDrmWhenAskedForContextThatPassedThenValidContextIdsReturned) {
    Drm2 *pDrm = new Drm2;
    EXPECT_EQ(0u, pDrm->lowPriorityContextId);
    pDrm->StoredRetVal = 0;
    pDrm->StoredCtxId = 2;
    EXPECT_TRUE(pDrm->contextCreate());
    EXPECT_EQ(2u, pDrm->lowPriorityContextId);
    pDrm->StoredRetVal = 0;
    pDrm->StoredCtxId = 1;
    delete pDrm;
}

TEST(DrmTest, GivenMockDrmWhenAskedForContextThatFailsThenFalseIsReturned) {
    Drm2 *pDrm = new Drm2;
    pDrm->StoredRetVal = -1;
    EXPECT_FALSE(pDrm->contextCreate());
    pDrm->StoredRetVal = 0;
    delete pDrm;
}

TEST(DrmTest, GivenMockDrmWhenAskedForContextWithLowPriorityThatFailsThenFalseIsReturned) {
    Drm2 *pDrm = new Drm2;
    EXPECT_TRUE(pDrm->contextCreate());
    pDrm->StoredRetVal = -1;
    EXPECT_FALSE(pDrm->setLowPriority());
    pDrm->StoredRetVal = 0;
    delete pDrm;
}
#endif

TEST(DrmTest, getExecSoftPin) {
    Drm2 *pDrm = new Drm2;
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
    Drm2 *pDrm = new Drm2;

    int ret = pDrm->enableTurboBoost();
    EXPECT_EQ(0, ret);

    delete pDrm;
}

TEST(DrmTest, getEnabledPooledEu) {
    Drm2 *pDrm = new Drm2;

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
    Drm2 *pDrm = new Drm2;

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
    Drm2 *pDrm = new Drm2;
    EXPECT_NE(nullptr, pDrm);

    auto errnoFromDrm = pDrm->getErrno();
    EXPECT_EQ(errno, errnoFromDrm);
    delete pDrm;
}
