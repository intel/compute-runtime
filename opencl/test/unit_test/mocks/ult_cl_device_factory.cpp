/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/unit_test/helpers/ult_hw_config.h"
#include "shared/test/unit_test/mocks/ult_device_factory.h"

#include "opencl/source/command_stream/create_command_stream_impl.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"

using namespace NEO;

UltClDeviceFactory::UltClDeviceFactory(uint32_t rootDevicesCount, uint32_t subDevicesCount) {

    auto executionEnvironment = new ClExecutionEnvironment();
    UltDeviceFactory::prepareDeviceEnvironments(*executionEnvironment, rootDevicesCount);

    pUltDeviceFactory = std::make_unique<UltDeviceFactory>(rootDevicesCount, subDevicesCount, *executionEnvironment);

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
