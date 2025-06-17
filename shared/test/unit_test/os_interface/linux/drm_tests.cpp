/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/submission_status.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/gpu_page_fault_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/directory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/mocks/linux/mock_os_context_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <fcntl.h>
#include <memory>

using namespace NEO;

namespace NEO {
extern bool returnEmptyFilesVector;
}

std::string getLinuxDevicesPath(const char *file) {
    std::string resultString(Os::sysFsPciPathPrefix);
    resultString += file;

    return resultString;
}

TEST(DrmTest, whenGettingSubmissionStatusFromReturnCodeThenProperValueIsReturned) {
    EXPECT_EQ(SubmissionStatus::success, Drm::getSubmissionStatusFromReturnCode(0));
    EXPECT_EQ(SubmissionStatus::outOfHostMemory, Drm::getSubmissionStatusFromReturnCode(EWOULDBLOCK));
    EXPECT_EQ(SubmissionStatus::outOfHostMemory, Drm::getSubmissionStatusFromReturnCode(ENOSPC));
    EXPECT_EQ(SubmissionStatus::outOfHostMemory, Drm::getSubmissionStatusFromReturnCode(ENOMEM));
    EXPECT_EQ(SubmissionStatus::outOfMemory, Drm::getSubmissionStatusFromReturnCode(ENXIO));
    EXPECT_EQ(SubmissionStatus::failed, Drm::getSubmissionStatusFromReturnCode(EBUSY));
}

TEST(DrmTest, GivenValidPciPathWhenGettingAdapterBdfThenCorrectValuesAreReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    {
        drm.setPciPath("0000:ab:cd.e");
        EXPECT_EQ(0, drm.queryAdapterBDF());
        auto adapterBdf = drm.adapterBDF;
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
        auto adapterBdf = drm.adapterBDF;
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
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.setPciPath("invalidPci");
    EXPECT_EQ(1, drm.queryAdapterBDF());
    auto adapterBdf = drm.adapterBDF;
    EXPECT_EQ(std::numeric_limits<uint32_t>::max(), adapterBdf.Data);

    auto pciInfo = drm.getPciBusInfo();
    EXPECT_EQ(PhysicalDevicePciBusInfo::invalidValue, pciInfo.pciDomain);
    EXPECT_EQ(PhysicalDevicePciBusInfo::invalidValue, pciInfo.pciBus);
    EXPECT_EQ(PhysicalDevicePciBusInfo::invalidValue, pciInfo.pciDevice);
    EXPECT_EQ(PhysicalDevicePciBusInfo::invalidValue, pciInfo.pciFunction);
}

TEST(DrmTest, GivenInvalidPciPathWhenFrequencyIsQueriedThenReturnError) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto hwInfo = *defaultHwInfo;

    int maxFrequency = 0;

    drm.setPciPath("invalidPci");
    int ret = drm.getMaxGpuFrequency(hwInfo, maxFrequency);
    EXPECT_NE(0, ret);

    EXPECT_EQ(0, maxFrequency);
}

TEST(DrmTest, GivenValidSysfsNodeWhenGetDeviceMemoryMaxClockRateInMhzIsCalledThenReturnSuccess) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.setPciPath("device");
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        const std::string testData("800");
        memcpy(buf, testData.data(), testData.length() + 1);
        return 4;
    });
    uint32_t clkRate = 0;
    EXPECT_TRUE(drm.getDeviceMemoryMaxClockRateInMhz(0, clkRate));
    EXPECT_EQ(clkRate, 800u);
}

struct MockIoctlHelperForSmallBar : public IoctlHelperUpstream {
    using IoctlHelperUpstream::IoctlHelperUpstream;

    std::unique_ptr<MemoryInfo> createMemoryInfo() override {
        auto memoryInfo{new MockMemoryInfo{drm}};
        memoryInfo->smallBarDetected = true;
        return std::unique_ptr<MemoryInfo>{memoryInfo};
    }

    bool isSmallBarConfigAllowed() const override { return smallBarAllowed; }

    bool smallBarAllowed = true;
};

TEST(DrmTest, givenSmallBarDetectedInMemoryInfoAndNotSupportedWhenSetupHardwareInfoCalledThenWarningMessagePrintedAndFailureIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.setPciPath("0000:ab:cd.e");

    auto setupHardwareInfo = [](HardwareInfo *hwInfo, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, defaultHwInfo.get(), setupHardwareInfo};

    auto mockIoctlHelper = std::make_unique<MockIoctlHelperForSmallBar>(drm);
    mockIoctlHelper->smallBarAllowed = false;

    drm.ioctlHelper.reset(mockIoctlHelper.release());

    ::testing::internal::CaptureStderr();
    EXPECT_EQ(-1, drm.setupHardwareInfo(&device, false));
    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_STREQ("WARNING: Small BAR detected for device 0000:ab:cd.e\n", output.c_str());
}

TEST(DrmTest, givenSmallBarDetectedInMemoryInfoAndSupportedWhenSetupHardwareInfoCalledThenWarningMessagePrintedAndInitializationIsNotBroken) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.setPciPath("0000:ab:cd.e");

    auto setupHardwareInfo = [](HardwareInfo *hwInfo, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, defaultHwInfo.get(), setupHardwareInfo};

    auto mockIoctlHelper = std::make_unique<MockIoctlHelperForSmallBar>(drm);
    mockIoctlHelper->smallBarAllowed = true;

    drm.ioctlHelper.reset(mockIoctlHelper.release());

    ::testing::internal::CaptureStderr();
    EXPECT_EQ(0, drm.setupHardwareInfo(&device, false));
    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_STREQ("WARNING: Small BAR detected for device 0000:ab:cd.e\n", output.c_str());
}

TEST(DrmTest, GivenMemoryInfoWithLocalMemoryRegionsWhenGetDeviceMemoryPhysicalSizeInBytesIsCalledThenCorrectSizeReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    auto ioctlHelper{drm.getIoctlHelper()};
    const auto memoryClassSystem = static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::memoryClassSystem));
    const auto memoryClassDevice = static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::memoryClassDevice));
    std::vector<MemoryRegion> memRegions(3);
    memRegions[0] = {{memoryClassSystem, 0}, 1024};
    memRegions[1] = {{memoryClassDevice, 0}, 2048};
    memRegions[2] = {{memoryClassDevice, 1}, 3072};
    drm.memoryInfo.reset(new MemoryInfo{memRegions, drm});

    uint64_t size{0U};
    EXPECT_TRUE(drm.getDeviceMemoryPhysicalSizeInBytes(0, size));
    EXPECT_EQ(2048u, size);
}

TEST(DrmTest, GivenMemoryInfoWithNoLocalMemoryRegionsWhenGetDeviceMemoryPhysicalSizeInBytesIsCalledThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    auto ioctlHelper{drm.getIoctlHelper()};
    const auto memoryClassSystem = static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::memoryClassSystem));
    std::vector<MemoryRegion> memRegions(1);
    memRegions[0] = {{memoryClassSystem, 0}, 2048};
    drm.memoryInfo.reset(new MemoryInfo{memRegions, drm});

    uint64_t size{0U};
    EXPECT_FALSE(drm.getDeviceMemoryPhysicalSizeInBytes(0, size));
    EXPECT_EQ(0U, size);
}

TEST(DrmTest, GivenNoMemoryInfoWhenGetDeviceMemoryPhysicalSizeInBytesIsCalledThenZeroIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    uint64_t size{0U};
    EXPECT_FALSE(drm.getDeviceMemoryPhysicalSizeInBytes(0, size));
    EXPECT_EQ(0U, size);
}

TEST(DrmTest, GivenInValidSysfsNodeWhenGetDeviceMemoryMaxClockRateInMhzIsCalledThenReturnSuccess) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.setPciPath("device");
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return -1;
    });

    uint32_t clkRate = 0;
    EXPECT_FALSE(drm.getDeviceMemoryMaxClockRateInMhz(0, clkRate));
}

TEST(DrmTest, givenSysfsNodeReadFailsWithErrnoWhenGetDeviceMemoryMaxClockRateInMhzIsCalledThenReturnError) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.setPciPath("device");
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        const std::string testData("800");
        memcpy(buf, testData.data(), testData.length() + 1);
        errno = 1;
        return 4;
    });
    uint32_t clkRate = 0;
    EXPECT_FALSE(drm.getDeviceMemoryMaxClockRateInMhz(0, clkRate));
}

TEST(DrmTest, givenSysfsNodeReadFailsWithImproperDataWhenGetDeviceMemoryMaxClockRateInMhzIsCalledThenReturnError) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.setPciPath("device");
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return 1;
    });

    VariableBackup<decltype(SysCalls::sysCallsPread)> mockPread(&SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        const std::string testData("abc");
        memcpy(buf, testData.data(), testData.length() + 1);
        return 4;
    });
    uint32_t clkRate = 0;
    EXPECT_FALSE(drm.getDeviceMemoryMaxClockRateInMhz(0, clkRate));
}

TEST(DrmTest, WhenGettingRevisionIdThenCorrectIdIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto pDrm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_NE(nullptr, pDrm);

    auto hwInfo = pDrm->getRootDeviceEnvironment().getMutableHardwareInfo();

    static constexpr uint16_t mockDeviceId = 0x1234;
    static constexpr uint16_t mockRevisionId = 0xB;

    hwInfo->platform.usDeviceID = 0;
    hwInfo->platform.usRevId = 0;

    VariableBackup<decltype(SysCalls::sysCallsIoctl)> mockIoctl(&SysCalls::sysCallsIoctl);

    SysCalls::sysCallsIoctl = [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        if (request == DRM_IOCTL_I915_GETPARAM) {
            auto getParam = reinterpret_cast<GetParam *>(arg);
            if (getParam->param == I915_PARAM_CHIPSET_ID) {
                *getParam->value = mockDeviceId;
            } else if (getParam->param == I915_PARAM_REVISION) {
                *getParam->value = mockRevisionId;
            } else {
                return -1;
            }
            return 0;
        }
        return 1;
    };

    EXPECT_TRUE(pDrm->queryDeviceIdAndRevision());

    EXPECT_EQ(mockDeviceId, hwInfo->platform.usDeviceID);
    EXPECT_EQ(mockRevisionId, hwInfo->platform.usRevId);
}

TEST(DrmTest, GivenDrmWhenAskedForGttSizeThenReturnCorrectValue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    uint64_t queryGttSize = 0;

    drm->storedRetValForGetGttSize = 0;
    drm->storedGTTSize = 1ull << 31;
    EXPECT_EQ(0, drm->Drm::queryGttSize(queryGttSize, true));
    EXPECT_EQ(drm->storedGTTSize, queryGttSize);

    queryGttSize = 0;
    drm->storedRetValForGetGttSize = 0;
    drm->storedGTTSize = 1ull << 47;
    EXPECT_EQ(0, drm->Drm::queryGttSize(queryGttSize, true));
    EXPECT_EQ(drm->storedGTTSize, queryGttSize);
    queryGttSize = 0;
    drm->storedRetValForGetGttSize = 0;
    drm->storedGTTSize = 1ull << 47;
    EXPECT_EQ(0, drm->Drm::queryGttSize(queryGttSize, false));
    EXPECT_EQ(drm->storedGTTSize, queryGttSize);

    queryGttSize = 0;
    drm->storedRetValForGetGttSize = 0;
    drm->storedGTTSize = (1ull << 48) - 1;
    EXPECT_EQ(0, drm->Drm::queryGttSize(queryGttSize, true));
    EXPECT_EQ(1ull << 48, queryGttSize);
    queryGttSize = 0;
    drm->storedRetValForGetGttSize = 0;
    drm->storedGTTSize = (1ull << 48) - 1;
    EXPECT_EQ(0, drm->Drm::queryGttSize(queryGttSize, false));
    EXPECT_EQ(drm->storedGTTSize, queryGttSize);

    queryGttSize = 0;
    drm->storedRetValForGetGttSize = 0;
    drm->storedGTTSize = (1ull << 47) + 1;
    EXPECT_EQ(0, drm->Drm::queryGttSize(queryGttSize, true));
    EXPECT_EQ(1ull << 48, queryGttSize);
    queryGttSize = 0;
    drm->storedRetValForGetGttSize = 0;
    drm->storedGTTSize = (1ull << 47) + 1;
    EXPECT_EQ(0, drm->Drm::queryGttSize(queryGttSize, false));
    EXPECT_EQ(drm->storedGTTSize, queryGttSize);

    queryGttSize = 0;
    drm->storedRetValForGetGttSize = -1;
    EXPECT_NE(0, drm->Drm::queryGttSize(queryGttSize, true));
    EXPECT_EQ(0u, queryGttSize);
}

TEST(DrmTest, GivenDrmWhenAskedForContextThatFailsThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    pDrm->storedRetVal = -1;
    EXPECT_EQ(-1, pDrm->createDrmContext(1, false, false));
    pDrm->storedRetVal = 0;
    delete pDrm;
}

TEST(DrmTest, GivenDrmWhenAskedForContextThatIsSuccessThenTrueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    pDrm->storedRetVal = 0;
    EXPECT_EQ(0, pDrm->createDrmContext(1, false, false));
    if (pDrm->ioctlHelper->hasContextFreqHint()) {
        EXPECT_EQ(1u, pDrm->lowLatencyHintRequested);
    }
    delete pDrm;
}

TEST(DrmTest, givenDrmWhenOsContextIsCreatedThenCreateAndDestroyNewDrmOsContext) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    {
        OsContextLinux osContext1(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
        osContext1.ensureContextInitialized(false);

        EXPECT_EQ(1u, osContext1.getDrmContextIds().size());
        EXPECT_EQ(drmMock.storedDrmContextId, osContext1.getDrmContextIds()[0]);
        EXPECT_EQ(0u, drmMock.receivedDestroyContextId);

        {
            OsContextLinux osContext2(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
            osContext2.ensureContextInitialized(false);
            EXPECT_EQ(1u, osContext2.getDrmContextIds().size());
            EXPECT_EQ(drmMock.storedDrmContextId, osContext2.getDrmContextIds()[0]);
            EXPECT_EQ(0u, drmMock.receivedDestroyContextId);
        }
    }

    EXPECT_EQ(4u, drmMock.receivedContextParamRequestCount);
}

TEST(DrmTest, whenCreatingDrmContextWithVirtualMemoryAddressSpaceThenProperVmIdIsSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    ASSERT_EQ(1u, drmMock.virtualMemoryIds.size());

    OsContextLinux osContext(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    EXPECT_EQ(2u, drmMock.receivedContextParamRequestCount);

    EXPECT_EQ(drmMock.requestSetVmId, static_cast<uint64_t>(drmMock.getVirtualMemoryAddressSpace(0u)));
}

TEST(DrmTest, whenCreatingDrmContextWithNoVirtualMemoryAddressSpaceThenProperContextIdIsSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.destroyVirtualMemoryAddressSpace();

    ASSERT_EQ(0u, drmMock.virtualMemoryIds.size());

    OsContextLinux osContext(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    EXPECT_EQ(1u, drmMock.receivedContextParamRequestCount); // unrecoverable context
}

TEST(DrmTest, givenDrmAndNegativeCheckNonPersistentContextsSupportWhenOsContextIsCreatedThenReceivedContextParamRequestCountReturnsCorrectValue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    auto expectedCount = 1u; // unrecoverable context

    {
        drmMock.storedRetValForPersistant = -1;
        drmMock.checkNonPersistentContextsSupport();
        expectedCount += 2;
        OsContextLinux osContext(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
        osContext.ensureContextInitialized(false);
        EXPECT_EQ(expectedCount, drmMock.receivedContextParamRequestCount);
    }
    {
        drmMock.storedRetValForPersistant = 0;
        drmMock.checkNonPersistentContextsSupport();
        ++expectedCount;
        OsContextLinux osContext(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
        osContext.ensureContextInitialized(false);
        expectedCount += 3;
        EXPECT_EQ(expectedCount, drmMock.receivedContextParamRequestCount);
    }
}

TEST(DrmTest, WhenEnablingTurboBoostThenSucceeds) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    int ret = pDrm->enableTurboBoost();
    EXPECT_EQ(0, ret);

    delete pDrm;
}

TEST(DrmTest, WhenGettingEnabledPooledEuThenCorrectValueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
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
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
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
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_NE(nullptr, pDrm);

    auto errnoFromDrm = pDrm->getErrno();
    EXPECT_EQ(errno, errnoFromDrm);
    delete pDrm;
}
TEST(DrmTest, givenPlatformWhereGetSseuRetFailureWhenCallSetQueueSliceCountThenSliceCountIsNotSet) {
    uint64_t newSliceCount = 1;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->storedRetValForGetSSEU = -1;
    drm->checkQueueSliceSupport();

    EXPECT_FALSE(drm->sliceCountChangeSupported);
    EXPECT_FALSE(drm->setQueueSliceCount(newSliceCount));
    EXPECT_NE(drm->getSliceMask(newSliceCount), drm->storedParamSseu);
}

TEST(DrmTest, whenCheckNonPeristentSupportIsCalledThenAreNonPersistentContextsSupportedReturnsCorrectValues) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
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
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = std::make_unique<DrmMock>(*executionEnvironment->rootDeviceEnvironments[0]);
    drm->storedRetValForSetSSEU = -1;
    drm->storedRetValForGetSSEU = 0;
    drm->checkQueueSliceSupport();

    EXPECT_TRUE(drm->sliceCountChangeSupported);
    EXPECT_FALSE(drm->setQueueSliceCount(newSliceCount));
}

TEST(DrmTest, givenPlatformWithSupportToChangeSliceCountWhenCallSetQueueSliceCountThenReturnTrue) {
    uint64_t newSliceCount = 1;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
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
extern uint32_t openFuncCalled;
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
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    OsContextLinux osContext(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    drmMock.latestCreatedVmId = 0u;
    auto expectedVmId = drmMock.latestCreatedVmId + 1;
    EXPECT_EQ(expectedVmId, drmMock.getVirtualMemoryAddressSpace(0));

    auto &contextIds = osContext.getDrmContextIds();
    EXPECT_EQ(1u, contextIds.size());
}

TEST(DrmTest, givenDrmWithPerContextVMRequiredWhenCreatingOsContextsThenImplicitVmIdPerContextIsUsed) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootEnv = *executionEnvironment.rootDeviceEnvironments[0];
    rootEnv.executionEnvironment.setDebuggingMode(NEO::DebuggingMode::online);

    DrmMock drmMock(rootEnv);
    EXPECT_TRUE(drmMock.requirePerContextVM);

    OsContextLinux osContext1(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext1.ensureContextInitialized(false);

    OsContextLinux osContext2(drmMock, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext2.ensureContextInitialized(false);
}

TEST(DrmTest, givenPerContextVMRequiredWhenCreatingOsContextsThenExplicitVmIsCreated) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootEnv = *executionEnvironment.rootDeviceEnvironments[0];
    rootEnv.executionEnvironment.setDebuggingMode(NEO::DebuggingMode::online);

    DrmMock drmMock(rootEnv);
    EXPECT_TRUE(drmMock.requirePerContextVM);

    drmMock.storedRetValForVmId = 20;

    OsContextLinux osContext(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);
    EXPECT_EQ(2u, drmMock.receivedContextParamRequestCount);

    auto &drmVmIds = osContext.getDrmVmIds();
    EXPECT_EQ(4u, drmVmIds.size());

    EXPECT_NE(20u, drmVmIds[0]);

    EXPECT_EQ(1, drmMock.ioctlCount.gemVmCreate);
    EXPECT_EQ(0u, drmMock.receivedGemVmControl.vmId);
    EXPECT_EQ(drmMock.latestCreatedVmId, drmVmIds[0]);
    EXPECT_EQ(1, drmMock.createDrmVmCalled);

    EXPECT_EQ(1, drmMock.ioctlCount.contextGetParam);
}

TEST(DrmTest, givenPerContextVMRequiredWhenVmIdCreationFailsThenQueryVmIsCalled) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootEnv = *executionEnvironment.rootDeviceEnvironments[0];

    DrmMock drmMock(rootEnv);
    drmMock.setPerContextVMRequired(true);

    drmMock.storedRetValForVmCreate = -1;

    MockOsContextLinux osContext(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    drmMock.createDrmVmCalled = 0;
    auto vmId = static_cast<uint32_t>(drmMock.storedRetValForVmId);

    auto status = osContext.ensureContextInitialized(false);
    EXPECT_EQ(1, drmMock.createDrmVmCalled);
    EXPECT_EQ(1, drmMock.ioctlCount.contextGetParam);
    EXPECT_EQ(osContext.drmVmIds[0], vmId);

    EXPECT_TRUE(status);
}

TEST(DrmTest, givenPerContextVMRequiredWhenCreatingOsContextForSubDeviceThenVmIdPerContextIsCreateddAndStoredAtSubDeviceIndex) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootEnv = *executionEnvironment.rootDeviceEnvironments[0];
    rootEnv.executionEnvironment.setDebuggingMode(NEO::DebuggingMode::online);

    DrmMock drmMock(rootEnv);
    EXPECT_TRUE(drmMock.requirePerContextVM);

    drmMock.storedRetValForVmId = 4;
    DeviceBitfield deviceBitfield(1 << 3);

    OsContextLinux osContext(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));
    osContext.ensureContextInitialized(false);
    EXPECT_EQ(2u, drmMock.receivedContextParamRequestCount);

    auto &drmVmIds = osContext.getDrmVmIds();
    EXPECT_EQ(4u, drmVmIds.size());

    EXPECT_EQ(drmMock.latestCreatedVmId, drmVmIds[3]);

    EXPECT_EQ(0u, drmVmIds[0]);
    EXPECT_EQ(0u, drmVmIds[2]);
}

TEST(DrmTest, givenPerContextVMRequiredWhenCreatingOsContextsForRootDeviceThenVmIdsPerContextAreCreatedAndStoredAtSubDeviceIndices) {
    MockExecutionEnvironment executionEnvironment{};
    auto &rootEnv = *executionEnvironment.rootDeviceEnvironments[0];
    rootEnv.executionEnvironment.setDebuggingMode(NEO::DebuggingMode::online);

    DrmMock drmMock(rootEnv);
    EXPECT_TRUE(drmMock.requirePerContextVM);

    drmMock.storedRetValForVmId = 4;
    DeviceBitfield deviceBitfield(1 | 1 << 1);

    OsContextLinux osContext(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(deviceBitfield));
    osContext.ensureContextInitialized(false);
    EXPECT_EQ(2 * 2u, drmMock.receivedContextParamRequestCount);

    auto &drmVmIds = osContext.getDrmVmIds();
    EXPECT_EQ(4u, drmVmIds.size());

    EXPECT_EQ(drmMock.latestCreatedVmId - 1, drmVmIds[0]);
    EXPECT_EQ(drmMock.latestCreatedVmId, drmVmIds[1]);

    EXPECT_EQ(0u, drmVmIds[2]);
}

TEST(DrmTest, givenNoPerContextVmsDrmWhenCreatingOsContextsThenVmIdIsNotQueriedAndStored) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_FALSE(drmMock.requirePerContextVM);

    drmMock.storedRetValForVmId = 1;

    OsContextLinux osContext(drmMock, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);
    EXPECT_EQ(2u, drmMock.receivedContextParamRequestCount);

    auto &drmVmIds = osContext.getDrmVmIds();
    EXPECT_EQ(0u, drmVmIds.size());
}

TEST(DrmTest, givenProgramDebuggingAndContextDebugAvailableWhenCreatingContextThenSetContextDebugFlagIsCalled) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    DrmMockNonFailing drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.contextDebugSupported = true;
    drmMock.callBaseCreateDrmContext = false;

    OsContextLinux osContext(drmMock, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    // drmMock returns ctxId == 0
    EXPECT_EQ(0u, drmMock.passedContextDebugId);
}

TEST(DrmTest, givenProgramDebuggingAndContextDebugAvailableWhenCreatingContextForInternalEngineThenSetContextDebugFlagIsNotCalled) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    DrmMockNonFailing drmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock.contextDebugSupported = true;

    OsContextLinux osContext(drmMock, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::internal}));
    osContext.ensureContextInitialized(false);

    EXPECT_EQ(static_cast<uint32_t>(-1), drmMock.passedContextDebugId);
}

TEST(DrmTest, givenNotEnabledDebuggingOrContextDebugUnsupportedWhenCreatingContextThenCooperativeFlagIsNotPassedToCreateDrmContext) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    DrmMockNonFailing drmMock(*executionEnvironment->rootDeviceEnvironments[0]);

    drmMock.contextDebugSupported = true;
    drmMock.callBaseCreateDrmContext = false;
    drmMock.capturedCooperativeContextRequest = true;

    EXPECT_FALSE(executionEnvironment->isDebuggingEnabled());

    OsContextLinux osContext(drmMock, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}));
    osContext.ensureContextInitialized(false);

    EXPECT_FALSE(drmMock.capturedCooperativeContextRequest);

    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    drmMock.contextDebugSupported = false;
    drmMock.callBaseCreateDrmContext = false;
    drmMock.capturedCooperativeContextRequest = true;

    OsContextLinux osContext2(drmMock, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_RCS, EngineUsage::regular}));
    osContext2.ensureContextInitialized(false);

    EXPECT_FALSE(drmMock.capturedCooperativeContextRequest);
}

TEST(DrmTest, givenPrintIoctlDebugFlagSetWhenGettingTimestampFrequencyThenCaptureExpectedOutput) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintIoctlEntries.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    int frequency = 0;

    StreamCapture capture;
    capture.captureStdout(); // start capturing

    int ret = drm.getTimestampFrequency(frequency);
    debugManager.flags.PrintIoctlEntries.set(false);
    std::string outputString = capture.getCapturedStdout(); // stop capturing

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1000, frequency);

    std::string expectedString = "DRM_IOCTL_I915_GETPARAM: param: I915_PARAM_CS_TIMESTAMP_FREQUENCY, output value: 1000, retCode: 0";
    EXPECT_NE(std::string::npos, outputString.find(expectedString));
}

TEST(DrmTest, givenPrintIoctlDebugFlagNotSetWhenGettingTimestampFrequencyThenCaptureExpectedOutput) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintIoctlEntries.set(false);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    int frequency = 0;

    StreamCapture capture;
    capture.captureStdout(); // start capturing

    int ret = drm.getTimestampFrequency(frequency);
    std::string outputString = capture.getCapturedStdout(); // stop capturing

    EXPECT_EQ(0, ret);
    EXPECT_EQ(1000, frequency);

    std::string expectedString = "DRM_IOCTL_I915_GETPARAM: param: I915_PARAM_CS_TIMESTAMP_FREQUENCY, output value: 1000, retCode: 0";
    EXPECT_EQ(std::string::npos, outputString.find(expectedString));
}

TEST(DrmTest, givenPrintIoctlDebugFlagSetWhenGettingOATimestampFrequencyThenCaptureExpectedOutput) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintIoctlEntries.set(true);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    int frequency = 0;

    StreamCapture capture;
    capture.captureStdout(); // start capturing

    int ret = drm.getOaTimestampFrequency(frequency);
    debugManager.flags.PrintIoctlEntries.set(false);
    std::string outputString = capture.getCapturedStdout(); // stop capturing

    EXPECT_EQ(0, ret);
    EXPECT_EQ(123456, frequency);

    std::string expectedString = "DRM_IOCTL_I915_GETPARAM: param: I915_PARAM_OA_TIMESTAMP_FREQUENCY, output value: 123456, retCode: 0";
    EXPECT_NE(std::string::npos, outputString.find(expectedString));
}

TEST(DrmTest, givenProgramDebuggingWhenCreatingContextThenUnrecoverableContextIsSet) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();
    executionEnvironment->calculateMaxOsContextCount();
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    OsContextLinux osContext(drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor());
    osContext.ensureContextInitialized(false);

    EXPECT_TRUE(drm.unrecoverableContextSet);
    EXPECT_EQ(0u, drm.receivedRecoverableContextValue);
    EXPECT_EQ(2u, drm.receivedContextParamRequestCount);
}

TEST(DrmTest, whenImmediateVmBindIsRequiredThenUseVmBindImmediate) {
    for (auto requireImmediateVmBind : ::testing::Bool()) {
        MockExecutionEnvironment executionEnvironment{};
        DrmMock drm(*executionEnvironment.rootDeviceEnvironments[0]);
        auto ioctlHelper = std::make_unique<MockIoctlHelper>(drm);
        ioctlHelper->isImmediateVmBindRequiredResult = requireImmediateVmBind;
        drm.ioctlHelper = std::move(ioctlHelper);
        EXPECT_EQ(requireImmediateVmBind, drm.useVMBindImmediate());
    }
}

TEST(DrmTest, whenPageFaultIsSupportedThenUseVmBindImmediate) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    for (auto hasPageFaultSupport : {false, true}) {
        drm.pageFaultSupported = hasPageFaultSupport;
        EXPECT_EQ(hasPageFaultSupport, drm.useVMBindImmediate());
    }
}

TEST(DrmTest, whenImmediateVmBindExtIsEnabledThenUseVmBindImmediate) {
    DebugManagerStateRestore restorer;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    DrmMock drm(*executionEnvironment->rootDeviceEnvironments[0]);

    for (auto enableImmediateBind : {false, true}) {
        debugManager.flags.EnableImmediateVmBindExt.set(enableImmediateBind);
        EXPECT_EQ(enableImmediateBind, drm.useVMBindImmediate());
    }
}

TEST(DrmQueryTest, GivenDrmWhenSetupHardwareInfoCalledThenCorrectMaxValuesInGtSystemInfoArePreservedAndIoctlHelperSet) {
    DebugManagerStateRestore restore;
    debugManager.flags.IgnoreProductSpecificIoctlHelper.set(true);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = *NEO::defaultHwInfo.get();
    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.failRetTopology = true;

    drm.storedEUVal = 48;
    drm.storedSSVal = 6;
    hwInfo->gtSystemInfo.SliceCount = 2;

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, hwInfo, setupHardwareInfo};

    drm.ioctlHelper.reset();
    drm.setupHardwareInfo(&device, false);
    EXPECT_NE(nullptr, drm.getIoctlHelper());
    EXPECT_EQ(2u, hwInfo->gtSystemInfo.MaxSlicesSupported);
    EXPECT_EQ(NEO::defaultHwInfo->gtSystemInfo.MaxSubSlicesSupported, hwInfo->gtSystemInfo.MaxSubSlicesSupported);
    EXPECT_EQ(NEO::defaultHwInfo->gtSystemInfo.MaxEuPerSubSlice, hwInfo->gtSystemInfo.MaxEuPerSubSlice);
}

TEST(DrmQueryTest, GivenLessAvailableSubSlicesThanMaxSubSlicesWhenQueryingTopologyInfoThenCorrectMaxSubSliceCountIsSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    auto hwInfo = *NEO::defaultHwInfo.get();
    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = hwInfo;
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.disableSomeTopology = true;

    DrmQueryTopologyData topologyData = {};
    drm.storedSVal = hwInfo.gtSystemInfo.MaxSlicesSupported;
    drm.storedSSVal = hwInfo.gtSystemInfo.MaxSubSlicesSupported;
    drm.storedEUVal = hwInfo.gtSystemInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice;

    drm.engineInfoQueried = true;
    drm.systemInfoQueried = true;
    const auto ret = drm.queryTopology(*executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo(), topologyData);

    const uint8_t topologyBitMask = 0b11000110;

    int actualSliceCount = 0;
    for (int i = 0; i < drm.storedSVal; i++) {
        const uint8_t mask = 0b1 << (i % 8);
        if ((topologyBitMask & mask) != 0) {
            actualSliceCount++;
        }
    }

    int actualSubSliceCount = 0;
    for (int i = 0; i < actualSliceCount; i++) {
        for (int j = 0; j < drm.storedSSVal / drm.storedSVal; j++) {
            const uint8_t mask = 0b1 << (j % 8);
            if ((topologyBitMask & mask) != 0) {
                actualSubSliceCount++;
            }
        }
    }

    int actualEUCount = 0;
    for (int i = 0; i < actualSubSliceCount; i++) {
        for (int j = 0; j < drm.storedEUVal / drm.storedSSVal; j++) {
            const uint8_t mask = 0b1 << (j % 8);
            if ((topologyBitMask & mask) != 0) {
                actualEUCount++;
            }
        }
    }

    EXPECT_GT(drm.storedSVal, actualSliceCount);
    EXPECT_GT(drm.storedSSVal, actualSubSliceCount);
    EXPECT_GT(drm.storedEUVal, actualEUCount);

    if (drm.storedSVal > 1) {
        EXPECT_TRUE(ret);
        EXPECT_LE(topologyData.maxSlices, static_cast<int>(hwInfo.gtSystemInfo.MaxSubSlicesSupported));
        EXPECT_LE(topologyData.maxSubSlicesPerSlice, static_cast<int>(hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported));
    } else {
        EXPECT_FALSE(ret);
        EXPECT_EQ(topologyData.maxSlices, 0);
        EXPECT_EQ(topologyData.maxSubSlicesPerSlice, 0);
    }

    EXPECT_EQ(topologyData.sliceCount, actualSliceCount);
    EXPECT_EQ(topologyData.subSliceCount, actualSubSliceCount);
    EXPECT_EQ(topologyData.euCount, actualEUCount);
    EXPECT_EQ(topologyData.maxEusPerSubSlice, static_cast<int>(hwInfo.gtSystemInfo.MaxEuPerSubSlice));
}

TEST(DrmQueryTest, givenDrmWhenGettingTopologyMapThenCorrectMapIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = *NEO::defaultHwInfo.get();
    DrmMock drmMock{*executionEnvironment->rootDeviceEnvironments[0]};

    DrmQueryTopologyData topologyData = {};

    drmMock.engineInfoQueried = true;
    drmMock.systemInfoQueried = true;
    EXPECT_TRUE(drmMock.queryTopology(*executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo(), topologyData));

    auto topologyMap = drmMock.getTopologyMap();

    EXPECT_LE(1u, topologyMap.size());

    for (uint32_t i = 0; i < topologyMap.size(); i++) {
        EXPECT_EQ(drmMock.storedSVal, static_cast<int>(topologyMap.at(i).sliceIndices.size()));
    }
}

TEST(DrmQueryTest, GivenSingleSliceConfigWhenQueryingTopologyInfoThenSubsliceIndicesAreStored) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    auto hwInfo = *NEO::defaultHwInfo.get();
    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = hwInfo;
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    DrmQueryTopologyData topologyData = {};
    drm.storedSVal = hwInfo.gtSystemInfo.MaxSlicesSupported;
    drm.storedSSVal = hwInfo.gtSystemInfo.MaxSubSlicesSupported;
    drm.storedEUVal = hwInfo.gtSystemInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice;

    drm.engineInfoQueried = true;
    drm.systemInfoQueried = true;
    EXPECT_TRUE(drm.queryTopology(*executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo(), topologyData));

    EXPECT_EQ(topologyData.sliceCount, static_cast<int>(hwInfo.gtSystemInfo.MaxSlicesSupported));
    EXPECT_EQ(topologyData.subSliceCount, static_cast<int>(hwInfo.gtSystemInfo.MaxSubSlicesSupported));
    EXPECT_EQ(topologyData.euCount, static_cast<int>(hwInfo.gtSystemInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice));

    EXPECT_EQ(topologyData.maxSlices, drm.storedSVal);
    EXPECT_EQ(topologyData.maxSubSlicesPerSlice, static_cast<int>(hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported));

    auto topologyMap = drm.getTopologyMap();

    for (uint32_t i = 0; i < topologyMap.size(); i++) {
        EXPECT_EQ(drm.storedSVal, static_cast<int>(topologyMap.at(i).sliceIndices.size()));

        if (drm.storedSVal == 1) {
            EXPECT_EQ(static_cast<size_t>(hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported), topologyMap.at(i).subsliceIndices.size());
        } else {
            EXPECT_EQ(0u, topologyMap.at(i).subsliceIndices.size());
        }

        for (int subsliceId = 0; subsliceId < static_cast<int>(topologyMap.at(i).subsliceIndices.size()); subsliceId++) {
            EXPECT_EQ(subsliceId, topologyMap.at(i).subsliceIndices[subsliceId]);
        }
    }
}

TEST(DrmQueryTest, GivenMultiSliceConfigWhenQueryingTopologyInfoThenSubsliceIndicesAreNotStored) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    auto hwInfo = *NEO::defaultHwInfo.get();
    *executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo() = hwInfo;
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    DrmQueryTopologyData topologyData = {};
    drm.storedSVal = hwInfo.gtSystemInfo.MaxSlicesSupported;
    drm.storedSSVal = hwInfo.gtSystemInfo.MaxSubSlicesSupported;
    drm.storedEUVal = hwInfo.gtSystemInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice;

    drm.engineInfoQueried = true;
    drm.systemInfoQueried = true;
    EXPECT_TRUE(drm.queryTopology(*executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo(), topologyData));

    EXPECT_EQ(topologyData.sliceCount, static_cast<int>(hwInfo.gtSystemInfo.MaxSlicesSupported));
    EXPECT_EQ(topologyData.subSliceCount, static_cast<int>(hwInfo.gtSystemInfo.MaxSubSlicesSupported));
    EXPECT_EQ(topologyData.euCount, static_cast<int>(hwInfo.gtSystemInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice));

    EXPECT_EQ(topologyData.maxSlices, static_cast<int>(drm.storedSVal));
    EXPECT_EQ(topologyData.maxSubSlicesPerSlice, static_cast<int>(hwInfo.gtSystemInfo.MaxSubSlicesSupported / hwInfo.gtSystemInfo.MaxSlicesSupported));

    auto topologyMap = drm.getTopologyMap();

    for (uint32_t i = 0; i < topologyMap.size(); i++) {
        EXPECT_EQ(drm.storedSVal, static_cast<int>(topologyMap.at(i).sliceIndices.size()));

        if (drm.storedSVal > 1) {
            EXPECT_EQ(0u, topologyMap.at(i).subsliceIndices.size());
        } else {
            EXPECT_EQ(static_cast<size_t>(topologyData.maxSubSlicesPerSlice), topologyMap.at(i).subsliceIndices.size());
        }
    }
}

TEST(DrmQueryTest, GivenNonTileArchitectureWhenFrequencyIsQueriedThenFallbackToLegacyInterface) {
    int expectedMaxFrequency = 2000;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
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

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
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

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
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

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
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
    USE_REAL_FILE_SYSTEM();
    int expectedMaxFrequency = 2000;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
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
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock *pDrm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    EXPECT_TRUE(pDrm->isDriverAvailable());
    EXPECT_FALSE(pDrm->skipResourceCleanup());
    delete pDrm;
}

TEST(DrmQueryTest, givenUapiPrelimVersionThenReturnCorrectString) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    std::string prelimVersionFile = getLinuxDevicesPath("device/drm/card1/prelim_uapi_version");
    EXPECT_TRUE(fileExists(prelimVersionFile));

    drm.setPciPath("device");

    std::string prelimVersion = "";
    drm.getPrelimVersion(prelimVersion);

    EXPECT_EQ("2.0", prelimVersion);
}

TEST(DrmQueryTest, givenUapiPrelimVersionWithInvalidPathThenReturnEmptyString) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.setPciPath("invalidPath");

    std::string prelimVersion = "2.0";
    drm.getPrelimVersion(prelimVersion);

    EXPECT_TRUE(prelimVersion.empty());
}

TEST(DrmTest, givenInvalidUapiPrelimVersionThenFallbackToBasePrelim) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    const auto productFamily = defaultHwInfo->platform.eProductFamily;
    std::unique_ptr<IoctlHelper> ioctlHelper(IoctlHelper::getI915Helper(productFamily, "-1", drm));
    EXPECT_NE(nullptr, ioctlHelper.get());
}

TEST(DrmTest, GivenCompletionFenceDebugFlagWhenCreatingDrmObjectThenExpectCorrectSetting) {
    DebugManagerStateRestore restore;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    DrmMock drmDefault{*executionEnvironment->rootDeviceEnvironments[0]};
    drmDefault.callBaseIsVmBindAvailable = true;
    if (drmDefault.isVmBindAvailable()) {
        EXPECT_TRUE(drmDefault.completionFenceSupport());
    } else {
        EXPECT_FALSE(drmDefault.completionFenceSupport());
    }

    debugManager.flags.UseVmBind.set(1);
    debugManager.flags.EnableDrmCompletionFence.set(1);
    DrmMock drmEnabled{*executionEnvironment->rootDeviceEnvironments[0]};
    drmEnabled.callBaseIsVmBindAvailable = true;
    EXPECT_TRUE(drmEnabled.completionFenceSupport());

    debugManager.flags.EnableDrmCompletionFence.set(0);
    DrmMock drmDisabled{*executionEnvironment->rootDeviceEnvironments[0]};
    drmDisabled.callBaseIsVmBindAvailable = true;
    EXPECT_FALSE(drmDisabled.completionFenceSupport());
}

TEST(DrmTest, GivenMinusEbusyIoctlErrorWhenCallingExecbufferThenCallIoctlAgain) {
    MockExecutionEnvironment executionEnvironment{};

    DrmMock drm{*executionEnvironment.rootDeviceEnvironments[0]};

    VariableBackup<decltype(SysCalls::sysCallsIoctl)> mockIoctl(&SysCalls::sysCallsIoctl);
    VariableBackup<int> mockErrno(&errno);

    SysCalls::sysCallsIoctl = [](int fileDescriptor, unsigned long int request, void *arg) -> int {
        static int ioctlCount;
        ioctlCount++;
        if (ioctlCount == 1) {
            errno = -EBUSY;
            return -1;
        }
        ioctlCount = 0;
        return 0;
    };

    EXPECT_EQ(0, drm.Drm::ioctl(DrmIoctl::gemExecbuffer2, nullptr));
}

TEST(DrmTest, GivenIoctlErrorWhenIsGpuHangIsCalledThenErrorIsThrown) {
    MockExecutionEnvironment executionEnvironment{};

    DrmMock drm{*executionEnvironment.rootDeviceEnvironments[0]};
    uint32_t contextId{0};
    EngineDescriptor engineDescriptor{EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular})};

    MockOsContextLinux mockOsContextLinux{drm, 0, contextId, engineDescriptor};
    mockOsContextLinux.drmContextIds.push_back(0);
    mockOsContextLinux.drmContextIds.push_back(3);

    EXPECT_THROW(drm.isGpuHangDetected(mockOsContextLinux), std::runtime_error);
}

TEST(DrmTest, GivenZeroBatchActiveAndZeroBatchPendingResetStatsWhenIsGpuHangIsCalledThenNoHangIsReported) {
    MockExecutionEnvironment executionEnvironment{};

    DrmMock drm{*executionEnvironment.rootDeviceEnvironments[0]};
    uint32_t contextId{0};
    EngineDescriptor engineDescriptor{EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular})};

    MockOsContextLinux mockOsContextLinux{drm, 0, contextId, engineDescriptor};
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
    MockExecutionEnvironment executionEnvironment{};

    DrmMock drm{*executionEnvironment.rootDeviceEnvironments[0]};
    uint32_t contextId{0};
    EngineDescriptor engineDescriptor{EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular})};

    MockOsContextLinux mockOsContextLinux{drm, 0, contextId, engineDescriptor};
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
    MockExecutionEnvironment executionEnvironment{};

    DrmMock drm{*executionEnvironment.rootDeviceEnvironments[0]};
    uint32_t contextId{0};
    EngineDescriptor engineDescriptor{EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular})};

    MockOsContextLinux mockOsContextLinux{drm, 0, contextId, engineDescriptor};
    mockOsContextLinux.drmContextIds.push_back(8);

    ResetStats resetStats{};
    resetStats.contextId = 8;
    resetStats.batchPending = 7;
    drm.resetStatsToReturn.push_back(resetStats);

    bool isGpuHangDetected{};
    EXPECT_NO_THROW(isGpuHangDetected = drm.isGpuHangDetected(mockOsContextLinux));
    EXPECT_TRUE(isGpuHangDetected);
}

class MockIoctlHelperResetStats : public MockIoctlHelper {
  public:
    using MockIoctlHelper::MockIoctlHelper;
    int getResetStats(ResetStats &resetStats, uint32_t *status, ResetStatsFault *resetStatsFault) override {
        int ret = MockIoctlHelper::getResetStats(resetStats, status, resetStatsFault);
        if (status) {
            *status = statusReturnValue;
        }
        if (resetStatsFault) {
            *resetStatsFault = resetStatsFaultReturnValue;
        }
        return ret;
    }

    bool validPageFault(uint16_t flags) override {
        return true;
    }

    uint32_t getStatusForResetStats(bool banned) override {
        if (banned) {
            return statusReturnValue;
        } else {
            return 0u;
        }
    }

    uint32_t statusReturnValue = 0;
    ResetStatsFault resetStatsFaultReturnValue{};
};

TEST(DrmTest, GivenResetStatsWithValidFaultAndContextNotBannedAndDebuggingEnabledWhenIsGpuHangIsCalledThenProcessNotTerminated) {
    DebugManagerStateRestore restore;
    debugManager.flags.DisableScratchPages.set(true);

    MockExecutionEnvironment executionEnvironment{};
    DrmMock drm{*executionEnvironment.rootDeviceEnvironments[0]};
    executionEnvironment.setDebuggingMode(NEO::DebuggingMode::online);
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();
    uint32_t contextId{0};
    EngineDescriptor engineDescriptor{EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular})};
    auto ioctlHelper = std::make_unique<MockIoctlHelperResetStats>(drm);

    MockOsContextLinux mockOsContextLinux{drm, 0, contextId, engineDescriptor};
    mockOsContextLinux.drmContextIds.push_back(0);

    ResetStats resetStatsExpected{};
    ResetStatsFault resetStatsFaultExpected{};
    resetStatsExpected.contextId = 0;
    drm.resetStatsToReturn.push_back(resetStatsExpected);

    resetStatsFaultExpected.flags = 1;
    resetStatsFaultExpected.addr = 0x1234;
    resetStatsFaultExpected.type = 2;
    resetStatsFaultExpected.level = 3;

    ioctlHelper->statusReturnValue = 0u;
    ioctlHelper->resetStatsFaultReturnValue = resetStatsFaultExpected;

    drm.ioctlHelper = std::move(ioctlHelper);
    EXPECT_FALSE(drm.isGpuHangDetected(mockOsContextLinux));
}

TEST(DrmDeathTest, GivenResetStatsWithValidFaultWhenIsGpuHangIsCalledThenProcessTerminated) {
    DebugManagerStateRestore restore;
    debugManager.flags.DisableScratchPages.set(true);

    MockExecutionEnvironment executionEnvironment{};
    DrmMock drm{*executionEnvironment.rootDeviceEnvironments[0]};
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();
    uint32_t contextId{0};
    EngineDescriptor engineDescriptor{EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular})};
    auto ioctlHelper = std::make_unique<MockIoctlHelperResetStats>(drm);

    MockOsContextLinux mockOsContextLinux{drm, 0, contextId, engineDescriptor};
    mockOsContextLinux.drmContextIds.push_back(0);

    ResetStats resetStatsExpected{};
    ResetStatsFault resetStatsFaultExpected{};
    resetStatsExpected.contextId = 0;
    drm.resetStatsToReturn.push_back(resetStatsExpected);

    resetStatsFaultExpected.flags = 1;
    resetStatsFaultExpected.addr = 0x1234;
    resetStatsFaultExpected.type = 2;
    resetStatsFaultExpected.level = 3;

    ioctlHelper->statusReturnValue = 2u;
    ioctlHelper->resetStatsFaultReturnValue = resetStatsFaultExpected;

    drm.ioctlHelper = std::move(ioctlHelper);

    int strSize = std::snprintf(nullptr, 0, "Segmentation fault from GPU at 0x%llx, ctx_id: %u (%s) type: %d (%s), level: %d (%s), access: %d (%s), banned: %d, aborting.\n",
                                static_cast<long long unsigned int>(resetStatsFaultExpected.addr),
                                resetStatsExpected.contextId,
                                EngineHelpers::engineTypeToString(aub_stream::ENGINE_BCS).c_str(),
                                resetStatsFaultExpected.type, GpuPageFaultHelpers::faultTypeToString(static_cast<FaultType>(resetStatsFaultExpected.type)).c_str(),
                                resetStatsFaultExpected.level, GpuPageFaultHelpers::faultLevelToString(static_cast<FaultLevel>(resetStatsFaultExpected.level)).c_str(),
                                resetStatsFaultExpected.access, GpuPageFaultHelpers::faultAccessToString(static_cast<FaultAccess>(resetStatsFaultExpected.access)).c_str(),
                                true) +
                  1;

    std::unique_ptr<char[]> buf(new char[strSize]);
    std::snprintf(buf.get(), strSize, "Segmentation fault from GPU at 0x%llx, ctx_id: %u (%s) type: %d (%s), level: %d (%s), access: %d (%s), banned: %d, aborting.\n",
                  static_cast<long long unsigned int>(resetStatsFaultExpected.addr),
                  resetStatsExpected.contextId,
                  EngineHelpers::engineTypeToString(aub_stream::ENGINE_BCS).c_str(),
                  resetStatsFaultExpected.type, GpuPageFaultHelpers::faultTypeToString(static_cast<FaultType>(resetStatsFaultExpected.type)).c_str(),
                  resetStatsFaultExpected.level, GpuPageFaultHelpers::faultLevelToString(static_cast<FaultLevel>(resetStatsFaultExpected.level)).c_str(),
                  resetStatsFaultExpected.access, GpuPageFaultHelpers::faultAccessToString(static_cast<FaultAccess>(resetStatsFaultExpected.access)).c_str(),
                  true);

    std::string expectedString = std::string(buf.get());

    ::testing::internal::CaptureStderr();
    StreamCapture capture;
    capture.captureStdout();
    EXPECT_THROW(drm.isGpuHangDetected(mockOsContextLinux), std::runtime_error);
    auto stderrString = ::testing::internal::GetCapturedStderr();
    auto stdoutString = capture.getCapturedStdout();
    EXPECT_EQ(expectedString, stderrString);
    EXPECT_EQ(expectedString, stdoutString);
}

struct DrmMockCheckPageFault : public DrmMock {
  public:
    using DrmMock::checkToDisableScratchPage;
    using DrmMock::DrmMock;
    using DrmMock::getGpuFaultCheckThreshold;
};

using DrmDisableScratchPagesDefaultTest = ::testing::Test;
HWTEST2_F(DrmDisableScratchPagesDefaultTest,
          givenDefaultDisableScratchPagesThenCheckingGpuFaultCheckIsSetToDefaultValueAndScratchPageIsDisabled, IsAtLeastXeHpcCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockCheckPageFault drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();
    EXPECT_TRUE(drm.checkToDisableScratchPage());
    EXPECT_EQ(10u, drm.getGpuFaultCheckThreshold());
}

HWTEST2_F(DrmDisableScratchPagesDefaultTest,
          givenDefaultDisableScratchPagesThenCheckingGpuFaultCheckIsSetToDefaultAndScratchPageIsEnabled, IsAtMostXeHpgCore) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockCheckPageFault drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();
    EXPECT_FALSE(drm.checkToDisableScratchPage());
    EXPECT_EQ(10u, drm.getGpuFaultCheckThreshold());
}

TEST(DrmTest, givenDisableScratchPagesWhenSettingGpuFaultCheckThresholdThenThesholdValueIsSet) {
    constexpr unsigned int iteration = 3u;
    constexpr unsigned int threshold = 3u;
    ASSERT_NE(0u, iteration);
    ASSERT_NE(0u, threshold);
    DebugManagerStateRestore restore;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    debugManager.flags.DisableScratchPages.set(false);
    debugManager.flags.GpuFaultCheckThreshold.set(-1);
    DrmMockCheckPageFault drm1{*executionEnvironment->rootDeviceEnvironments[0]};
    drm1.configureScratchPagePolicy();
    drm1.configureGpuFaultCheckThreshold();
    EXPECT_FALSE(drm1.checkToDisableScratchPage());
    EXPECT_EQ(10u, drm1.getGpuFaultCheckThreshold());

    debugManager.flags.DisableScratchPages.set(true);
    debugManager.flags.GpuFaultCheckThreshold.set(-1);
    DrmMockCheckPageFault drm2{*executionEnvironment->rootDeviceEnvironments[0]};
    drm2.configureScratchPagePolicy();
    drm2.configureGpuFaultCheckThreshold();
    EXPECT_TRUE(drm2.checkToDisableScratchPage());
    EXPECT_EQ(10u, drm2.getGpuFaultCheckThreshold());

    debugManager.flags.DisableScratchPages.set(true);
    debugManager.flags.GpuFaultCheckThreshold.set(threshold);
    DrmMockCheckPageFault drm3{*executionEnvironment->rootDeviceEnvironments[0]};
    drm3.configureScratchPagePolicy();
    drm3.configureGpuFaultCheckThreshold();
    EXPECT_TRUE(drm3.checkToDisableScratchPage());
    EXPECT_EQ(threshold, drm3.getGpuFaultCheckThreshold());

    debugManager.flags.DisableScratchPages.set(false);
    debugManager.flags.GpuFaultCheckThreshold.set(threshold);
    DrmMockCheckPageFault drm4{*executionEnvironment->rootDeviceEnvironments[0]};
    drm4.configureScratchPagePolicy();
    drm4.configureGpuFaultCheckThreshold();
    EXPECT_FALSE(drm4.checkToDisableScratchPage());
    EXPECT_EQ(threshold, drm4.getGpuFaultCheckThreshold());
}

struct MockDrmMemoryManagerCheckPageFault : public MockDrmMemoryManager {
    using MockDrmMemoryManager::MockDrmMemoryManager;
    void checkUnexpectedGpuPageFault() override {
        checkUnexpectedGpuPageFaultCalled++;
    }
    size_t checkUnexpectedGpuPageFaultCalled = 0;
};

TEST(DrmTest, givenDisableScratchPagesSetWhenSettingGpuFaultCheckThresholdThenFaultCheckingIsHappeningAfterThreshold) {
    constexpr unsigned int iteration = 3u;
    constexpr unsigned int threshold = 3u;
    ASSERT_NE(0u, iteration);
    ASSERT_NE(0u, threshold);
    DebugManagerStateRestore restore;
    debugManager.flags.DisableScratchPages.set(true);
    debugManager.flags.GpuFaultCheckThreshold.set(threshold);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMock(*rootDeviceEnvironment)));

    auto memoryManager = new MockDrmMemoryManagerCheckPageFault(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    auto &drm = *executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<DrmMock>();
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();

    uint32_t contextId{0};
    EngineDescriptor engineDescriptor{EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular})};

    MockOsContextLinux mockOsContextLinux{drm, 0, contextId, engineDescriptor};
    mockOsContextLinux.drmContextIds.push_back(0);

    ResetStats resetStats{};
    resetStats.contextId = 0;
    drm.resetStatsToReturn.push_back(resetStats);

    bool isGpuHangDetected{};
    for (auto i = 0u; i < iteration; i++) {
        memoryManager->checkUnexpectedGpuPageFaultCalled = 0u;
        for (auto j = 0u; j < threshold; j++) {
            EXPECT_NO_THROW(isGpuHangDetected = drm.isGpuHangDetected(mockOsContextLinux));
            EXPECT_FALSE(isGpuHangDetected);
            EXPECT_EQ(0u, memoryManager->checkUnexpectedGpuPageFaultCalled);
        }
        EXPECT_NO_THROW(isGpuHangDetected = drm.isGpuHangDetected(mockOsContextLinux));
        EXPECT_FALSE(isGpuHangDetected);
        EXPECT_EQ(1u, memoryManager->checkUnexpectedGpuPageFaultCalled);
    }
}

TEST(DrmTest, givenDisableScratchPagesSetWhenSettingGpuFaultCheckThresholdToZeroThenFaultCheckingDoesNotHappen) {
    constexpr unsigned int iteration = 20u;
    ASSERT_NE(0u, iteration);
    DebugManagerStateRestore restore;
    debugManager.flags.DisableScratchPages.set(true);
    debugManager.flags.GpuFaultCheckThreshold.set(0u);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMock(*rootDeviceEnvironment)));

    auto memoryManager = new MockDrmMemoryManagerCheckPageFault(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    auto &drm = *executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<DrmMock>();
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();

    uint32_t contextId{0};
    EngineDescriptor engineDescriptor{EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular})};

    MockOsContextLinux mockOsContextLinux{drm, 0, contextId, engineDescriptor};
    mockOsContextLinux.drmContextIds.push_back(0);

    ResetStats resetStats{};
    resetStats.contextId = 0;
    drm.resetStatsToReturn.push_back(resetStats);

    bool isGpuHangDetected{};
    for (auto i = 0u; i < iteration; i++) {
        EXPECT_NO_THROW(isGpuHangDetected = drm.isGpuHangDetected(mockOsContextLinux));
        EXPECT_FALSE(isGpuHangDetected);
    }
    EXPECT_EQ(0u, memoryManager->checkUnexpectedGpuPageFaultCalled);
}

TEST(DrmTest, whenNotDisablingScratchPagesThenFaultCheckingDoesNotHappen) {
    constexpr unsigned int iteration = 20u;
    ASSERT_NE(0u, iteration);
    DebugManagerStateRestore restore;
    debugManager.flags.DisableScratchPages.set(false);

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMock(*rootDeviceEnvironment)));

    auto memoryManager = new MockDrmMemoryManagerCheckPageFault(GemCloseWorkerMode::gemCloseWorkerInactive, false, false, *executionEnvironment);
    executionEnvironment->memoryManager.reset(memoryManager);
    auto &drm = *executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<DrmMock>();
    drm.configureScratchPagePolicy();
    drm.configureGpuFaultCheckThreshold();

    uint32_t contextId{0};
    EngineDescriptor engineDescriptor{EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular})};

    MockOsContextLinux mockOsContextLinux{drm, 0, contextId, engineDescriptor};
    mockOsContextLinux.drmContextIds.push_back(0);

    ResetStats resetStats{};
    resetStats.contextId = 0;
    drm.resetStatsToReturn.push_back(resetStats);

    bool isGpuHangDetected{};
    for (auto i = 0u; i < iteration; i++) {
        EXPECT_NO_THROW(isGpuHangDetected = drm.isGpuHangDetected(mockOsContextLinux));
        EXPECT_FALSE(isGpuHangDetected);
    }
    EXPECT_EQ(0u, memoryManager->checkUnexpectedGpuPageFaultCalled);
}

TEST(DrmTest, givenSetupIoctlHelperWhenCalledTwiceThenIoctlHelperIsSetOnlyOnce) {
    DebugManagerStateRestore restore;
    debugManager.flags.IgnoreProductSpecificIoctlHelper.set(true);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.ioctlHelper.reset(nullptr);

    const auto productFamily = executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo()->platform.eProductFamily;
    drm.setupIoctlHelper(productFamily);

    EXPECT_NE(nullptr, drm.ioctlHelper.get());
    auto ioctlHelper = drm.ioctlHelper.get();
    drm.setupIoctlHelper(productFamily);
    EXPECT_EQ(ioctlHelper, drm.ioctlHelper.get());
}

TEST(DrmTest, GivenDrmWhenDiscoveringDevicesThenCloseOnExecFlagIsPassedToFdOpen) {
    SysCalls::openFuncCalled = 0;
    VariableBackup<decltype(SysCalls::openFuncCalled)> openCounter(&SysCalls::openFuncCalled);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        EXPECT_TRUE(flags & O_CLOEXEC);
        return 1;
    });

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->setHwInfoAndInitHelpers(defaultHwInfo.get());
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(new DrmMock(*rootDeviceEnvironment)));

    auto devices = Drm::discoverDevices(*executionEnvironment);
    EXPECT_NE(0u, SysCalls::openFuncCalled);

    SysCalls::openFuncCalled = 0;
    VariableBackup<bool> emptyDir(&NEO::returnEmptyFilesVector, true);
    devices = Drm::discoverDevices(*executionEnvironment);
    EXPECT_NE(0u, SysCalls::openFuncCalled);
}

TEST(DrmWrapperTest, WhenGettingDrmIoctlVersionValueThenIoctlHelperIsNotNeeded) {
    EXPECT_EQ(getIoctlRequestValue(DrmIoctl::version, nullptr), static_cast<unsigned int>(DRM_IOCTL_VERSION));
}

TEST(DrmWrapperTest, WhenGettingIoctlStringValueThenProperStringIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    MockIoctlHelper ioctlHelper{drm};
    EXPECT_STREQ(ioctlHelper.getIoctlString(DrmIoctl::getparam).c_str(), "DRM_IOCTL_I915_GETPARAM");
}
TEST(DrmWrapperTest, WhenGettingDrmParamValueStringThenProperStringIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    MockIoctlHelper ioctlHelper{drm};
    std::map<DrmParam, const char *> ioctlCodeStringMap = {
        {DrmParam::paramHasPooledEu, "I915_PARAM_HAS_POOLED_EU"},
        {DrmParam::paramEuTotal, "I915_PARAM_EU_TOTAL"},
        {DrmParam::paramSubsliceTotal, "I915_PARAM_SUBSLICE_TOTAL"},
        {DrmParam::paramMinEuInPool, "I915_PARAM_MIN_EU_IN_POOL"},
        {DrmParam::paramCsTimestampFrequency, "I915_PARAM_CS_TIMESTAMP_FREQUENCY"}};
    for (auto &ioctlCodeString : ioctlCodeStringMap) {
        EXPECT_STREQ(ioctlHelper.getDrmParamString(ioctlCodeString.first).c_str(), ioctlCodeString.second);
    }
}

TEST(DrmHwInfoTest, givenTopologyDataWithoutSystemInfoWhenSettingHwInfoThenCorrectValuesAreSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    auto ioctlHelper = static_cast<MockIoctlHelper *>(drm.ioctlHelper.get());

    ioctlHelper->topologyDataToSet.sliceCount = 2;
    ioctlHelper->topologyDataToSet.subSliceCount = 6;
    ioctlHelper->topologyDataToSet.euCount = 12;
    ioctlHelper->topologyDataToSet.numL3Banks = 3;
    ioctlHelper->topologyDataToSet.maxSlices = 4;
    ioctlHelper->topologyDataToSet.maxSubSlicesPerSlice = 4;
    ioctlHelper->topologyDataToSet.maxEusPerSubSlice = 9;

    ioctlHelper->topologyMapToSet[0].sliceIndices = {1, 2};
    ioctlHelper->topologyMapToSet[0].subsliceIndices = {};
    ioctlHelper->topologyMapToSet[1].sliceIndices = {1, 2};
    ioctlHelper->topologyMapToSet[1].subsliceIndices = {};

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    hwInfo->gtSystemInfo = {};

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, hwInfo, setupHardwareInfo};

    drm.systemInfoQueried = true;
    EXPECT_EQ(nullptr, drm.systemInfo.get());
    drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(nullptr, drm.systemInfo.get());

    EXPECT_EQ(hwInfo->gtSystemInfo.SliceCount, 2u);
    EXPECT_EQ(hwInfo->gtSystemInfo.SubSliceCount, 6u);
    EXPECT_EQ(hwInfo->gtSystemInfo.DualSubSliceCount, 6u);
    EXPECT_EQ(hwInfo->gtSystemInfo.EUCount, 12u);
    EXPECT_EQ(hwInfo->gtSystemInfo.L3BankCount, 3u);
    EXPECT_EQ(hwInfo->gtSystemInfo.MaxEuPerSubSlice, 9u);
    EXPECT_EQ(hwInfo->gtSystemInfo.MaxSlicesSupported, 2u);
    EXPECT_EQ(hwInfo->gtSystemInfo.MaxSubSlicesSupported, 16u);
    EXPECT_EQ(hwInfo->gtSystemInfo.MaxDualSubSlicesSupported, 16u);

    EXPECT_TRUE(hwInfo->gtSystemInfo.IsDynamicallyPopulated);

    EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[0].Enabled);
    EXPECT_TRUE(hwInfo->gtSystemInfo.SliceInfo[1].Enabled);
    EXPECT_TRUE(hwInfo->gtSystemInfo.SliceInfo[2].Enabled);
    EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[3].Enabled);

    for (auto slice = 4u; slice < GT_MAX_SLICE; slice++) {
        EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[slice].Enabled);
    }
}

TEST(DrmHwInfoTest, givenTopologyDataWithAsymtricTopologyMappingWhenSettingHwInfoThenMinimalSubsetIsTaken) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    auto ioctlHelper = static_cast<MockIoctlHelper *>(drm.ioctlHelper.get());

    ioctlHelper->topologyDataToSet.sliceCount = 3;
    ioctlHelper->topologyDataToSet.subSliceCount = 6;
    ioctlHelper->topologyDataToSet.euCount = 12;
    ioctlHelper->topologyDataToSet.numL3Banks = 3;
    ioctlHelper->topologyDataToSet.maxSlices = 4;
    ioctlHelper->topologyDataToSet.maxSubSlicesPerSlice = 4;
    ioctlHelper->topologyDataToSet.maxEusPerSubSlice = 9;

    ioctlHelper->topologyMapToSet[0].sliceIndices = {1, 2, 3};
    ioctlHelper->topologyMapToSet[0].subsliceIndices = {};
    ioctlHelper->topologyMapToSet[1].sliceIndices = {2, 3, 4};
    ioctlHelper->topologyMapToSet[1].subsliceIndices = {};

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    hwInfo->gtSystemInfo = {};

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, hwInfo, setupHardwareInfo};

    drm.systemInfoQueried = true;
    EXPECT_EQ(nullptr, drm.systemInfo.get());
    drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(nullptr, drm.systemInfo.get());
    EXPECT_TRUE(hwInfo->gtSystemInfo.IsDynamicallyPopulated);

    EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[0].Enabled);
    EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[1].Enabled);
    EXPECT_TRUE(hwInfo->gtSystemInfo.SliceInfo[2].Enabled);
    EXPECT_TRUE(hwInfo->gtSystemInfo.SliceInfo[3].Enabled);

    for (auto slice = 4u; slice < GT_MAX_SLICE; slice++) {
        EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[slice].Enabled);
    }
}

TEST(DrmHwInfoTest, givenTopologyDataWithSingleSliceWhenSettingHwInfoThenCorrectValuesAreSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    auto ioctlHelper = static_cast<MockIoctlHelper *>(drm.ioctlHelper.get());

    ioctlHelper->topologyDataToSet.sliceCount = 1;
    ioctlHelper->topologyDataToSet.subSliceCount = 6;
    ioctlHelper->topologyDataToSet.euCount = 12;
    ioctlHelper->topologyDataToSet.numL3Banks = 3;
    ioctlHelper->topologyDataToSet.maxSlices = 4;
    ioctlHelper->topologyDataToSet.maxSubSlicesPerSlice = 4;
    ioctlHelper->topologyDataToSet.maxEusPerSubSlice = 9;

    ioctlHelper->topologyMapToSet[0].sliceIndices = {1};
    ioctlHelper->topologyMapToSet[0].subsliceIndices = {1, 2, 3, 4, 5, 6};
    ioctlHelper->topologyMapToSet[1].sliceIndices = {1};
    ioctlHelper->topologyMapToSet[1].subsliceIndices = {2, 3, 4, 5, 6, 7};

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    hwInfo->gtSystemInfo = {};

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, hwInfo, setupHardwareInfo};

    drm.systemInfoQueried = true;
    EXPECT_EQ(nullptr, drm.systemInfo.get());
    drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(nullptr, drm.systemInfo.get());
    EXPECT_TRUE(hwInfo->gtSystemInfo.IsDynamicallyPopulated);

    EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[0].Enabled);
    EXPECT_TRUE(hwInfo->gtSystemInfo.SliceInfo[1].Enabled);
    EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[2].Enabled);
    EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[3].Enabled);

    for (auto slice = 4u; slice < GT_MAX_SLICE; slice++) {
        EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[slice].Enabled);
    }

    EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[1].SubSliceInfo[0].Enabled);
    EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[1].SubSliceInfo[1].Enabled);
    for (auto subslice = 2u; subslice < 7; subslice++) {
        EXPECT_TRUE(hwInfo->gtSystemInfo.SliceInfo[1].SubSliceInfo[subslice].Enabled);
    }
    for (auto subslice = 7u; subslice < GT_MAX_SUBSLICE_PER_SLICE; subslice++) {
        EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[1].SubSliceInfo[subslice].Enabled);
    }
}

TEST(DrmHwInfoTest, givenTopologyDataWithoutTopologyMappingWhenSettingHwInfoThenCorrectValuesAreSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    auto ioctlHelper = static_cast<MockIoctlHelper *>(drm.ioctlHelper.get());

    ioctlHelper->topologyDataToSet.sliceCount = 2;
    ioctlHelper->topologyDataToSet.subSliceCount = 6;
    ioctlHelper->topologyDataToSet.euCount = 12;
    ioctlHelper->topologyDataToSet.numL3Banks = 3;
    ioctlHelper->topologyDataToSet.maxSlices = 4;
    ioctlHelper->topologyDataToSet.maxSubSlicesPerSlice = 4;
    ioctlHelper->topologyDataToSet.maxEusPerSubSlice = 9;

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    hwInfo->gtSystemInfo = {};

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, hwInfo, setupHardwareInfo};

    drm.systemInfoQueried = true;
    EXPECT_EQ(nullptr, drm.systemInfo.get());
    drm.setupHardwareInfo(&device, false);
    EXPECT_EQ(nullptr, drm.systemInfo.get());
    EXPECT_FALSE(hwInfo->gtSystemInfo.IsDynamicallyPopulated);

    for (auto slice = 0u; slice < GT_MAX_SLICE; slice++) {
        EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[slice].Enabled);
    }
}

TEST(DrmHwInfoTest, givenTopologyDataWithIncorrectSliceMaskWhenSettingHwInfoThenFailureIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    auto ioctlHelper = static_cast<MockIoctlHelper *>(drm.ioctlHelper.get());

    ioctlHelper->topologyDataToSet.sliceCount = 2;
    ioctlHelper->topologyDataToSet.subSliceCount = 6;
    ioctlHelper->topologyDataToSet.euCount = 12;
    ioctlHelper->topologyDataToSet.numL3Banks = 3;
    ioctlHelper->topologyDataToSet.maxSlices = 4;
    ioctlHelper->topologyDataToSet.maxSubSlicesPerSlice = 4;
    ioctlHelper->topologyDataToSet.maxEusPerSubSlice = 9;

    ioctlHelper->topologyMapToSet[0].sliceIndices = {1};
    ioctlHelper->topologyMapToSet[1].sliceIndices = {2};

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    hwInfo->gtSystemInfo = {};

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, hwInfo, setupHardwareInfo};

    drm.systemInfoQueried = true;
    EXPECT_EQ(nullptr, drm.systemInfo.get());
    EXPECT_NE(0, drm.setupHardwareInfo(&device, false));
}

TEST(DrmHwInfoTest, givenTopologyDataWithSingleSliceAndNoCommonSubSliceMaskWhenSettingHwInfoThenSuccessIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    auto ioctlHelper = static_cast<MockIoctlHelper *>(drm.ioctlHelper.get());

    ioctlHelper->topologyDataToSet.sliceCount = 1;
    ioctlHelper->topologyDataToSet.subSliceCount = 3;
    ioctlHelper->topologyDataToSet.euCount = 12;
    ioctlHelper->topologyDataToSet.numL3Banks = 3;
    ioctlHelper->topologyDataToSet.maxSlices = 2;
    ioctlHelper->topologyDataToSet.maxSubSlicesPerSlice = 7;
    ioctlHelper->topologyDataToSet.maxEusPerSubSlice = 9;

    ioctlHelper->topologyMapToSet[0].sliceIndices = {1};
    ioctlHelper->topologyMapToSet[0].subsliceIndices = {0, 1, 2};
    ioctlHelper->topologyMapToSet[1].sliceIndices = {1};
    ioctlHelper->topologyMapToSet[1].subsliceIndices = {4, 5, 6};

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    hwInfo->gtSystemInfo = {};

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, hwInfo, setupHardwareInfo};

    drm.systemInfoQueried = true;
    EXPECT_EQ(nullptr, drm.systemInfo.get());
    EXPECT_EQ(0, drm.setupHardwareInfo(&device, false));

    EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[0].Enabled);
    EXPECT_TRUE(hwInfo->gtSystemInfo.SliceInfo[1].Enabled);

    for (auto slice = 2u; slice < GT_MAX_SLICE; slice++) {
        EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[slice].Enabled);
    }
    for (auto subslice = 0u; subslice < GT_MAX_SUBSLICE_PER_SLICE; subslice++) {
        EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[1].SubSliceInfo[subslice].Enabled);
    }

    EXPECT_GE(GfxCoreHelper::getHighestEnabledDualSubSlice(*hwInfo), 2u * ioctlHelper->topologyDataToSet.maxSubSlicesPerSlice);
}

TEST(DrmHwInfoTest, givenTopologyDataWithSingleSliceAndMoreSubslicesThanMaxSubslicePerSliceWhenSettingHwInfoThenNoSubSliceInfoIsSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    auto ioctlHelper = static_cast<MockIoctlHelper *>(drm.ioctlHelper.get());

    ioctlHelper->topologyDataToSet.sliceCount = 1;
    ioctlHelper->topologyDataToSet.subSliceCount = 3;
    ioctlHelper->topologyDataToSet.euCount = 12;
    ioctlHelper->topologyDataToSet.numL3Banks = 3;
    ioctlHelper->topologyDataToSet.maxSlices = 2;
    ioctlHelper->topologyDataToSet.maxSubSlicesPerSlice = 7;
    ioctlHelper->topologyDataToSet.maxEusPerSubSlice = 9;

    ioctlHelper->topologyMapToSet[0].sliceIndices = {1};
    ioctlHelper->topologyMapToSet[0].subsliceIndices = {0, 1, GT_MAX_SUBSLICE_PER_SLICE};
    ioctlHelper->topologyMapToSet[1].sliceIndices = {1};
    ioctlHelper->topologyMapToSet[1].subsliceIndices = {0, 1, GT_MAX_SUBSLICE_PER_SLICE};

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    hwInfo->gtSystemInfo = {};

    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, hwInfo, setupHardwareInfo};

    drm.systemInfoQueried = true;
    EXPECT_EQ(nullptr, drm.systemInfo.get());
    EXPECT_EQ(0, drm.setupHardwareInfo(&device, false));

    EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[0].Enabled);
    EXPECT_TRUE(hwInfo->gtSystemInfo.SliceInfo[1].Enabled);

    for (auto slice = 2u; slice < GT_MAX_SLICE; slice++) {
        EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[slice].Enabled);
    }
    for (auto subslice = 0u; subslice < GT_MAX_SUBSLICE_PER_SLICE; subslice++) {
        EXPECT_FALSE(hwInfo->gtSystemInfo.SliceInfo[1].SubSliceInfo[subslice].Enabled);
    }

    EXPECT_GE(GfxCoreHelper::getHighestEnabledDualSubSlice(*hwInfo), 2u * ioctlHelper->topologyDataToSet.maxSubSlicesPerSlice);
}

TEST(DrmHwInfoTest, givenTopologyDataWithoutL3BankCountWhenSettingHwInfoThenL3BankCountIsSetBasedOnMaxDualSubslicesBeforeQueryTopology) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);

    auto ioctlHelper = static_cast<MockIoctlHelper *>(drm.ioctlHelper.get());

    ioctlHelper->topologyDataToSet.sliceCount = 1;
    ioctlHelper->topologyDataToSet.subSliceCount = 1;
    ioctlHelper->topologyDataToSet.maxEusPerSubSlice = 1;
    ioctlHelper->topologyDataToSet.euCount = 1;
    ioctlHelper->topologyDataToSet.maxSlices = 1;
    ioctlHelper->topologyDataToSet.maxSubSlicesPerSlice = 16;

    auto hwInfo = executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo();

    hwInfo->gtSystemInfo = {};

    auto setupHardwareInfo = [](HardwareInfo *hwInfo, bool, const ReleaseHelper *) {
        hwInfo->gtSystemInfo.MaxSubSlicesSupported = 8;
        hwInfo->gtSystemInfo.MaxDualSubSlicesSupported = 8;
    };
    DeviceDescriptor device = {0, hwInfo, setupHardwareInfo};

    drm.systemInfoQueried = true;
    EXPECT_EQ(nullptr, drm.systemInfo.get());
    EXPECT_EQ(0, drm.setupHardwareInfo(&device, false));
    EXPECT_EQ(nullptr, drm.systemInfo.get());

    EXPECT_EQ(hwInfo->gtSystemInfo.L3BankCount, 8u);

    EXPECT_EQ(hwInfo->gtSystemInfo.MaxSubSlicesSupported, 16u);
    EXPECT_EQ(hwInfo->gtSystemInfo.MaxDualSubSlicesSupported, 16u);
}

TEST(DrmWrapperTest, givenEAgainOrEIntrOrEBusyWhenCheckingIfReinvokeRequiredThenTrueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    MockIoctlHelper ioctlHelper{drm};
    EXPECT_TRUE(checkIfIoctlReinvokeRequired(EAGAIN, DrmIoctl::getparam, &ioctlHelper));
    EXPECT_TRUE(checkIfIoctlReinvokeRequired(EINTR, DrmIoctl::getparam, &ioctlHelper));
    EXPECT_TRUE(checkIfIoctlReinvokeRequired(EBUSY, DrmIoctl::getparam, &ioctlHelper));
    EXPECT_TRUE(checkIfIoctlReinvokeRequired(-EBUSY, DrmIoctl::getparam, &ioctlHelper));
}

TEST(DrmWrapperTest, givenNoIoctlHelperAndEAgainOrEIntrOrEBusyWhenCheckingIfReinvokeRequiredThenTrueIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    EXPECT_TRUE(checkIfIoctlReinvokeRequired(EAGAIN, DrmIoctl::getparam, nullptr));
    EXPECT_TRUE(checkIfIoctlReinvokeRequired(EINTR, DrmIoctl::getparam, nullptr));
    EXPECT_TRUE(checkIfIoctlReinvokeRequired(EBUSY, DrmIoctl::getparam, nullptr));
    EXPECT_TRUE(checkIfIoctlReinvokeRequired(-EBUSY, DrmIoctl::getparam, nullptr));
}

TEST(DrmWrapperTest, givenErrorWhenCheckingIfReinvokeRequiredThenFalseIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    MockIoctlHelper ioctlHelper{drm};
    EXPECT_FALSE(checkIfIoctlReinvokeRequired(ENOENT, DrmIoctl::getparam, &ioctlHelper));
    EXPECT_FALSE(checkIfIoctlReinvokeRequired(ENOENT, DrmIoctl::getparam, nullptr));
}

TEST(IoctlHelperTest, whenGettingFileNameForFrequencyFilesThenProperStringIsReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto ioctlHelper = drm.getIoctlHelper();
    EXPECT_STREQ("/gt_max_freq_mhz", ioctlHelper->getFileForMaxGpuFrequency().c_str());
    EXPECT_STREQ("/gt/gt2/rps_max_freq_mhz", ioctlHelper->getFileForMaxGpuFrequencyOfSubDevice(2).c_str());
    EXPECT_STREQ("/gt/gt1/mem_RP0_freq_mhz", ioctlHelper->getFileForMaxMemoryFrequencyOfSubDevice(1).c_str());
}

TEST(IoctlHelperTest, givenIoctlHelperWhenCallCreateGemThenProperValuesSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};

    auto ioctlHelper = drm.getIoctlHelper();
    ASSERT_NE(nullptr, ioctlHelper);

    uint64_t size = 1234;
    uint32_t memoryBanks = 3u;

    EXPECT_EQ(0, drm.ioctlCount.gemCreate);
    uint32_t handle = ioctlHelper->createGem(size, memoryBanks, false);
    EXPECT_EQ(1, drm.ioctlCount.gemCreate);

    EXPECT_EQ(size, drm.createParamsSize);

    // dummy mock handle
    EXPECT_EQ(1u, drm.createParamsHandle);
    EXPECT_EQ(handle, drm.createParamsHandle);
}

TEST(DistanceInfoTest, givenDistanceInfosWhenAssignRegionsFromDistancesThenCorrectRegionsSet) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto ioctlHelper = drm.getIoctlHelper();

    auto memoryClassSystem = static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::memoryClassSystem));
    auto memoryClassDevice = static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::memoryClassDevice));

    std::vector<MemoryRegion> memRegions(4);
    memRegions[0] = {{memoryClassSystem, 0}, 1024, 0};
    memRegions[1] = {{memoryClassDevice, 0}, 1024, 0};
    memRegions[2] = {{memoryClassDevice, 1}, 1024, 0};
    memRegions[3] = {{memoryClassDevice, 2}, 1024, 0};
    auto memoryInfo = std::make_unique<MemoryInfo>(memRegions, drm);

    std::vector<EngineClassInstance> engines(3);
    engines[0] = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassRender)), 0};
    engines[1] = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy)), 0};
    engines[2] = {static_cast<uint16_t>(ioctlHelper->getDrmParamValue(DrmParam::engineClassCopy)), 2};

    auto distances = std::vector<DistanceInfo>();
    for (const auto &region : memRegions) {
        if (region.region.memoryClass == memoryClassSystem) {
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
    EXPECT_ANY_THROW(memoryInfo->getMemoryRegionSize(4));
}

TEST(DrmTest, GivenProductSpecificIoctlHelperAvailableWhenSetupIoctlHelperThenCreateProductSpecificOne) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.ioctlHelper.reset();

    auto productFamily = defaultHwInfo->platform.eProductFamily;
    VariableBackup<std::optional<std::function<std::unique_ptr<IoctlHelper>(Drm & drm)>>> createFuncBackup{&ioctlHelperFactory[productFamily]};

    static uint32_t customFuncCalled = 0;

    ioctlHelperFactory[productFamily] = [](Drm &drm) -> std::unique_ptr<IoctlHelper> {
        EXPECT_EQ(0u, customFuncCalled);
        customFuncCalled++;

        return std::make_unique<MockIoctlHelper>(drm);
    };

    customFuncCalled = 0;

    drm.setupIoctlHelper(productFamily);

    EXPECT_EQ(1u, customFuncCalled);
}

TEST(DrmTest, GivenProductSpecificIoctlHelperAvailableAndDebugFlagToIgnoreIsSetWhenSetupIoctlHelperThenDontCreateProductSpecificOne) {
    DebugManagerStateRestore restore;
    debugManager.flags.IgnoreProductSpecificIoctlHelper.set(true);
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    drm.ioctlHelper.reset();

    auto productFamily = defaultHwInfo->platform.eProductFamily;
    VariableBackup<std::optional<std::function<std::unique_ptr<IoctlHelper>(Drm & drm)>>> createFuncBackup{&ioctlHelperFactory[productFamily]};

    static uint32_t customFuncCalled = 0;

    ioctlHelperFactory[productFamily] = [](Drm &drm) -> std::unique_ptr<IoctlHelper> {
        EXPECT_EQ(0u, customFuncCalled);
        customFuncCalled++;

        return std::make_unique<MockIoctlHelper>(drm);
    };

    customFuncCalled = 0;

    drm.setupIoctlHelper(productFamily);

    EXPECT_EQ(0u, customFuncCalled);
}

TEST(DrmTest, GivenSysFsPciPathWhenCallinggetSysFsPciPathBaseNameThenResultIsCorrect) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();

    class DrmMockPciPath : public DrmMock {
      public:
        DrmMockPciPath(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {}
        std::string mockSysFsPciPath = "/sys/devices/pci0000:00/0000:00:02.0/drm/card0";
        std::string getSysFsPciPath() override { return mockSysFsPciPath; }
    };
    DrmMockPciPath drm{*executionEnvironment->rootDeviceEnvironments[0]};
    EXPECT_STREQ("card0", drm.getSysFsPciPathBaseName().c_str());
    drm.mockSysFsPciPath = "/sys/devices/pci0000:00/0000:00:02.0/drm/card7";
    EXPECT_STREQ("card7", drm.getSysFsPciPathBaseName().c_str());
    drm.mockSysFsPciPath = "card8";
    EXPECT_STREQ("card8", drm.getSysFsPciPathBaseName().c_str());
}

using DrmHwTest = ::testing::Test;
HWTEST_F(DrmHwTest, GivenDrmWhenSetupHardwareInfoCalledThenGfxCoreHelperIsInitializedFromProductHelper) {
    DebugManagerStateRestore restore;
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {

        void initializeFromProductHelper(const ProductHelper &productHelper) override {
            initFromProductHelperCalled = true;
        }
        bool initFromProductHelperCalled = false;
    };

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    NEO::RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*executionEnvironment->rootDeviceEnvironments[0]);

    DrmMock drm{*executionEnvironment->rootDeviceEnvironments[0]};
    auto setupHardwareInfo = [](HardwareInfo *, bool, const ReleaseHelper *) {};
    DeviceDescriptor device = {0, executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo(), setupHardwareInfo};

    drm.ioctlHelper = std::make_unique<MockIoctlHelper>(drm);
    drm.setupHardwareInfo(&device, false);

    EXPECT_TRUE(raii.mockGfxCoreHelper->initFromProductHelperCalled);
}
