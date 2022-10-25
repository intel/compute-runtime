/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
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

        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(&hwInfo);
        }
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<NEO::DrmMemoryOperationsHandlerBind>(*executionEnvironment->rootDeviceEnvironments[i], i);
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

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            NEO::DrmMemoryOperationsHandlerBind *drm = static_cast<DrmMemoryOperationsHandlerBind *>(executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface.get());
            EXPECT_EQ(drm->getRootDeviceIndex(), i);
        }
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

    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
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

    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
            EXPECT_FALSE(!pDrm->getPciPath().compare(sortedBdf[i]));
        }
    }
    delete driverHandle;
}

class DriverPciOrderWithSimilarBusLinuxFixture : public ::testing::Test {
  public:
    void SetUp() override {
        DebugManagerStateRestore restorer;
        DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(1);

        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(&hwInfo);
        }
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<NEO::DrmMemoryOperationsHandlerBind>(*executionEnvironment->rootDeviceEnvironments[i], i);
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

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            NEO::DrmMemoryOperationsHandlerBind *drm = static_cast<DrmMemoryOperationsHandlerBind *>(executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface.get());
            EXPECT_EQ(drm->getRootDeviceIndex(), i);
        }
    }
    void TearDown() override {}

    static constexpr uint32_t numRootDevices = 4u;
    static constexpr uint32_t numSubDevices = 2u;
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::string bdf[numRootDevices] = {"0000:03:04.0", "0000:03:05.0", "0000:03:06.0", "0000:03:01.0"};
    std::string sortedBdf[numRootDevices] = {"0000:03:01.0", "0000:03:04.0", "0000:03:05.0", "0000:03:06.0"};
    std::unique_ptr<UltDeviceFactory> deviceFactory;
};

TEST_F(DriverPciOrderWithSimilarBusLinuxFixture, GivenEnvironmentVariableForDeviceOrderAccordingToPciSetWhenRetrievingNeoDevicesThenNeoDevicesAccordingToBusOrderRetrieved) {

    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
            EXPECT_TRUE(!pDrm->getPciPath().compare(sortedBdf[i]));
        }
    }
    delete driverHandle;
}

TEST_F(DriverPciOrderWithSimilarBusLinuxFixture,
       GivenEnvironmentVariableForDeviceOrderAccordingToPciSetWhenRetrievingNeoDevicesAndThenInitializingVertexesThenNeoDevicesAccordingToBusOrderRetrieved) {

    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    driverHandle->initializeVertexes();

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
            EXPECT_TRUE(!pDrm->getPciPath().compare(sortedBdf[i]));
        }
    }
    delete driverHandle;
}

class DriverPciOrderWithDifferentDeviceLinuxFixture : public ::testing::Test {
  public:
    void SetUp() override {
        DebugManagerStateRestore restorer;
        DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(1);

        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(&hwInfo);
        }
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<NEO::DrmMemoryOperationsHandlerBind>(*executionEnvironment->rootDeviceEnvironments[i], i);
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

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            NEO::DrmMemoryOperationsHandlerBind *drm = static_cast<DrmMemoryOperationsHandlerBind *>(executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface.get());
            EXPECT_EQ(drm->getRootDeviceIndex(), i);
        }
    }
    void TearDown() override {}

    static constexpr uint32_t numRootDevices = 2u;
    static constexpr uint32_t numSubDevices = 2u;
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::string bdf[numRootDevices] = {"0000:03:05.0", "0000:03:04.0"};
    std::string sortedBdf[numRootDevices] = {"0000:03:04.0", "0000:03:05.0"};
    std::unique_ptr<UltDeviceFactory> deviceFactory;
};

TEST_F(DriverPciOrderWithDifferentDeviceLinuxFixture, GivenEnvironmentVariableForDeviceOrderAccordingToPciSetWhenRetrievingNeoDevicesThenNeoDevicesAccordingToBusOrderRetrieved) {

    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
            EXPECT_TRUE(!pDrm->getPciPath().compare(sortedBdf[i]));
        }
    }
    delete driverHandle;
}

TEST_F(DriverPciOrderWithDifferentDeviceLinuxFixture,
       GivenEnvironmentVariableForDeviceOrderAccordingToPciSetWhenRetrievingNeoDevicesAndThenInitializingVertexesThenNeoDevicesAccordingToBusOrderRetrieved) {

    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    driverHandle->initializeVertexes();

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
            EXPECT_TRUE(!pDrm->getPciPath().compare(sortedBdf[i]));
        }
    }
    delete driverHandle;
}

class DriverPciOrderWithSimilarBusAndDeviceLinuxFixture : public ::testing::Test {
  public:
    void SetUp() override {
        DebugManagerStateRestore restorer;
        DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(1);

        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(&hwInfo);
        }
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<NEO::DrmMemoryOperationsHandlerBind>(*executionEnvironment->rootDeviceEnvironments[i], i);
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

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            NEO::DrmMemoryOperationsHandlerBind *drm = static_cast<DrmMemoryOperationsHandlerBind *>(executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface.get());
            EXPECT_EQ(drm->getRootDeviceIndex(), i);
        }
    }
    void TearDown() override {}

    static constexpr uint32_t numRootDevices = 2u;
    static constexpr uint32_t numSubDevices = 2u;
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::string bdf[numRootDevices] = {"0000:03:04.1", "0000:03:04.0"};
    std::string sortedBdf[numRootDevices] = {"0000:03:04.0", "0000:03:04.1"};
    std::unique_ptr<UltDeviceFactory> deviceFactory;
};

TEST_F(DriverPciOrderWithSimilarBusAndDeviceLinuxFixture, GivenEnvironmentVariableForDeviceOrderAccordingToPciSetWhenRetrievingNeoDevicesThenNeoDevicesAccordingToBusOrderRetrieved) {

    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
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

        auto executionEnvironment = new NEO::MockExecutionEnvironment(NEO::defaultHwInfo.get(), true, numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<NEO::DrmMemoryOperationsHandlerBind>(*executionEnvironment->rootDeviceEnvironments[i], i);
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

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            NEO::DrmMemoryOperationsHandlerBind *drm = static_cast<DrmMemoryOperationsHandlerBind *>(executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface.get());
            EXPECT_EQ(drm->getRootDeviceIndex(), i);
        }
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

    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
            EXPECT_TRUE(!pDrm->getPciPath().compare(sortedBdf[i]));
        }
    }
    delete driverHandle;
}

class DriverPciOrderSortDoesNothing : public ::testing::Test {
  public:
    void SetUp() override {
        DebugManagerStateRestore restorer;
        DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(1);

        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(&hwInfo);
        }
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface = std::make_unique<NEO::DrmMemoryOperationsHandlerBind>(*executionEnvironment->rootDeviceEnvironments[i], i);
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

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            NEO::DrmMemoryOperationsHandlerBind *drm = static_cast<DrmMemoryOperationsHandlerBind *>(executionEnvironment->rootDeviceEnvironments[i]->memoryOperationsInterface.get());
            EXPECT_EQ(drm->getRootDeviceIndex(), i);
        }
    }
    void TearDown() override {}

    static constexpr uint32_t numRootDevices = 2u;
    static constexpr uint32_t numSubDevices = 2u;
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::string bdf[numRootDevices] = {"0000:03:04.0", "0001:03:04.0"};
    std::string sortedBdf[numRootDevices] = {"0000:03:04.0", "0001:03:04.0"};
    std::unique_ptr<UltDeviceFactory> deviceFactory;
};

TEST_F(DriverPciOrderSortDoesNothing, GivenEnvironmentVariableForDeviceOrderAccordingToPciSetThenVerifyCaseSortDoesNothing) {

    DriverHandleImp *driverHandle = new DriverHandleImp;

    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto l0Device = driverHandle->devices[i];
        if (l0Device != nullptr) {
            auto pDrm = l0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0Device->getRootDeviceIndex()]->osInterface->getDriverModel()->as<Drm>();
            EXPECT_NE(pDrm, nullptr);
            EXPECT_TRUE(!pDrm->getPciPath().compare(sortedBdf[i]));
        }
    }
    delete driverHandle;
}

class DriverWDDMLinuxFixture : public ::testing::Test {
  public:
    void SetUp() override {

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
            devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface.reset(new NEO::OSInterface());
            devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
        }
    }
    void TearDown() override {}

    NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
    static constexpr uint32_t numRootDevices = 5u;
    static constexpr uint32_t numSubDevices = 2u;
    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::unique_ptr<UltDeviceFactory> deviceFactory;
};

TEST_F(DriverWDDMLinuxFixture, ClearPciSortFlagToVerifyCodeCoverageOnly) {

    DriverHandleImp *driverHandle = new DriverHandleImp;
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));
    DebugManagerStateRestore restorer;
    DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(0);

    executionEnvironment->sortNeoDevices();

    delete driverHandle;
}

TEST_F(DriverWDDMLinuxFixture, ClearPciSortFlagWithFabricEnumerationToVerifyCodeCoverageOnly) {

    DriverHandleImp *driverHandle = new DriverHandleImp;
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    driverHandle->initializeVertexes();

    DebugManagerStateRestore restorer;
    DebugManager.flags.ZE_ENABLE_PCI_ID_DEVICE_ORDER.set(0);

    executionEnvironment->sortNeoDevices();

    delete driverHandle;
}

} // namespace ult
} // namespace L0
