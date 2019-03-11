/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/os_interface/linux/allocator_helper.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "test.h"
#include "unit_tests/custom_event_listener.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/os_interface/linux/device_command_stream_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "mock_os_layer.h"

#include <string>

using namespace OCLRT;

class DrmWrap : public Drm {
  public:
    static Drm *createDrm(int32_t deviceOrdinal) {
        return Drm::create(deviceOrdinal);
    }
    static void closeDevice(int32_t deviceOrdinal) {
        Drm::closeDevice(deviceOrdinal);
    };
};

class DrmTestsFixture {
  public:
    void SetUp() {
        //make static things into initial state
        DrmWrap::closeDevice(0);
        resetOSMockGlobalState();
    }

    void TearDown() {
    }
};

typedef Test<DrmTestsFixture> DrmTests;

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
    haveDri = 1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
}

TEST_F(DrmTests, createNoDevice) {
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
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.PrintDebugMessages.set(true);

    deviceId = -1;

    ::testing::internal::CaptureStderr();
    auto drm = DrmWrap::createDrm(0);
    EXPECT_EQ(drm, nullptr);
    std::string errStr = ::testing::internal::GetCapturedStderr();
    EXPECT_THAT(errStr, ::testing::HasSubstr(std::string("FATAL: Unknown device: deviceId: ffffffff, revisionId: 0000")));
}

TEST_F(DrmTests, createNoSoftPin) {
    haveSoftPin = 0;

    auto drm = DrmWrap::createDrm(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, failOnDeviceId) {
    failOnDeviceId = -1;

    auto drm = DrmWrap::createDrm(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, failOnRevisionId) {
    failOnRevisionId = -1;

    auto drm = DrmWrap::createDrm(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, failOnSoftPin) {
    failOnSoftPin = -1;

    auto drm = DrmWrap::createDrm(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, failOnParamBoost) {
    failOnParamBoost = -1;

    auto drm = DrmWrap::createDrm(0);
    //non-fatal error - issue warning only
    EXPECT_NE(drm, nullptr);
}

#ifdef SUPPORT_BDW
TEST_F(DrmTests, givenKernelNotSupportingTurboPatchWhenBdwDeviceIsCreatedThenSimplifiedMocsSelectionIsFalse) {
    deviceId = IBDW_GT3_WRK_DEVICE_F0_ID;
    failOnParamBoost = -1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
    EXPECT_FALSE(drm->getSimplifiedMocsTableUsage());
}
#endif

#ifdef SUPPORT_SKL
TEST_F(DrmTests, givenKernelNotSupportingTurboPatchWhenSklDeviceIsCreatedThenSimplifiedMocsSelectionIsTrue) {
    deviceId = ISKL_GT2_DT_DEVICE_F0_ID;
    failOnParamBoost = -1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
    EXPECT_TRUE(drm->getSimplifiedMocsTableUsage());
}
#endif
#ifdef SUPPORT_KBL
TEST_F(DrmTests, givenKernelNotSupportingTurboPatchWhenKblDeviceIsCreatedThenSimplifiedMocsSelectionIsTrue) {
    deviceId = IKBL_GT1_ULT_DEVICE_F0_ID;
    failOnParamBoost = -1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
    EXPECT_TRUE(drm->getSimplifiedMocsTableUsage());
}
#endif
#ifdef SUPPORT_BXT
TEST_F(DrmTests, givenKernelNotSupportingTurboPatchWhenBxtDeviceIsCreatedThenSimplifiedMocsSelectionIsTrue) {
    deviceId = IBXT_X_DEVICE_F0_ID;
    failOnParamBoost = -1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
    EXPECT_TRUE(drm->getSimplifiedMocsTableUsage());
}
#endif
#ifdef SUPPORT_GLK
TEST_F(DrmTests, givenKernelNotSupportingTurboPatchWhenGlkDeviceIsCreatedThenSimplifiedMocsSelectionIsTrue) {
    deviceId = IGLK_GT2_ULT_18EU_DEVICE_F0_ID;
    failOnParamBoost = -1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
    EXPECT_TRUE(drm->getSimplifiedMocsTableUsage());
}
#endif
#ifdef SUPPORT_CFL
TEST_F(DrmTests, givenKernelNotSupportingTurboPatchWhenCflDeviceIsCreatedThenSimplifiedMocsSelectionIsTrue) {
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

TEST_F(DrmTests, checkPreemption) {
    auto drm = DrmWrap::createDrm(0);
    EXPECT_NE(drm, nullptr);
    drm->checkPreemptionSupport();
#if defined(I915_PARAM_HAS_PREEMPTION)
    EXPECT_TRUE(drm->isPreemptionSupported());
#else
    EXPECT_FALSE(drm->isPreemptionSupported());
#endif
    DrmWrap::closeDevice(0);
    drm = DrmWrap::get(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, failOnContextCreate) {
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
    failOnDrmVersion = -1;
    auto drm = DrmWrap::createDrm(0);
    EXPECT_EQ(drm, nullptr);
    failOnDrmVersion = 0;
    DrmWrap::closeDevice(0);

    drm = DrmWrap::get(0);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, failOnInvalidDeviceName) {
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
    EXPECT_EQ((alignUp(4 * GB - 8096, 4096)), OCLRT::getSizeToMap());
}

TEST(DrmMemoryManagerCreate, givenLocalMemoryEnabledWhenMemoryManagerIsCreatedThenLocalMemoryEnabledIsTrue) {
    DrmMockSuccess mock;
    ExecutionEnvironment executionEnvironment;

    executionEnvironment.osInterface = std::make_unique<OSInterface>();
    executionEnvironment.osInterface->get()->setDrm(&mock);

    auto drmMemoryManager = MemoryManager::createMemoryManager(false, true, executionEnvironment);
    EXPECT_TRUE(drmMemoryManager->isLocalMemorySupported());
    executionEnvironment.memoryManager = std::move(drmMemoryManager);
}

TEST(DrmMemoryManagerCreate, givenLocalMemoryDisabledWhenMemoryManagerIsCreatedThenLocalMemoryEnabledIsFalse) {
    DrmMockSuccess mock;
    ExecutionEnvironment executionEnvironment;

    executionEnvironment.osInterface = std::make_unique<OSInterface>();
    executionEnvironment.osInterface->get()->setDrm(&mock);
    auto drmMemoryManager = MemoryManager::createMemoryManager(false, false, executionEnvironment);
    EXPECT_FALSE(drmMemoryManager->isLocalMemorySupported());
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

    auto retVal = RUN_ALL_TESTS();

    return retVal;
}
