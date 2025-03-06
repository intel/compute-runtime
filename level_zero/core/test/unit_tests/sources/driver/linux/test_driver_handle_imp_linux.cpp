/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace NEO {
extern std::map<std::string, std::vector<std::string>> directoryFilesMap;
}
namespace L0 {
namespace ult {

class MockIoctlQueryFabricStats : public NEO::IoctlHelperPrelim20 {
  public:
    using IoctlHelperPrelim20::IoctlHelperPrelim20;
    bool getFabricLatency(uint32_t fabricId, uint32_t &latency, uint32_t &bandwidth) override {
        latency = 1;
        bandwidth = 10;
        return true;
    }
};

class DriverQueryPeerTest : public ::testing::Test {
  public:
    class DrmMockQueryFabricStats : public DrmMock {
      public:
        DrmMockQueryFabricStats(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {}
        std::string getSysFsPciPath() override {
            return "/sys/class/drm/card1";
        }
    };

    void SetUp() override {
        DebugManagerStateRestore restorer;

        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);

        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(&hwInfo);
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface =
                std::make_unique<NEO::DrmMemoryOperationsHandlerBind>(*executionEnvironment->rootDeviceEnvironments[i], i);
        }
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
            devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            auto osInterface = devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface.get();
            auto drmMock = new DrmMockQueryFabricStats(*executionEnvironment->rootDeviceEnvironments[i]);
            drmMock->useBaseGetDeviceMemoryPhysicalSizeInBytes = false;
            drmMock->ioctlHelper.reset(new MockIoctlQueryFabricStats(*drmMock));
            osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));
        }
    }
    void TearDown() override {}

    static constexpr uint32_t numRootDevices = 2u;
    static constexpr uint32_t numSubDevices = 2u;
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
};

TEST_F(DriverQueryPeerTest, whenQueryingPeerStatsWithNoFileEntriesThenFallBackCopyIsUsedAndSuccessIsReturned) {
    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
        }
    }

    auto device0 = driverHandle->devices[0];
    auto device1 = driverHandle->devices[1];
    ze_bool_t value = false;
    ze_result_t res = device0->canAccessPeer(device1, &value);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(value);

    delete driverHandle;

    NEO::directoryFilesMap.clear();
}

TEST_F(DriverQueryPeerTest, whenQueryingPeerStatsWithNoFabricTThenFallBackCopyIsUsedAndSuccessIsReturned) {
    NEO::directoryFilesMap.insert({"/sys/class/drm/card0/device", {"unknown"}});
    NEO::directoryFilesMap.insert({"/sys/class/drm/card1/device", {"unknown"}});

    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
        }
    }

    auto device0 = driverHandle->devices[0];
    auto device1 = driverHandle->devices[1];
    ze_bool_t value = false;
    ze_result_t res = device0->canAccessPeer(device1, &value);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(value);

    delete driverHandle;

    NEO::directoryFilesMap.clear();
}

TEST_F(DriverQueryPeerTest, whenQueryingPeerStatsWithFabricAndFdIsNotReturnedTheFallbackCopyIsUsedSuccessIsReturned) {
    NEO::directoryFilesMap.insert({"/sys/class/drm/card0/device", {"i915.iaf.0"}});
    NEO::directoryFilesMap.insert({"/sys/class/drm/card1/device", {"i915.iaf.0"}});

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return -1;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        return -1;
    });

    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
        }
    }

    auto device0 = driverHandle->devices[0];
    auto device1 = driverHandle->devices[1];
    ze_bool_t value = false;
    ze_result_t res = device0->canAccessPeer(device1, &value);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(value);

    delete driverHandle;
    NEO::directoryFilesMap.clear();
}

TEST_F(DriverQueryPeerTest, whenQueryingPeerStatsWithFabricThenSuccessIsReturned) {
    NEO::directoryFilesMap.insert({"/sys/class/drm/card0/device", {"i915.iaf.0"}});
    NEO::directoryFilesMap.insert({"/sys/class/drm/card1/device", {"i915.iaf.0"}});

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "i915.iaf.0/iaf_fabric_id"};

        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"i915.iaf.0/iaf_fabric_id", "63"}};

        fd -= 1;

        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy_s(buf, supportedFiles[fd].second.size(), supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }

        return -1;
    });

    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
        }
    }

    auto device0 = driverHandle->devices[0];
    auto device1 = driverHandle->devices[1];
    ze_bool_t value = false;
    ze_result_t res = device0->canAccessPeer(device1, &value);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(value);

    delete driverHandle;
    NEO::directoryFilesMap.clear();
}

TEST_F(DriverQueryPeerTest, whenQueryingPeerStatsWithLegacyFabricThenSuccessIsReturned) {

    NEO::directoryFilesMap.insert({"/sys/class/drm/card0/device", {"iaf.0"}});
    NEO::directoryFilesMap.insert({"/sys/class/drm/card1/device", {"iaf.0"}});

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "iaf.0/iaf_fabric_id"};

        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"iaf.0/iaf_fabric_id", "63"}};

        fd -= 1;

        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy_s(buf, supportedFiles[fd].second.size(), supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }

        return -1;
    });

    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
        }
    }

    auto device0 = driverHandle->devices[0];
    auto device1 = driverHandle->devices[1];
    ze_bool_t value = false;
    ze_result_t res = device0->canAccessPeer(device1, &value);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(value);

    delete driverHandle;

    NEO::directoryFilesMap.clear();
}

class DriverQueryPeerTestOsInterfaceFail : public ::testing::Test {
  public:
    class DrmMockQueryFabricStats : public DrmMock {
      public:
        DrmMockQueryFabricStats(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {}
        std::string getSysFsPciPath() override {
            return "/sys/class/drm/card1";
        }
    };

    void SetUp() override {
        DebugManagerStateRestore restorer;

        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);

        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(&hwInfo);
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface =
                std::make_unique<NEO::DrmMemoryOperationsHandlerBind>(*executionEnvironment->rootDeviceEnvironments[i], i);
        }
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
            devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            auto osInterface = devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface.get();
            auto drmMock = new DrmMockQueryFabricStats(*executionEnvironment->rootDeviceEnvironments[i]);
            drmMock->useBaseGetDeviceMemoryPhysicalSizeInBytes = false;
            osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));
        }
    }
    void TearDown() override {}

    static constexpr uint32_t numRootDevices = 2u;
    static constexpr uint32_t numSubDevices = 2u;
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
};

TEST_F(DriverQueryPeerTestOsInterfaceFail, whenQueryingPeerStatsWithNoOsInterfaceOnPeerThenFallBackCopyIsUsedAndSuccessIsReturned) {
    NEO::directoryFilesMap.insert({"/sys/class/drm/card0/device", {"unknown"}});
    NEO::directoryFilesMap.insert({"/sys/class/drm/card1/device", {"unknown"}});

    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
        }
    }

    auto device0 = driverHandle->devices[0];
    auto device1 = driverHandle->devices[1];
    ze_bool_t value = false;
    ze_result_t res = device0->canAccessPeer(device1, &value);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(value);

    delete driverHandle;

    NEO::directoryFilesMap.clear();
}

class MockIoctlQueryFabricStatsFail : public NEO::IoctlHelperPrelim20 {
  public:
    using IoctlHelperPrelim20::IoctlHelperPrelim20;
    bool getFabricLatency(uint32_t fabricId, uint32_t &latency, uint32_t &bandwidth) override {
        return false;
    }
};

class DriverQueryPeerTestFail : public ::testing::Test {
  public:
    class DrmMockQueryFabricStats : public DrmMock {
      public:
        DrmMockQueryFabricStats(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {}
        std::string getSysFsPciPath() override {
            return "/sys/class/drm/card1";
        }
    };

    void SetUp() override {
        DebugManagerStateRestore restorer;

        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);

        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(&hwInfo);
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface =
                std::make_unique<NEO::DrmMemoryOperationsHandlerBind>(*executionEnvironment->rootDeviceEnvironments[i], i);
        }
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
            devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            auto osInterface = devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface.get();
            auto drmMock = new DrmMockQueryFabricStats(*executionEnvironment->rootDeviceEnvironments[i]);
            drmMock->useBaseGetDeviceMemoryPhysicalSizeInBytes = false;
            drmMock->ioctlHelper.reset(new MockIoctlQueryFabricStatsFail(*drmMock));
            osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));
        }
    }
    void TearDown() override {}

    static constexpr uint32_t numRootDevices = 2u;
    static constexpr uint32_t numSubDevices = 2u;
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
};

TEST_F(DriverQueryPeerTestFail, whenQueryingPeerStatsWithFabricAndIoctlFailsThenFallBackCopyIsUsedAndSuccessIsReturned) {
    NEO::directoryFilesMap.insert({"/sys/class/drm/card0/device", {"iaf.0"}});
    NEO::directoryFilesMap.insert({"/sys/class/drm/card1/device", {"iaf.0"}});

    VariableBackup<decltype(NEO::SysCalls::sysCallsOpen)> mockOpen(&NEO::SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        std::vector<std::string> supportedFiles = {
            "iaf.0/iaf_fabric_id"};

        auto itr = std::find(supportedFiles.begin(), supportedFiles.end(), std::string(pathname));
        if (itr != supportedFiles.end()) {
            return static_cast<int>(std::distance(supportedFiles.begin(), itr)) + 1;
        }
        return 0;
    });

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, [](int fd, void *buf, size_t count, off_t offset) -> ssize_t {
        std::vector<std::pair<std::string, std::string>> supportedFiles = {
            {"iaf.0/iaf_fabric_id", "63"}};

        fd -= 1;

        if ((fd >= 0) && (fd < static_cast<int>(supportedFiles.size()))) {
            memcpy_s(buf, supportedFiles[fd].second.size(), supportedFiles[fd].second.c_str(), supportedFiles[fd].second.size());
            return supportedFiles[fd].second.size();
        }

        return -1;
    });

    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
        }
    }

    auto device0 = driverHandle->devices[0];
    auto device1 = driverHandle->devices[1];
    ze_bool_t value = false;

    ze_result_t res = device0->canAccessPeer(device1, &value);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(value);

    delete driverHandle;

    NEO::directoryFilesMap.clear();
}

} // namespace ult
} // namespace L0
