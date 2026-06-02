/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_fabric.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/fabric/fabric.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

struct DiscoverFabricVerticesFixture : public ::testing::Test {
    void SetUp() override {
        debugManager.flags.EnableWalkerPartition.set(-1);
        VariableBackup<bool> mockDeviceFlagBackup(&NEO::MockDevice::createSingleDevice, false);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        NEO::ExecutionEnvironment *executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }

        deviceFactory = std::make_unique<UltDeviceFactory>(numRootDevices, 0u, *executionEnvironment);

        for (auto i = 0u; i < executionEnvironment->rootDeviceEnvironments.size(); i++) {
            devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory->rootDevices[i]));
        }
        driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
        driverHandle->initialize(std::move(devices));

        for (auto &l0device : driverHandle->devices) {
            auto &rootDeviceEnvironment = *l0device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[l0device->getRootDeviceIndex()];
            if (!rootDeviceEnvironment.osInterface) {
                auto osIface = new NEO::OSInterface();
                auto drmMock = new DrmMockResources(rootDeviceEnvironment);
                drmMock->drmFabric = std::make_unique<NEO::DrmFabricStub>();
                rootDeviceEnvironment.osInterface.reset(osIface);
                rootDeviceEnvironment.osInterface->setDriverModel(std::unique_ptr<NEO::Drm>(drmMock));
            }
        }
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<Mock<L0::DriverHandle>> driverHandle;
    std::unique_ptr<UltDeviceFactory> deviceFactory;

    const uint32_t numRootDevices = 2u;
};

TEST_F(DiscoverFabricVerticesFixture, givenDeviceListWhenDiscoverFabricVerticesCalledThenOneVertexCreatedPerDevice) {
    std::vector<FabricVertex *> fabricVertices;

    FabricVertex::discoverFabricVertices(driverHandle->devices, fabricVertices);

    EXPECT_EQ(numRootDevices, fabricVertices.size());

    for (auto vertex : fabricVertices) {
        delete vertex;
    }
}

TEST_F(DiscoverFabricVerticesFixture, givenDeviceListWhenDiscoverFabricVerticesCalledThenEachDeviceHasFabricVertexSet) {
    std::vector<FabricVertex *> fabricVertices;

    FabricVertex::discoverFabricVertices(driverHandle->devices, fabricVertices);

    EXPECT_EQ(numRootDevices, fabricVertices.size());
    for (uint32_t i = 0u; i < numRootDevices; i++) {
        ze_fabric_vertex_handle_t hVertex = nullptr;
        EXPECT_EQ(ZE_RESULT_SUCCESS, driverHandle->devices[i]->getFabricVertex(&hVertex));
        EXPECT_NE(nullptr, hVertex);
    }

    for (auto vertex : fabricVertices) {
        delete vertex;
    }
}

} // namespace ult
} // namespace L0
