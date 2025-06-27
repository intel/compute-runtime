/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"

#include "shared/test/common/mocks/ult_device_factory.h"

#include "opencl/source/execution_environment/cl_execution_environment.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

using namespace NEO;

UltClDeviceFactory::UltClDeviceFactory(uint32_t rootDevicesCount, uint32_t subDevicesCount) {
    pUltDeviceFactory = std::make_unique<UltDeviceFactory>(rootDevicesCount, subDevicesCount, *(new ClExecutionEnvironment));

    for (auto &pRootDevice : pUltDeviceFactory->rootDevices) {
        auto pRootClDevice = new MockClDevice{pRootDevice};
        for (auto &pClSubDevice : pRootClDevice->subDevices) {
            subDevices.push_back(pClSubDevice.get());
        }
        rootDevices.push_back(pRootClDevice);
    }
}

UltClDeviceFactory::UltClDeviceFactory(uint32_t rootDevicesCount, uint32_t subDevicesCount, ClExecutionEnvironment *clExecutionEnvironment) {
    pUltDeviceFactory = std::make_unique<UltDeviceFactory>(rootDevicesCount, subDevicesCount, *clExecutionEnvironment);

    for (auto &pRootDevice : pUltDeviceFactory->rootDevices) {
        auto pRootClDevice = new MockClDevice{pRootDevice};
        for (auto &pClSubDevice : pRootClDevice->subDevices) {
            subDevices.push_back(pClSubDevice.get());
        }
        rootDevices.push_back(pRootClDevice);
    }
}

UltClDeviceFactory::~UltClDeviceFactory() {
    for (auto &pClDevice : rootDevices) {
        pClDevice->decRefInternal();
    }
}
