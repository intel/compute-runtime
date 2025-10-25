/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cpu_copy_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/os_interface/linux/drm_mock_memory_info.h"

#include "drm_neo.h"
#include "gtest/gtest.h"

using namespace NEO;

struct MockSmallBarConfigMemoryInfo : public ::MockMemoryInfo {
    MockSmallBarConfigMemoryInfo(const Drm &drm) : ::MockMemoryInfo(drm) {}
    void setSmallBarDetected(bool smallBar) {
        this->smallBarDetected = smallBar;
    }
};

struct MockSmallBarConfigDrm : public DrmMock {
    MockSmallBarConfigDrm(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {}
    void setMemoryInfo(MemoryInfo *memInfo) {
        this->memoryInfo.reset(memInfo);
    }
};

struct MockSmallBarConfigNonDrm : public MockDriverModel {
    MockSmallBarConfigNonDrm() : MockDriverModel(DriverModelType::unknown) {}
};

class MockOSInterface : public OSInterface {
  public:
    MockOSInterface() = default;
    void setDriverModel(DriverModel *driverModel) { this->driverModel.reset(driverModel); }
};

TEST(CpuCopyHelperTests, givenNullOsInterfaceWhenCheckingSmallBarConfigThenReturnsFalse) {
    EXPECT_FALSE(isSmallBarConfigPresent(nullptr));
}

TEST(CpuCopyHelperTests, givenNullDriverModelWhenCheckingSmallBarConfigThenReturnsFalse) {
    MockOSInterface osIface;
    osIface.setDriverModel(nullptr);
    EXPECT_FALSE(isSmallBarConfigPresent(&osIface));
}

TEST(CpuCopyHelperTests, givenNonDrmDriverModelWhenCheckingSmallBarConfigThenReturnsFalse) {
    MockOSInterface osIface;
    auto nonDrm = std::unique_ptr<MockSmallBarConfigNonDrm>(new MockSmallBarConfigNonDrm());
    osIface.setDriverModel(nonDrm.release());
    EXPECT_FALSE(isSmallBarConfigPresent(&osIface));
}

TEST(CpuCopyHelperTests, givenDrmWithNullMemoryInfoWhenCheckingSmallBarConfigThenReturnsFalse) {
    MockOSInterface osIface;
    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u));
    auto rootEnv = executionEnvironment->rootDeviceEnvironments[0].get();
    auto drm = std::unique_ptr<MockSmallBarConfigDrm>(new MockSmallBarConfigDrm(*rootEnv));
    drm->setMemoryInfo(nullptr);
    osIface.setDriverModel(drm.release());
    EXPECT_FALSE(isSmallBarConfigPresent(&osIface));
}

TEST(CpuCopyHelperTests, givenDrmWithMemoryInfoWhenSmallBarDetectedReturnsTrue) {
    MockOSInterface osIface;
    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u));
    auto rootEnv = executionEnvironment->rootDeviceEnvironments[0].get();
    auto drm = std::unique_ptr<MockSmallBarConfigDrm>(new MockSmallBarConfigDrm(*rootEnv));
    auto memInfo = std::unique_ptr<MockSmallBarConfigMemoryInfo>(new MockSmallBarConfigMemoryInfo(*drm));
    memInfo->setSmallBarDetected(true);
    drm->setMemoryInfo(memInfo.release());
    osIface.setDriverModel(drm.release());
    EXPECT_TRUE(isSmallBarConfigPresent(&osIface));
}

TEST(CpuCopyHelperTests, givenDrmWithMemoryInfoWhenSmallBarDetectedReturnsFalse) {
    MockOSInterface osIface;
    auto executionEnvironment = std::unique_ptr<ExecutionEnvironment>(MockDevice::prepareExecutionEnvironment(defaultHwInfo.get(), 0u));
    auto rootEnv = executionEnvironment->rootDeviceEnvironments[0].get();
    auto drm = std::unique_ptr<MockSmallBarConfigDrm>(new MockSmallBarConfigDrm(*rootEnv));
    auto memInfo = std::unique_ptr<MockSmallBarConfigMemoryInfo>(new MockSmallBarConfigMemoryInfo(*drm));
    memInfo->setSmallBarDetected(false);
    drm->setMemoryInfo(memInfo.release());
    osIface.setDriverModel(drm.release());
    EXPECT_FALSE(isSmallBarConfigPresent(&osIface));
}
