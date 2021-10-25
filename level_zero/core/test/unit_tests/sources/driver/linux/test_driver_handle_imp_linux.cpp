/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

using namespace NEO;

namespace L0 {
namespace ult {

constexpr int mockFd = 0;

class TestDriverMockDrm : public Drm {
  public:
    TestDriverMockDrm(std::string &bdf, RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(mockFd, bdf.c_str()), rootDeviceEnvironment) {}
};

class DriverLinuxFixture : public ::testing::Test {
  public:
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(&hwInfo);
        }
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
        }
        for (auto i = 0u; i < devices.size(); i++) {
            devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            auto osInterface = devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface.get();
            osInterface->setDriverModel(std::make_unique<TestDriverMockDrm>(bdf[i], const_cast<NEO::RootDeviceEnvironment &>(devices[i]->getRootDeviceEnvironment())));
        }
        executionEnvironment->sortNeoDevices();
    }
    void TearDown() override {}

    static constexpr uint32_t numRootDevices = 5u;
    static constexpr uint32_t numSubDevices = 2u;
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::string bdf[numRootDevices] = {"0000:03:04.0", "0000:08:02.0", "0000:08:03.1", "0000:10:03.0", "0000:02:01.0"};
    std::string sortedBdf[numRootDevices] = {"0000:02:01.0", "0000:03:04.0", "0000:08:02.0", "0000:08:03.1", "0000:10:03.0"};
    std::unique_ptr<UltDeviceFactory> deviceFactory;
};

class DriverLinuxWithPciOrderTests : public DriverLinuxFixture {
  public:
    void SetUp() override {
        DebugManagerStateRestore restorer;
        DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(1);

        DriverLinuxFixture::SetUp();
    }
    void TearDown() override {
        DriverLinuxFixture::TearDown();
    }
};

TEST_F(DriverLinuxWithPciOrderTests, GivenEnvironmentVariableForDeviceOrderAccordingToPciSetWhenRetrievingNeoDevicesThenNeoDevicesAccordingToBusOrderRetrieved) {
    NEO::MockCompilerEnableGuard mock(true);
    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto L0Device = driverHandle->devices[i];
        if (L0Device != nullptr) {
            auto pDrm = L0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[L0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
            EXPECT_TRUE(!pDrm->getPciPath().compare(sortedBdf[i]));
        }
    }
    delete driverHandle;
}

class DriverLinuxWithouthPciOrderTests : public DriverLinuxFixture {
  public:
    void SetUp() override {
        DebugManagerStateRestore restorer;
        DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(0);

        DriverLinuxFixture::SetUp();
    }
    void TearDown() override {
        DriverLinuxFixture::TearDown();
    }
};

TEST_F(DriverLinuxWithouthPciOrderTests, GivenNoEnvironmentVariableForDeviceOrderAccordingToPciSetWhenRetrievingNeoDevicesThenNeoDevicesAreNotSorted) {
    NEO::MockCompilerEnableGuard mock(true);
    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto L0Device = driverHandle->devices[i];
        if (L0Device != nullptr) {
            auto pDrm = L0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[L0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
            EXPECT_FALSE(!pDrm->getPciPath().compare(sortedBdf[i]));
        }
    }
    delete driverHandle;
}

class DriverPciOrderWitSimilarBusLinuxFixture : public ::testing::Test {
  public:
    void SetUp() override {
        DebugManagerStateRestore restorer;
        DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(1);

        NEO::MockCompilerEnableGuard mock(true);
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(&hwInfo);
        }
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
        }
        for (auto i = 0u; i < devices.size(); i++) {
            devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            auto osInterface = devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface.get();
            osInterface->setDriverModel(std::make_unique<TestDriverMockDrm>(bdf[i], const_cast<NEO::RootDeviceEnvironment &>(devices[i]->getRootDeviceEnvironment())));
        }
        executionEnvironment->sortNeoDevices();
    }
    void TearDown() override {}

    static constexpr uint32_t numRootDevices = 4u;
    static constexpr uint32_t numSubDevices = 2u;
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::string bdf[numRootDevices] = {"0000:03:04.0", "0000:03:05.0", "0000:03:06.0", "0000:03:01.0"};
    std::string sortedBdf[numRootDevices] = {"0000:03:01.0", "0000:03:04.0", "0000:03:05.0", "0000:03:06.0"};
    std::unique_ptr<UltDeviceFactory> deviceFactory;
};

TEST_F(DriverPciOrderWitSimilarBusLinuxFixture, GivenEnvironmentVariableForDeviceOrderAccordingToPciSetWhenRetrievingNeoDevicesThenNeoDevicesAccordingToBusOrderRetrieved) {
    NEO::MockCompilerEnableGuard mock(true);
    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto L0Device = driverHandle->devices[i];
        if (L0Device != nullptr) {
            auto pDrm = L0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[L0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
            EXPECT_TRUE(!pDrm->getPciPath().compare(sortedBdf[i]));
        }
    }
    delete driverHandle;
}

class DriverPciOrderWitDifferentDeviceLinuxFixture : public ::testing::Test {
  public:
    void SetUp() override {
        DebugManagerStateRestore restorer;
        DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(1);

        NEO::MockCompilerEnableGuard mock(true);
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(&hwInfo);
        }
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
        }
        for (auto i = 0u; i < devices.size(); i++) {
            devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            auto osInterface = devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface.get();
            osInterface->setDriverModel(std::make_unique<TestDriverMockDrm>(bdf[i], const_cast<NEO::RootDeviceEnvironment &>(devices[i]->getRootDeviceEnvironment())));
        }
        executionEnvironment->sortNeoDevices();
    }
    void TearDown() override {}

    static constexpr uint32_t numRootDevices = 2u;
    static constexpr uint32_t numSubDevices = 2u;
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::string bdf[numRootDevices] = {"0000:03:05.0", "0000:03:04.0"};
    std::string sortedBdf[numRootDevices] = {"0000:03:04.0", "0000:03:05.0"};
    std::unique_ptr<UltDeviceFactory> deviceFactory;
};

TEST_F(DriverPciOrderWitDifferentDeviceLinuxFixture, GivenEnvironmentVariableForDeviceOrderAccordingToPciSetWhenRetrievingNeoDevicesThenNeoDevicesAccordingToBusOrderRetrieved) {
    NEO::MockCompilerEnableGuard mock(true);
    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto L0Device = driverHandle->devices[i];
        if (L0Device != nullptr) {
            auto pDrm = L0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[L0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
            EXPECT_TRUE(!pDrm->getPciPath().compare(sortedBdf[i]));
        }
    }
    delete driverHandle;
}

class DriverPciOrderWitSimilarBusAndDeviceLinuxFixture : public ::testing::Test {
  public:
    void SetUp() override {
        DebugManagerStateRestore restorer;
        DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(1);

        NEO::MockCompilerEnableGuard mock(true);
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(&hwInfo);
        }
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
        }
        for (auto i = 0u; i < devices.size(); i++) {
            devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            auto osInterface = devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface.get();
            osInterface->setDriverModel(std::make_unique<TestDriverMockDrm>(bdf[i], const_cast<NEO::RootDeviceEnvironment &>(devices[i]->getRootDeviceEnvironment())));
        }
        executionEnvironment->sortNeoDevices();
    }
    void TearDown() override {}

    static constexpr uint32_t numRootDevices = 2u;
    static constexpr uint32_t numSubDevices = 2u;
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::string bdf[numRootDevices] = {"0000:03:04.1", "0000:03:04.0"};
    std::string sortedBdf[numRootDevices] = {"0000:03:04.0", "0000:03:04.1"};
    std::unique_ptr<UltDeviceFactory> deviceFactory;
};

TEST_F(DriverPciOrderWitSimilarBusAndDeviceLinuxFixture, GivenEnvironmentVariableForDeviceOrderAccordingToPciSetWhenRetrievingNeoDevicesThenNeoDevicesAccordingToBusOrderRetrieved) {
    NEO::MockCompilerEnableGuard mock(true);
    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto L0Device = driverHandle->devices[i];
        if (L0Device != nullptr) {
            auto pDrm = L0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[L0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
            EXPECT_TRUE(!pDrm->getPciPath().compare(sortedBdf[i]));
        }
    }
    delete driverHandle;
}

class DriverPciOrderWitSimilarBDFLinuxFixture : public ::testing::Test {
  public:
    void SetUp() override {
        DebugManagerStateRestore restorer;
        DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(1);

        NEO::MockCompilerEnableGuard mock(true);
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(&hwInfo);
        }
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
        }
        for (auto i = 0u; i < devices.size(); i++) {
            devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            auto osInterface = devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface.get();
            osInterface->setDriverModel(std::make_unique<TestDriverMockDrm>(bdf[i], const_cast<NEO::RootDeviceEnvironment &>(devices[i]->getRootDeviceEnvironment())));
        }
        executionEnvironment->sortNeoDevices();
    }
    void TearDown() override {}

    static constexpr uint32_t numRootDevices = 2u;
    static constexpr uint32_t numSubDevices = 2u;
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::string bdf[numRootDevices] = {"0001:03:04.0", "0000:03:04.0"};
    std::string sortedBdf[numRootDevices] = {"0000:03:04.0", "0001:03:04.0"};
    std::unique_ptr<UltDeviceFactory> deviceFactory;
};

TEST_F(DriverPciOrderWitSimilarBDFLinuxFixture, GivenEnvironmentVariableForDeviceOrderAccordingToPciSetWhenRetrievingNeoDevicesThenNeoDevicesAccordingToDomainOrderRetrieved) {
    NEO::MockCompilerEnableGuard mock(true);
    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto L0Device = driverHandle->devices[i];
        if (L0Device != nullptr) {
            auto pDrm = L0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[L0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
            EXPECT_TRUE(!pDrm->getPciPath().compare(sortedBdf[i]));
        }
    }
    delete driverHandle;
}

} // namespace ult
} // namespace L0
