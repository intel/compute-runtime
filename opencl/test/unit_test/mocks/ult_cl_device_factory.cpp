/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"

#include "shared/source/command_stream/create_command_stream_impl.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/ult_device_factory.h"

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

UltClDeviceFactory::~UltClDeviceFactory() {
    for (auto &pClDevice : rootDevices) {
        pClDevice->decRefInternal();
    }
}
