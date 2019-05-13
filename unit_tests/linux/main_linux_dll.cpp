/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/basic_math.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/linux/allocator_helper.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "test.h"
#include "unit_tests/custom_event_listener.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/linux/drm_wrap.h"
#include "unit_tests/linux/mock_os_layer.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <string>

using namespace NEO;

class DrmTestsFixture {
  public:
    void SetUp() {
    }

    void TearDown() {
        DrmWrap::closeDevice(0);
    }
};

typedef Test<DrmTestsFixture> DrmTests;

void initializeTestedDevice() {
    for (uint32_t i = 0; deviceDescriptorTable[i].eGtType != GTTYPE::GTTYPE_UNDEFINED; i++) {
        if (platformDevices[0]->platform.eProductFamily == deviceDescriptorTable[i].pHwInfo->platform.eProductFamily) {
            deviceId = deviceDescriptorTable[i].deviceId;
            break;
        }
    }
}

TEST_F(DrmTests, getReturnsNull) {
    auto drm = Drm::get(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, getNoOverrun) {
    //negative device ordinal
    auto drm = Drm::get(-1);
    EXPECT_EQ(drm, nullptr);

    //some high value
    drm = Drm::get(1 << (sizeof(int32_t) * 8 - 2));
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, closeNotOpened) {
    auto drm = DrmWrap::get(0);
    EXPECT_EQ(drm, nullptr);

    DrmWrap::closeDevice(0);

    DrmWrap::get(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, openClose) {
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);

    DrmWrap::closeDevice(0);

    drm = DrmWrap::get(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, closeNoOverrun) {
    //negative device ordinal
    DrmWrap::closeDevice(-1);

    //some high value
    DrmWrap::closeDevice(1 << (sizeof(int32_t) * 8 - 2));
}

TEST_F(DrmTests, createReturnsDrm) {
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);

    drm_i915_getparam_t getParam;
    int lDeviceId;

    VariableBackup<decltype(ioctlCnt)> backupIoctlCnt(&ioctlCnt);
    VariableBackup<int> backupIoctlSeq(&ioctlSeq[0]);

    ioctlCnt = 0;
    ioctlSeq[0] = -1;
    errno = EINTR;
    // check if device works, although there was EINTR error from KMD
    getParam.param = I915_PARAM_CHIPSET_ID;
    getParam.value = &lDeviceId;
    auto ret = drm->ioctl(DRM_IOCTL_I915_GETPARAM, &getParam);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(deviceId, lDeviceId);

    ioctlCnt = 0;
    ioctlSeq[0] = -1;
    errno = EAGAIN;
    // check if device works, although there was EAGAIN error from KMD
    getParam.param = I915_PARAM_CHIPSET_ID;
    getParam.value = &lDeviceId;
    ret = drm->ioctl(DRM_IOCTL_I915_GETPARAM, &getParam);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(deviceId, lDeviceId);

    ioctlCnt = 0;
    ioctlSeq[0] = -1;
    errno = 0;
    // we failed with any other error code
    getParam.param = I915_PARAM_CHIPSET_ID;
    getParam.value = &lDeviceId;
    ret = drm->ioctl(DRM_IOCTL_I915_GETPARAM, &getParam);
    EXPECT_EQ(-1, ret);
    EXPECT_EQ(deviceId, lDeviceId);
}

TEST_F(DrmTests, createTwiceReturnsSameDrm) {
    auto drm1 = DrmWrap::createDrm(0);
    EXPECT_NE(drm1, nullptr);
    auto drm2 = DrmWrap::createDrm(0);
    EXPECT_NE(drm2, nullptr);
    EXPECT_EQ(drm1, drm2);
}

TEST_F(DrmTests, createDriFallback) {
    VariableBackup<decltype(haveDri)> backupHaveDri(&haveDri);

    haveDri = 1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
}

TEST_F(DrmTests, createNoDevice) {
    VariableBackup<decltype(haveDri)> backupHaveDri(&haveDri);
    haveDri = -1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, createNoOverrun) {
    auto drm = DrmWrap::createDrm(-1);
    EXPECT_EQ(drm, nullptr);

    drm = DrmWrap::createDrm(1 << (sizeof(int32_t) * 8 - 2));
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, createUnknownDevice) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.PrintDebugMessages.set(true);

    VariableBackup<decltype(deviceId)> backupDeviceId(&deviceId);

    deviceId = -1;

    ::testing::internal::CaptureStderr();
    auto drm = DrmWrap::createDrm(0);
    EXPECT_EQ(drm, nullptr);
    std::string errStr = ::testing::internal::GetCapturedStderr();
    EXPECT_THAT(errStr, ::testing::HasSubstr(std::string("FATAL: Unknown device: deviceId: ffffffff, revisionId: 0000")));
}

TEST_F(DrmTests, createNoSoftPin) {
    VariableBackup<decltype(haveSoftPin)> backupHaveSoftPin(&haveSoftPin);
    haveSoftPin = 0;

    auto drm = DrmWrap::createDrm(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, failOnDeviceId) {
    VariableBackup<decltype(failOnDeviceId)> backupFailOnDeviceId(&failOnDeviceId);
    failOnDeviceId = -1;

    auto drm = DrmWrap::createDrm(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, failOnRevisionId) {
    VariableBackup<decltype(failOnRevisionId)> backupFailOnRevisionId(&failOnRevisionId);
    failOnRevisionId = -1;

    auto drm = DrmWrap::createDrm(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, failOnSoftPin) {
    VariableBackup<decltype(failOnSoftPin)> backupFailOnSoftPin(&failOnSoftPin);
    failOnSoftPin = -1;

    auto drm = DrmWrap::createDrm(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, failOnParamBoost) {
    VariableBackup<decltype(failOnParamBoost)> backupFailOnParamBoost(&failOnParamBoost);
    failOnParamBoost = -1;

    auto drm = DrmWrap::createDrm(0);
    //non-fatal error - issue warning only
    EXPECT_NE(drm, nullptr);
}

#ifdef TESTS_BDW
TEST_F(DrmTests, givenKernelNotSupportingTurboPatchWhenBdwDeviceIsCreatedThenSimplifiedMocsSelectionIsFalse) {
    VariableBackup<decltype(deviceId)> backupDeviceId(&deviceId);
    VariableBackup<decltype(failOnParamBoost)> backupFailOnParamBoost(&failOnParamBoost);

    deviceId = IBDW_GT3_WRK_DEVICE_F0_ID;
    failOnParamBoost = -1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
    EXPECT_FALSE(drm->getSimplifiedMocsTableUsage());
}
#endif

#ifdef TESTS_SKL
TEST_F(DrmTests, givenKernelNotSupportingTurboPatchWhenSklDeviceIsCreatedThenSimplifiedMocsSelectionIsTrue) {
    VariableBackup<decltype(deviceId)> backupDeviceId(&deviceId);
    VariableBackup<decltype(failOnParamBoost)> backupFailOnParamBoost(&failOnParamBoost);

    deviceId = ISKL_GT2_DT_DEVICE_F0_ID;
    failOnParamBoost = -1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
    EXPECT_TRUE(drm->getSimplifiedMocsTableUsage());
}
#endif
#ifdef TESTS_KBL
TEST_F(DrmTests, givenKernelNotSupportingTurboPatchWhenKblDeviceIsCreatedThenSimplifiedMocsSelectionIsTrue) {
    VariableBackup<decltype(deviceId)> backupDeviceId(&deviceId);
    VariableBackup<decltype(failOnParamBoost)> backupFailOnParamBoost(&failOnParamBoost);

    deviceId = IKBL_GT1_ULT_DEVICE_F0_ID;
    failOnParamBoost = -1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
    EXPECT_TRUE(drm->getSimplifiedMocsTableUsage());
}
#endif
#ifdef TESTS_BXT
TEST_F(DrmTests, givenKernelNotSupportingTurboPatchWhenBxtDeviceIsCreatedThenSimplifiedMocsSelectionIsTrue) {
    VariableBackup<decltype(deviceId)> backupDeviceId(&deviceId);
    VariableBackup<decltype(failOnParamBoost)> backupFailOnParamBoost(&failOnParamBoost);

    deviceId = IBXT_X_DEVICE_F0_ID;
    failOnParamBoost = -1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
    EXPECT_TRUE(drm->getSimplifiedMocsTableUsage());
}
#endif
#ifdef TESTS_GLK
TEST_F(DrmTests, givenKernelNotSupportingTurboPatchWhenGlkDeviceIsCreatedThenSimplifiedMocsSelectionIsTrue) {
    VariableBackup<decltype(deviceId)> backupDeviceId(&deviceId);
    VariableBackup<decltype(failOnParamBoost)> backupFailOnParamBoost(&failOnParamBoost);

    deviceId = IGLK_GT2_ULT_18EU_DEVICE_F0_ID;
    failOnParamBoost = -1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
    EXPECT_TRUE(drm->getSimplifiedMocsTableUsage());
}
#endif
#ifdef TESTS_CFL
TEST_F(DrmTests, givenKernelNotSupportingTurboPatchWhenCflDeviceIsCreatedThenSimplifiedMocsSelectionIsTrue) {
    VariableBackup<decltype(deviceId)> backupDeviceId(&deviceId);
    VariableBackup<decltype(failOnParamBoost)> backupFailOnParamBoost(&failOnParamBoost);

    deviceId = ICFL_GT1_S61_DT_DEVICE_F0_ID;
    failOnParamBoost = -1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
    EXPECT_TRUE(drm->getSimplifiedMocsTableUsage());
}
#endif

TEST_F(DrmTests, givenKernelSupportingTurboPatchWhenDeviceIsCreatedThenSimplifiedMocsSelectionIsFalse) {
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
    EXPECT_FALSE(drm->getSimplifiedMocsTableUsage());
}

TEST_F(DrmTests, failOnContextCreate) {
    VariableBackup<decltype(failOnContextCreate)> backupFailOnContextCreate(&failOnContextCreate);

    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
    failOnContextCreate = -1;
    EXPECT_THROW(drm->createDrmContext(), std::exception);
    EXPECT_FALSE(drm->isPreemptionSupported());
    failOnContextCreate = 0;
    DrmWrap::closeDevice(0);

    drm = DrmWrap::get(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, failOnSetPriority) {
    VariableBackup<decltype(failOnSetPriority)> backupFailOnSetPriority(&failOnSetPriority);

    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
    failOnSetPriority = -1;
    auto drmContext = drm->createDrmContext();
    EXPECT_THROW(drm->setLowPriorityContextParam(drmContext), std::exception);
    EXPECT_FALSE(drm->isPreemptionSupported());
    failOnSetPriority = 0;
    DrmWrap::closeDevice(0);

    drm = DrmWrap::get(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, failOnDrmGetVersion) {
    VariableBackup<decltype(failOnDrmVersion)> backupFailOnDrmVersion(&failOnDrmVersion);

    failOnDrmVersion = -1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_EQ(drm, nullptr);
    failOnDrmVersion = 0;
    DrmWrap::closeDevice(0);

    drm = DrmWrap::get(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, failOnInvalidDeviceName) {
    VariableBackup<decltype(failOnDrmVersion)> backupFailOnDrmVersion(&failOnDrmVersion);

    strcpy(providedDrmVersion, "NA");
    auto drm = DrmWrap::createDrm(0);
    EXPECT_EQ(drm, nullptr);
    failOnDrmVersion = 0;
    strcpy(providedDrmVersion, "i915");
    DrmWrap::closeDevice(0);

    drm = DrmWrap::get(0);
    EXPECT_EQ(drm, nullptr);
}

TEST(AllocatorHelper, givenExpectedSizeToMapWhenGetSizetoMapCalledThenExpectedValueReturned) {
    EXPECT_EQ((alignUp(4 * GB - 8096, 4096)), NEO::getSizeToMap());
}

TEST(DrmMemoryManagerCreate, whenCallCreateMemoryManagerThenDrmMemoryManagerIsCreated) {
    DrmMockSuccess mock;
    MockExecutionEnvironment executionEnvironment(*platformDevices);

    executionEnvironment.osInterface = std::make_unique<OSInterface>();
    executionEnvironment.osInterface->get()->setDrm(&mock);
    auto drmMemoryManager = MemoryManager::createMemoryManager(executionEnvironment);
    EXPECT_NE(nullptr, drmMemoryManager.get());
    executionEnvironment.memoryManager = std::move(drmMemoryManager);
}

int main(int argc, char **argv) {
    bool useDefaultListener = false;

    ::testing::InitGoogleTest(&argc, argv);

    // parse remaining args assuming they're mine
    for (int i = 1; i < argc; ++i) {
        if (!strcmp("--disable_default_listener", argv[i])) {
            useDefaultListener = false;
        } else if (!strcmp("--enable_default_listener", argv[i])) {
            useDefaultListener = true;
        }
    }

    if (useDefaultListener == false) {
        auto &listeners = ::testing::UnitTest::GetInstance()->listeners();
        auto defaultListener = listeners.default_result_printer();
        auto customEventListener = new CCustomEventListener(defaultListener);

        listeners.Release(defaultListener);
        listeners.Append(customEventListener);
    }

    initializeTestedDevice();

    auto retVal = RUN_ALL_TESTS();

    return retVal;
}
