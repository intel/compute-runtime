/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_interface.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

using namespace NEO;

namespace L0 {
namespace ult {
const uint32_t numRootDevices = 4u;
const uint32_t numSubDevices = 2u;
constexpr int mockFd = 0;
class TestDriverMockDrm : public Drm {
  public:
    TestDriverMockDrm(std::string &bdf, RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceId>(mockFd, bdf.c_str()), rootDeviceEnvironment) {}
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
        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, numSubDevices, *executionEnvironment);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
        }
        for (auto i = 0u; i < devices.size(); i++) {
            devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface = std::make_unique<NEO::OSInterface>();
            auto osInterface = devices[i]->getExecutionEnvironment()->rootDeviceEnvironments[i]->osInterface.get()->get();
            osInterface->setDrm(new TestDriverMockDrm(bdf[i], const_cast<NEO::RootDeviceEnvironment &>(devices[i]->getRootDeviceEnvironment())));
        }
    }
    void TearDown() override {}

    std::vector<std::unique_ptr<NEO::Device>> devices;
    std::string bdf[numRootDevices] = {"08:02.0", "03:04.0", "10:03.0", "02:01.0"};
    std::string sortedBdf[numRootDevices] = {"02:01.0", "03:04.0", "08:02.0", "10:03.0"};
    std::unique_ptr<UltDeviceFactory> deviceFactory;
};

TEST_F(DriverLinuxFixture, GivenEnvironmentVariableForDeviceOrderAccordingToPciSetWhenRetrievingNeoDevicesThenNeoDevicesAccordingToBusOrderRetrieved) {
    DriverHandleImp *driverHandle = new DriverHandleImp;
    driverHandle->enablePciIdDeviceOrder = true;
    EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->initialize(std::move(devices)));

    for (uint32_t i = 0; i < numRootDevices; i++) {
        auto L0Device = driverHandle->devices[i];
        if (L0Device != nullptr) {
            auto pDrm = L0Device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[L0Device->getRootDeviceIndex()]->osInterface.get()->get()->getDrm();
            EXPECT_NE(pDrm, nullptr);
            EXPECT_TRUE(!pDrm->getPciPath().compare(sortedBdf[i]));
        }
    }
    delete driverHandle;
}

} // namespace ult
} // namespace L0
