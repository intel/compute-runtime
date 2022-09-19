/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/direct_submission_controller.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/linux/allocator_helper.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/custom_event_listener.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.inl"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/ult_hw_config.inl"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/signal_utils.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/linux/drm_wrap.h"
#include "opencl/test/unit_test/linux/mock_os_layer.h"

#include "gtest/gtest.h"
#include "os_inc.h"

#include <string>
#include <string_view>

namespace Os {
extern const char *dxcoreDllName;
}

namespace NEO {
void __attribute__((destructor)) platformsDestructor();
extern const DeviceDescriptor deviceDescriptorTable[];
} // namespace NEO

NEO::OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo, uint64_t gpuAddressSpace) {
    return nullptr;
}

using namespace NEO;

class DrmTestsFixture {
  public:
    void setUp() {
        if (deviceDescriptorTable[0].deviceId == 0) {
            GTEST_SKIP();
        }

        executionEnvironment.prepareRootDeviceEnvironments(1);
        rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    }

    void tearDown() {
    }
    ExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
};

typedef Test<DrmTestsFixture> DrmTests;

void initializeTestedDevice() {
    for (uint32_t i = 0; deviceDescriptorTable[i].deviceId != 0; i++) {
        if (defaultHwInfo->platform.eProductFamily == deviceDescriptorTable[i].pHwInfo->platform.eProductFamily) {
            deviceId = deviceDescriptorTable[i].deviceId;
            break;
        }
    }
}

int openRetVal = 0;
std::string lastOpenedPath;
int testOpen(const char *fullPath, int, ...) {
    return openRetVal;
};

int openCounter = 1;
int openWithCounter(const char *fullPath, int, ...) {
    if (openCounter > 0) {
        if (fullPath) {
            lastOpenedPath = fullPath;
        }
        openCounter--;
        return 1023; // valid file descriptor for ULT
    }
    return -1;
};

struct DrmSimpleTests : public ::testing::Test {
    void SetUp() override {
        if (deviceDescriptorTable[0].deviceId == 0) {
            GTEST_SKIP();
        }
    }
};

TEST_F(DrmSimpleTests, GivenTwoOpenableDevicesWhenDiscoverDevicesThenCreateTwoHwDeviceIds) {
    VariableBackup<decltype(openFull)> backupOpenFull(&openFull);
    openFull = openWithCounter;
    openCounter = 2;
    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(2u, hwDeviceIds.size());
}

TEST_F(DrmSimpleTests, GivenSelectedNotExistingDeviceUsingFilterBdfWhenGetDeviceFdThenFail) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.FilterBdfPath.set("invalid");
    VariableBackup<decltype(openFull)> backupOpenFull(&openFull);
    openFull = nullptr; // open shouldn't be called
    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_TRUE(hwDeviceIds.empty());
}

TEST_F(DrmSimpleTests, GivenSelectedExistingDeviceUsingFilterBdfWhenGetDeviceFdThenReturnFd) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.FilterBdfPath.set("0000:00:02.0");
    VariableBackup<decltype(openFull)> backupOpenFull(&openFull);
    openFull = openWithCounter;
    openCounter = 10;
    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(1u, hwDeviceIds.size());
    EXPECT_NE(nullptr, hwDeviceIds[0].get());
    EXPECT_STREQ("/dev/dri/by-path/platform-4010000000.pcie-pci-0000:00:02.0-render", lastOpenedPath.c_str());
    EXPECT_EQ(9, openCounter); // only one opened file
}

TEST_F(DrmSimpleTests, GivenSelectedExistingDeviceWhenOpenDirSuccedsThenHwDeviceIdsHaveProperPciPaths) {
    VariableBackup<decltype(openFull)> backupOpenFull(&openFull);
    VariableBackup<decltype(failOnOpenDir)> backupOpenDir(&failOnOpenDir, false);
    VariableBackup<decltype(entryIndex)> backupEntryIndex(&entryIndex, 0u);
    openFull = openWithCounter;

    ExecutionEnvironment executionEnvironment;

    entryIndex = 0;
    openCounter = 1;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(1u, hwDeviceIds.size());
    EXPECT_NE(nullptr, hwDeviceIds[0].get());
    EXPECT_STREQ("0000:00:03.1", hwDeviceIds[0]->as<HwDeviceIdDrm>()->getPciPath());

    entryIndex = 0;
    openCounter = 2;
    hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(2u, hwDeviceIds.size());
    EXPECT_NE(nullptr, hwDeviceIds[0].get());
    EXPECT_STREQ("0000:00:03.1", hwDeviceIds[0]->as<HwDeviceIdDrm>()->getPciPath());
    EXPECT_NE(nullptr, hwDeviceIds[1].get());
    EXPECT_STREQ("0000:00:02.0", hwDeviceIds[1]->as<HwDeviceIdDrm>()->getPciPath());
}

TEST_F(DrmSimpleTests, GivenSelectedExistingDeviceWhenOpenDirFailsThenRetryOpeningRenderDevices) {
    VariableBackup<decltype(openFull)> backupOpenFull(&openFull);
    VariableBackup<decltype(failOnOpenDir)> backupOpenDir(&failOnOpenDir, true);
    VariableBackup<decltype(readLinkCalledTimes)> backupReadlink(&readLinkCalledTimes, 0);
    openFull = openWithCounter;
    openCounter = 1;

    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_STREQ("/dev/dri/renderD128", lastOpenedPath.c_str());
    EXPECT_EQ(1u, hwDeviceIds.size());
    EXPECT_NE(nullptr, hwDeviceIds[0].get());
    EXPECT_STREQ("0000:00:02.0", hwDeviceIds[0]->as<HwDeviceIdDrm>()->getPciPath());

    openCounter = 2;
    hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_STREQ("/dev/dri/renderD129", lastOpenedPath.c_str());
    EXPECT_EQ(2u, hwDeviceIds.size());
    EXPECT_NE(nullptr, hwDeviceIds[0].get());
    EXPECT_STREQ("0000:00:02.0", hwDeviceIds[0]->as<HwDeviceIdDrm>()->getPciPath());
    EXPECT_NE(nullptr, hwDeviceIds[1].get());
    EXPECT_STREQ("0000:00:03.0", hwDeviceIds[1]->as<HwDeviceIdDrm>()->getPciPath());
}

TEST_F(DrmSimpleTests, givenPrintIoctlEntriesWhenCallIoctlThenIoctlIsPrinted) {
    ::testing::internal::CaptureStdout();

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = DrmWrap::createDrm(*executionEnvironment->rootDeviceEnvironments[0]);

    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintIoctlEntries.set(true);

    uint32_t contextId = 1u;
    drm->destroyDrmContext(contextId);

    std::string output = ::testing::internal::GetCapturedStdout();
    EXPECT_STREQ(output.c_str(), "IOCTL DRM_IOCTL_I915_GEM_CONTEXT_DESTROY called\nIOCTL DRM_IOCTL_I915_GEM_CONTEXT_DESTROY returns 0\n");
}

struct DrmFailedIoctlTests : public ::testing::Test {
    void SetUp() override {
        if (deviceDescriptorTable[0].deviceId == 0) {
            GTEST_SKIP();
        }
    }
};

TEST_F(DrmFailedIoctlTests, givenPrintIoctlEntriesWhenCallFailedIoctlThenExpectedIoctlIsPrinted) {
    ::testing::internal::CaptureStdout();

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = DrmWrap::createDrm(*executionEnvironment->rootDeviceEnvironments[0]);

    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintIoctlEntries.set(true);

    uint32_t contextId = 1u;
    uint32_t vmId = 100u;
    drm->queryVmId(contextId, vmId);

    std::string output = ::testing::internal::GetCapturedStdout();
    EXPECT_STREQ(output.c_str(), "IOCTL DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM called\nIOCTL DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM returns -1, errno 9(Bad file descriptor)\n");
}

TEST_F(DrmSimpleTests, givenPrintIoctlTimesWhenCallIoctlThenStatisticsAreGathered) {
    constexpr long long initialMin = std::numeric_limits<long long>::max();
    constexpr long long initialMax = std::numeric_limits<long long>::min();

    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    auto drm = DrmWrap::createDrm(*executionEnvironment->rootDeviceEnvironments[0]);

    DebugManagerStateRestore restorer;
    DebugManager.flags.PrintIoctlTimes.set(true);
    VariableBackup<decltype(forceExtraIoctlDuration)> backupForceExtraIoctlDuration(&forceExtraIoctlDuration, true);

    EXPECT_TRUE(drm->ioctlStatistics.empty());

    int euTotal = 0u;
    uint32_t contextId = 1u;

    drm->getEuTotal(euTotal);
    EXPECT_EQ(1u, drm->ioctlStatistics.size());

    drm->getEuTotal(euTotal);
    EXPECT_EQ(1u, drm->ioctlStatistics.size());

    drm->setLowPriorityContextParam(contextId);
    EXPECT_EQ(2u, drm->ioctlStatistics.size());

    auto euTotalData = drm->ioctlStatistics.find(DrmIoctl::Getparam);
    ASSERT_TRUE(euTotalData != drm->ioctlStatistics.end());
    EXPECT_EQ(DrmIoctl::Getparam, euTotalData->first);
    EXPECT_EQ(2u, euTotalData->second.count);
    EXPECT_NE(0, euTotalData->second.totalTime);
    EXPECT_NE(initialMin, euTotalData->second.minTime);
    EXPECT_LE(euTotalData->second.minTime, euTotalData->second.maxTime);
    EXPECT_LE(initialMax, euTotalData->second.minTime);
    EXPECT_NE(initialMin, euTotalData->second.maxTime);
    EXPECT_NE(initialMax, euTotalData->second.maxTime);
    auto firstTime = euTotalData->second.totalTime;

    auto lowPriorityData = drm->ioctlStatistics.find(DrmIoctl::GemContextSetparam);
    ASSERT_TRUE(lowPriorityData != drm->ioctlStatistics.end());
    EXPECT_EQ(DrmIoctl::GemContextSetparam, lowPriorityData->first);
    EXPECT_EQ(1u, lowPriorityData->second.count);
    EXPECT_NE(0, lowPriorityData->second.totalTime);
    EXPECT_NE(initialMin, lowPriorityData->second.minTime);
    EXPECT_NE(initialMax, lowPriorityData->second.minTime);
    EXPECT_NE(initialMin, lowPriorityData->second.maxTime);
    EXPECT_NE(initialMax, lowPriorityData->second.maxTime);

    drm->getEuTotal(euTotal);
    EXPECT_EQ(drm->ioctlStatistics.size(), 2u);

    euTotalData = drm->ioctlStatistics.find(DrmIoctl::Getparam);
    ASSERT_TRUE(euTotalData != drm->ioctlStatistics.end());
    EXPECT_EQ(DrmIoctl::Getparam, euTotalData->first);
    EXPECT_EQ(3u, euTotalData->second.count);
    EXPECT_NE(0u, euTotalData->second.totalTime);

    auto secondTime = euTotalData->second.totalTime;
    EXPECT_GT(secondTime, firstTime);

    lowPriorityData = drm->ioctlStatistics.find(DrmIoctl::GemContextSetparam);
    ASSERT_TRUE(lowPriorityData != drm->ioctlStatistics.end());
    EXPECT_EQ(DrmIoctl::GemContextSetparam, lowPriorityData->first);
    EXPECT_EQ(1u, lowPriorityData->second.count);
    EXPECT_NE(0, lowPriorityData->second.totalTime);

    drm->destroyDrmContext(contextId);
    EXPECT_EQ(3u, drm->ioctlStatistics.size());

    euTotalData = drm->ioctlStatistics.find(DrmIoctl::Getparam);
    ASSERT_TRUE(euTotalData != drm->ioctlStatistics.end());
    EXPECT_EQ(DrmIoctl::Getparam, euTotalData->first);
    EXPECT_EQ(3u, euTotalData->second.count);
    EXPECT_NE(0, euTotalData->second.totalTime);

    lowPriorityData = drm->ioctlStatistics.find(DrmIoctl::GemContextSetparam);
    ASSERT_TRUE(lowPriorityData != drm->ioctlStatistics.end());
    EXPECT_EQ(DrmIoctl::GemContextSetparam, lowPriorityData->first);
    EXPECT_EQ(1u, lowPriorityData->second.count);
    EXPECT_NE(0, lowPriorityData->second.totalTime);

    auto destroyData = drm->ioctlStatistics.find(DrmIoctl::GemContextDestroy);
    ASSERT_TRUE(destroyData != drm->ioctlStatistics.end());
    EXPECT_EQ(DrmIoctl::GemContextDestroy, destroyData->first);
    EXPECT_EQ(1u, destroyData->second.count);
    EXPECT_NE(0, destroyData->second.totalTime);

    ::testing::internal::CaptureStdout();

    drm.reset();

    std::string output = ::testing::internal::GetCapturedStdout();
    EXPECT_STRNE("", output.c_str());

    std::string_view requestString("Request");
    std::string_view totalTimeString("Total time(ns)");
    std::string_view countString("Count");
    std::string_view avgTimeString("Avg time per ioctl");
    std::string_view minString("Min");
    std::string_view maxString("Max");

    std::size_t position = output.find(requestString);
    EXPECT_NE(std::string::npos, position);
    position += requestString.size();

    position = output.find(totalTimeString, position);
    EXPECT_NE(std::string::npos, position);
    position += totalTimeString.size();

    position = output.find(countString, position);
    EXPECT_NE(std::string::npos, position);
    position += countString.size();

    position = output.find(avgTimeString, position);
    EXPECT_NE(std::string::npos, position);
    position += avgTimeString.size();

    position = output.find(minString, position);
    EXPECT_NE(std::string::npos, position);
    position += minString.size();

    position = output.find(maxString, position);
    EXPECT_NE(std::string::npos, position);
}

TEST_F(DrmSimpleTests, GivenSelectedNonExistingDeviceWhenOpenDirFailsThenRetryOpeningRenderDevicesAndNoDevicesAreCreated) {
    VariableBackup<decltype(openFull)> backupOpenFull(&openFull);
    VariableBackup<decltype(failOnOpenDir)> backupOpenDir(&failOnOpenDir, true);
    openFull = openWithCounter;
    openCounter = 0;

    ExecutionEnvironment executionEnvironment;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_EQ(0u, hwDeviceIds.size());
}

TEST_F(DrmSimpleTests, GivenFailingOpenDirAndMultipleAvailableDevicesWhenCreateMultipleRootDevicesFlagIsSetThenTheFlagIsRespected) {
    DebugManagerStateRestore stateRestore;
    VariableBackup<decltype(openFull)> backupOpenFull(&openFull);
    VariableBackup<decltype(failOnOpenDir)> backupOpenDir(&failOnOpenDir, true);
    VariableBackup<decltype(readLinkCalledTimes)> backupReadlink(&readLinkCalledTimes, 0);
    openFull = openWithCounter;
    ExecutionEnvironment executionEnvironment;
    const uint32_t requestedNumRootDevices = 2u;
    DebugManager.flags.CreateMultipleRootDevices.set(requestedNumRootDevices);

    openCounter = 4;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_STREQ("/dev/dri/renderD129", lastOpenedPath.c_str());
    EXPECT_EQ(requestedNumRootDevices, hwDeviceIds.size());
    EXPECT_NE(nullptr, hwDeviceIds[0].get());
    EXPECT_STREQ("0000:00:02.0", hwDeviceIds[0]->as<HwDeviceIdDrm>()->getPciPath());
    EXPECT_NE(nullptr, hwDeviceIds[1].get());
    EXPECT_STREQ("0000:00:03.0", hwDeviceIds[1]->as<HwDeviceIdDrm>()->getPciPath());
}

TEST_F(DrmSimpleTests, GivenMultipleAvailableDevicesWhenCreateMultipleRootDevicesFlagIsSetThenTheFlagIsRespected) {
    DebugManagerStateRestore stateRestore;
    VariableBackup<decltype(openFull)> backupOpenFull(&openFull);
    openFull = openWithCounter;
    ExecutionEnvironment executionEnvironment;
    const uint32_t requestedNumRootDevices = 2u;
    DebugManager.flags.CreateMultipleRootDevices.set(requestedNumRootDevices);

    openCounter = 4;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_STREQ("/dev/dri/by-path/platform-4010000000.pcie-pci-0000:00:02.0-render", lastOpenedPath.c_str());
    EXPECT_EQ(requestedNumRootDevices, hwDeviceIds.size());
    EXPECT_NE(nullptr, hwDeviceIds[0].get());
    EXPECT_STREQ("0000:00:03.1", hwDeviceIds[0]->as<HwDeviceIdDrm>()->getPciPath());
    EXPECT_NE(nullptr, hwDeviceIds[1].get());
    EXPECT_STREQ("0000:00:02.0", hwDeviceIds[1]->as<HwDeviceIdDrm>()->getPciPath());
}

TEST_F(DrmTests, GivenSelectedIncorectDeviceByDeviceIdWhenGetDeviceFdThenFail) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.FilterDeviceId.set("invalid");
    auto drm1 = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_EQ(drm1, nullptr);
}

TEST_F(DrmTests, GivenSelectedCorrectDeviceByDeviceIdWhenGetDeviceFdThenSucceed) {
    DebugManagerStateRestore stateRestore;
    std::stringstream deviceIdStr;
    deviceIdStr << std::hex << deviceId;

    DebugManager.flags.FilterDeviceId.set(deviceIdStr.str());
    auto drm1 = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_NE(drm1, nullptr);
}

TEST_F(DrmSimpleTests, givenUseVmBindFlagWhenOverrideBindSupportThenReturnProperValue) {
    DebugManagerStateRestore dbgRestorer;
    bool useVmBind = false;

    DebugManager.flags.UseVmBind.set(1);
    Drm::overrideBindSupport(useVmBind);
    EXPECT_TRUE(useVmBind);

    DebugManager.flags.UseVmBind.set(0);
    Drm::overrideBindSupport(useVmBind);
    EXPECT_FALSE(useVmBind);

    DebugManager.flags.UseVmBind.set(-1);
    Drm::overrideBindSupport(useVmBind);
    EXPECT_FALSE(useVmBind);
}

TEST_F(DrmTests, GivenErrorCodeWhenCreatingDrmThenDrmCreatedOnlyWithSpecificErrors) {
    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_NE(drm, nullptr);

    GetParam getParam{};
    int lDeviceId;

    VariableBackup<decltype(ioctlCnt)> backupIoctlCnt(&ioctlCnt);
    VariableBackup<int> backupIoctlSeq(&ioctlSeq[0]);

    ioctlCnt = 0;
    ioctlSeq[0] = -1;
    errno = EINTR;
    // check if device works, although there was EINTR error from KMD
    getParam.param = I915_PARAM_CHIPSET_ID;
    getParam.value = &lDeviceId;
    auto ret = drm->ioctl(DrmIoctl::Getparam, &getParam);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(deviceId, lDeviceId);

    ioctlCnt = 0;
    ioctlSeq[0] = -1;
    errno = EAGAIN;
    // check if device works, although there was EAGAIN error from KMD
    getParam.param = I915_PARAM_CHIPSET_ID;
    getParam.value = &lDeviceId;
    ret = drm->ioctl(DrmIoctl::Getparam, &getParam);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(deviceId, lDeviceId);

    ioctlCnt = 0;
    ioctlSeq[0] = -1;
    errno = EBUSY;
    // check if device works, although there was EBUSY error from KMD
    getParam.param = I915_PARAM_CHIPSET_ID;
    getParam.value = &lDeviceId;
    ret = drm->ioctl(DrmIoctl::Getparam, &getParam);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(deviceId, lDeviceId);

    ioctlCnt = 0;
    ioctlSeq[0] = -1;
    errno = 0;
    // we failed with any other error code
    getParam.param = I915_PARAM_CHIPSET_ID;
    getParam.value = &lDeviceId;
    ret = drm->ioctl(DrmIoctl::Getparam, &getParam);
    EXPECT_EQ(-1, ret);
    EXPECT_EQ(deviceId, lDeviceId);
}

TEST_F(DrmTests, WhenCreatingTwiceThenDifferentDrmReturned) {
    auto drm1 = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_NE(drm1, nullptr);
    auto drm2 = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_NE(drm2, nullptr);
    EXPECT_NE(drm1, drm2);
}

TEST_F(DrmTests, WhenDriDeviceFoundThenDrmCreatedOnFallback) {
    VariableBackup<decltype(haveDri)> backupHaveDri(&haveDri);

    haveDri = 1;
    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_NE(drm, nullptr);
}

TEST_F(DrmTests, GivenNoDeviceWhenCreatingDrmThenNullIsReturned) {
    VariableBackup<decltype(haveDri)> backupHaveDri(&haveDri);
    haveDri = -1;
    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, GivenUnknownDeviceWhenCreatingDrmThenNullIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.PrintDebugMessages.set(true);

    VariableBackup<decltype(deviceId)> backupDeviceId(&deviceId);
    VariableBackup<decltype(revisionId)> backupRevisionId(&revisionId);

    deviceId = -1;
    revisionId = -1;

    ::testing::internal::CaptureStderr();
    ::testing::internal::CaptureStdout();
    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_EQ(drm, nullptr);
    std::string errStr = ::testing::internal::GetCapturedStderr();
    EXPECT_TRUE(hasSubstr(errStr, std::string("FATAL: Unknown device: deviceId: ffff, revisionId: ffff")));
    ::testing::internal::GetCapturedStdout();
}

TEST_F(DrmTests, GivenKnownDeviceWhenCreatingDrmThenHwInfoIsProperlySet) {
    VariableBackup<decltype(revisionId)> backupRevisionId(&revisionId);

    revisionId = 123;

    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_NE(drm, nullptr);

    EXPECT_EQ(revisionId, rootDeviceEnvironment->getHardwareInfo()->platform.usRevId);
    EXPECT_EQ(deviceId, rootDeviceEnvironment->getHardwareInfo()->platform.usDeviceID);
}

TEST_F(DrmTests, GivenNoSoftPinWhenCreatingDrmThenNullIsReturned) {
    VariableBackup<decltype(haveSoftPin)> backupHaveSoftPin(&haveSoftPin);
    haveSoftPin = 0;

    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, WhenCantFindDeviceIdThenDrmIsNotCreated) {
    VariableBackup<decltype(failOnDeviceId)> backupFailOnDeviceId(&failOnDeviceId);
    failOnDeviceId = -1;

    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, WhenCantQueryEuCountThenDrmIsNotCreated) {
    VariableBackup<decltype(failOnEuTotal)> backupfailOnEuTotal(&failOnEuTotal);
    failOnEuTotal = -1;

    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, WhenCantQuerySubsliceCountThenDrmIsNotCreated) {
    VariableBackup<decltype(failOnSubsliceTotal)> backupfailOnSubsliceTotal(&failOnSubsliceTotal);
    failOnSubsliceTotal = -1;

    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, WhenCantQueryRevisionIdThenDrmIsNotCreated) {
    VariableBackup<decltype(failOnRevisionId)> backupFailOnRevisionId(&failOnRevisionId);
    failOnRevisionId = -1;

    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, WhenCantQuerySoftPinSupportThenDrmIsNotCreated) {
    VariableBackup<decltype(failOnSoftPin)> backupFailOnSoftPin(&failOnSoftPin);
    failOnSoftPin = -1;

    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_EQ(drm, nullptr);
}

TEST_F(DrmTests, GivenFailOnParamBoostWhenCreatingDrmThenDrmIsCreated) {
    VariableBackup<decltype(failOnParamBoost)> backupFailOnParamBoost(&failOnParamBoost);
    failOnParamBoost = -1;

    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    //non-fatal error - issue warning only
    EXPECT_NE(drm, nullptr);
}

TEST_F(DrmTests, GivenFailOnContextCreateWhenCreatingDrmThenDrmIsCreated) {
    VariableBackup<decltype(failOnContextCreate)> backupFailOnContextCreate(&failOnContextCreate);

    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_NE(drm, nullptr);
    failOnContextCreate = -1;
    EXPECT_THROW(drm->createDrmContext(1, false, false), std::exception);
    EXPECT_FALSE(drm->isPreemptionSupported());
    failOnContextCreate = 0;
}

TEST_F(DrmTests, GivenFailOnSetPriorityWhenCreatingDrmThenDrmIsCreated) {
    VariableBackup<decltype(failOnSetPriority)> backupFailOnSetPriority(&failOnSetPriority);

    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_NE(drm, nullptr);
    failOnSetPriority = -1;
    auto drmContext = drm->createDrmContext(1, false, false);
    EXPECT_THROW(drm->setLowPriorityContextParam(drmContext), std::exception);
    EXPECT_FALSE(drm->isPreemptionSupported());
    failOnSetPriority = 0;
}

TEST_F(DrmTests, WhenCantQueryDrmVersionThenDrmIsNotCreated) {
    VariableBackup<decltype(failOnDrmVersion)> backupFailOnDrmVersion(&failOnDrmVersion);

    failOnDrmVersion = -1;
    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_EQ(drm, nullptr);
    failOnDrmVersion = 0;
}

TEST_F(DrmTests, GivenInvalidDrmVersionNameWhenCreatingDrmThenNullIsReturned) {
    VariableBackup<decltype(failOnDrmVersion)> backupFailOnDrmVersion(&failOnDrmVersion);

    strcpy(providedDrmVersion, "NA");
    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_EQ(drm, nullptr);
    failOnDrmVersion = 0;
    strcpy(providedDrmVersion, "i915");
}

TEST_F(DrmTests, whenDrmIsCreatedThenSetMemoryRegionsDoesntFailAndDrmObjectIsReturned) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnableLocalMemory.set(1);

    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_NE(drm, nullptr);
}

TEST(AllocatorHelper, givenExpectedSizeToReserveWhenGetSizeToReserveCalledThenExpectedValueReturned) {
    EXPECT_EQ((maxNBitValue(47) + 1) / 4, NEO::getSizeToReserve());
}

TEST(DrmMemoryManagerCreate, whenCallCreateMemoryManagerThenDrmMemoryManagerIsCreated) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverridePatIndex.set(0);

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto drm = new DrmMockSuccess(fakeFd, *executionEnvironment.rootDeviceEnvironments[0]);

    drm->setupIoctlHelper(defaultHwInfo->platform.eProductFamily);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto drmMemoryManager = MemoryManager::createMemoryManager(executionEnvironment);
    EXPECT_NE(nullptr, drmMemoryManager.get());
    executionEnvironment.memoryManager = std::move(drmMemoryManager);
}

TEST(DrmMemoryManagerCreate, givenEnableHostPtrValidationSetToZeroWhenCreateDrmMemoryManagerThenHostPtrValidationIsDisabled) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableHostPtrValidation.set(0);
    DebugManager.flags.EnableGemCloseWorker.set(0);
    DebugManager.flags.OverridePatIndex.set(0);

    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.forceOsAgnosticMemoryManager = false;

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    auto drm = new DrmMockSuccess(fakeFd, *executionEnvironment.rootDeviceEnvironments[0]);

    drm->setupIoctlHelper(defaultHwInfo->platform.eProductFamily);
    executionEnvironment.rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    executionEnvironment.rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto drmMemoryManager = MemoryManager::createMemoryManager(executionEnvironment);
    EXPECT_NE(nullptr, drmMemoryManager.get());
    EXPECT_FALSE(static_cast<DrmMemoryManager *>(drmMemoryManager.get())->isValidateHostMemoryEnabled());
    executionEnvironment.memoryManager = std::move(drmMemoryManager);
}

TEST(OsInterfaceTests, givenOsInterfaceWhenEnableLocalMemoryIsSpecifiedThenItIsSetToTrueOn64Bit) {
    EXPECT_TRUE(OSInterface::osEnableLocalMemory);
}

TEST_F(DrmTests, whenDrmIsCreatedWithMultipleSubDevicesThenCreateMultipleVirtualMemoryAddressSpaces) {
    DebugManagerStateRestore restore;
    DebugManager.flags.CreateMultipleSubDevices.set(2);

    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_NE(drm, nullptr);

    if (drm->isPerContextVMRequired()) {
        GTEST_SKIP();
    }

    auto numSubDevices = HwHelper::getSubDevicesCount(rootDeviceEnvironment->getHardwareInfo());
    for (auto id = 0u; id < numSubDevices; id++) {
        EXPECT_EQ(id + 1, drm->getVirtualMemoryAddressSpace(id));
    }
}

TEST_F(DrmTests, givenDebuggingEnabledWhenDrmIsCreatedThenPerContextVMIsTrueGetVirtualMemoryAddressSpaceReturnsZeroAndVMsAreNotCreated) {
    DebugManagerStateRestore restore;
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    DebugManager.flags.UseVmBind.set(1);

    rootDeviceEnvironment->executionEnvironment.setDebuggingEnabled();

    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    ASSERT_NE(drm, nullptr);
    if (drm->isVmBindAvailable()) {
        EXPECT_TRUE(drm->isPerContextVMRequired());

        auto numSubDevices = HwHelper::getSubDevicesCount(rootDeviceEnvironment->getHardwareInfo());
        for (auto id = 0u; id < numSubDevices; id++) {
            EXPECT_EQ(0u, drm->getVirtualMemoryAddressSpace(id));
        }
        EXPECT_EQ(0u, static_cast<DrmWrap *>(drm.get())->virtualMemoryIds.size());
    }
}

TEST_F(DrmTests, givenEnabledDebuggingAndVmBindNotAvailableWhenDrmIsCreatedThenPerContextVMIsFalseVMsAreCreatedAndDebugMessageIsPrinted) {
    DebugManagerStateRestore restore;

    ::testing::internal::CaptureStderr();
    ::testing::internal::CaptureStdout();

    DebugManager.flags.CreateMultipleSubDevices.set(2);
    DebugManager.flags.UseVmBind.set(0);
    DebugManager.flags.PrintDebugMessages.set(true);

    rootDeviceEnvironment->executionEnvironment.setDebuggingEnabled();

    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_NE(drm, nullptr);

    if (drm->isPerContextVMRequired()) {
        ::testing::internal::GetCapturedStdout();
        ::testing::internal::GetCapturedStderr();
        GTEST_SKIP();
    }

    auto numSubDevices = HwHelper::getSubDevicesCount(rootDeviceEnvironment->getHardwareInfo());
    for (auto id = 0u; id < numSubDevices; id++) {
        EXPECT_NE(0u, drm->getVirtualMemoryAddressSpace(id));
    }
    EXPECT_NE(0u, static_cast<DrmWrap *>(drm.get())->virtualMemoryIds.size());

    DebugManager.flags.PrintDebugMessages.set(false);
    ::testing::internal::GetCapturedStdout();
    std::string errStr = ::testing::internal::GetCapturedStderr();

    EXPECT_TRUE(hasSubstr(errStr, std::string("WARNING: Debugging not supported\n")));
}

TEST_F(DrmTests, givenDrmIsCreatedWhenCreateVirtualMemoryFailsThenReturnVirtualMemoryIdZeroAndPrintDebugMessage) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.PrintDebugMessages.set(true);

    VariableBackup<decltype(failOnVirtualMemoryCreate)> backupFailOnVirtualMemoryCreate(&failOnVirtualMemoryCreate);

    failOnVirtualMemoryCreate = -1;

    ::testing::internal::CaptureStderr();
    ::testing::internal::CaptureStdout();
    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_NE(drm, nullptr);

    EXPECT_EQ(0u, drm->getVirtualMemoryAddressSpace(0));
    EXPECT_EQ(0u, static_cast<DrmWrap *>(drm.get())->virtualMemoryIds.size());

    std::string errStr = ::testing::internal::GetCapturedStderr();
    if (!drm->isPerContextVMRequired()) {
        EXPECT_TRUE(hasSubstr(errStr, std::string("INFO: Device doesn't support GEM Virtual Memory")));
    }
    ::testing::internal::GetCapturedStdout();
}

TEST(SysCalls, WhenSysCallsPollCalledThenCallIsRedirectedToOs) {
    struct pollfd pollFd;
    pollFd.fd = 0;
    pollFd.events = 0;

    auto result = NEO::SysCalls::poll(&pollFd, 1, 0);
    EXPECT_LE(0, result);
}

TEST(SysCalls, WhenSysCallsFstatCalledThenCallIsRedirectedToOs) {
    struct stat st = {};
    auto result = NEO::SysCalls::fstat(0, &st);
    EXPECT_EQ(0, result);
}

int main(int argc, char **argv) {
    bool useDefaultListener = false;
    bool enableAlarm = true;

    ::testing::InitGoogleTest(&argc, argv);

    // parse remaining args assuming they're mine
    for (int i = 1; i < argc; ++i) {
        if (!strcmp("--disable_default_listener", argv[i])) {
            useDefaultListener = false;
        } else if (!strcmp("--enable_default_listener", argv[i])) {
            useDefaultListener = true;
        } else if (!strcmp("--disable_alarm", argv[i])) {
            enableAlarm = false;
        }
    }

    if (useDefaultListener == false) {
        auto &listeners = ::testing::UnitTest::GetInstance()->listeners();
        auto defaultListener = listeners.default_result_printer();
        auto customEventListener = new CCustomEventListener(defaultListener);

        listeners.Release(defaultListener);
        listeners.Append(customEventListener);
    }

    defaultHwInfo = std::make_unique<HardwareInfo>();
    *defaultHwInfo = DEFAULT_TEST_PLATFORM::hwInfo;

    initializeTestedDevice();

    Os::dxcoreDllName = "";

    int sigOut = setAlarm(enableAlarm);
    if (sigOut != 0)
        return sigOut;

    auto retVal = RUN_ALL_TESTS();

    return retVal;
}

TEST_F(DrmTests, whenCreateDrmIsCalledThenProperHwInfoIsSetup) {
    auto oldHwInfo = rootDeviceEnvironment->getMutableHardwareInfo();
    *oldHwInfo = {};
    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    EXPECT_NE(drm, nullptr);
    auto currentHwInfo = rootDeviceEnvironment->getHardwareInfo();
    EXPECT_NE(IGFX_UNKNOWN, currentHwInfo->platform.eProductFamily);
    EXPECT_NE(IGFX_UNKNOWN_CORE, currentHwInfo->platform.eRenderCoreFamily);
    EXPECT_LT(0u, currentHwInfo->gtSystemInfo.EUCount);
    EXPECT_LT(0u, currentHwInfo->gtSystemInfo.SubSliceCount);
}

TEST(DirectSubmissionControllerTest, whenCheckDirectSubmissionControllerSupportThenReturnsTrue) {
    EXPECT_TRUE(DirectSubmissionController::isSupported());
}

TEST(CommandQueueTest, whenCheckEngineRoundRobinAssignThenReturnsTrue) {
    EXPECT_TRUE(CommandQueue::isAssignEngineRoundRobinEnabled());
}

TEST(CommandQueueTest, whenCheckEngineTimestampWaitEnabledThenReturnsTrue) {
    EXPECT_TRUE(CommandQueue::isTimestampWaitEnabled());
}

TEST(DeviceTest, whenCheckBlitSplitEnabledThenReturnsTrue) {
    EXPECT_TRUE(Device::isBlitSplitEnabled());
}

TEST(PlatformsDestructor, whenGlobalPlatformsDestructorIsCalledThenGlobalPlatformsAreDestroyed) {
    EXPECT_NE(nullptr, platformsImpl);
    platformsDestructor();

    EXPECT_EQ(nullptr, platformsImpl);
    platformsImpl = new std::vector<std::unique_ptr<Platform>>;
}

TEST_F(DrmTests, givenValidPciPathThenPciBusInfoIsAvailable) {
    VariableBackup<decltype(openFull)> backupOpenFull(&openFull);
    VariableBackup<decltype(failOnOpenDir)> backupOpenDir(&failOnOpenDir, false);
    VariableBackup<decltype(entryIndex)> backupEntryIndex(&entryIndex, 0u);

    openFull = openWithCounter;
    entryIndex = 1;
    openCounter = 2;

    auto drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    ASSERT_NE(drm, nullptr);
    EXPECT_EQ(drm->getPciBusInfo().pciDomain, 0u);
    EXPECT_EQ(drm->getPciBusInfo().pciBus, 0u);
    EXPECT_EQ(drm->getPciBusInfo().pciDevice, 3u);
    EXPECT_EQ(drm->getPciBusInfo().pciFunction, 1u);

    entryIndex = 2;
    openCounter = 1;

    drm = DrmWrap::createDrm(*rootDeviceEnvironment);
    ASSERT_NE(drm, nullptr);
    EXPECT_EQ(drm->getPciBusInfo().pciDomain, 0u);
    EXPECT_EQ(drm->getPciBusInfo().pciBus, 0u);
    EXPECT_EQ(drm->getPciBusInfo().pciDevice, 2u);
    EXPECT_EQ(drm->getPciBusInfo().pciFunction, 0u);

    uint32_t referenceData[4][4] = {
        {0x0a00, 0x00, 0x03, 0x1},
        {0x0000, 0xb3, 0x03, 0x1},
        {0x0000, 0x00, 0xb3, 0x1},
        {0x0000, 0x00, 0x03, 0xa}};
    for (uint32_t idx = 7; idx < 11; idx++) {
        entryIndex = idx;
        openCounter = 1;
        drm = DrmWrap::createDrm(*rootDeviceEnvironment);
        ASSERT_NE(drm, nullptr);

        EXPECT_EQ(drm->getPciBusInfo().pciDomain, referenceData[idx - 7][0]);
        EXPECT_EQ(drm->getPciBusInfo().pciBus, referenceData[idx - 7][1]);
        EXPECT_EQ(drm->getPciBusInfo().pciDevice, referenceData[idx - 7][2]);
        EXPECT_EQ(drm->getPciBusInfo().pciFunction, referenceData[idx - 7][3]);
    }
}
TEST_F(DrmTests, givenInValidPciPathThenNothingIsReturned) {
    VariableBackup<decltype(openFull)> backupOpenFull(&openFull);
    VariableBackup<decltype(failOnOpenDir)> backupOpenDir(&failOnOpenDir, false);
    VariableBackup<decltype(entryIndex)> backupEntryIndex(&entryIndex, 0u);

    openFull = openWithCounter;

    entryIndex = 11;
    openCounter = 1;
    auto hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_TRUE(hwDeviceIds.empty());

    entryIndex = 12;
    openCounter = 1;
    hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_TRUE(hwDeviceIds.empty());

    entryIndex = 13;
    openCounter = 1;
    hwDeviceIds = OSInterface::discoverDevices(executionEnvironment);
    EXPECT_TRUE(hwDeviceIds.empty());
}
