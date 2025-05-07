/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/device/device.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/ze_intel_gpu.h"

uint32_t ZE_APICALL zerDeviceTranslateToIdentifier(ze_device_handle_t device) {
    if (!device) {
        auto driverHandle = static_cast<L0::DriverHandleImp *>(L0::globalDriverHandles->front());
        driverHandle->setErrorDescription("Invalid device handle");
        return std::numeric_limits<uint32_t>::max();
    }
    return L0::Device::fromHandle(device)->getIdentifier();
}

ze_device_handle_t ZE_APICALL zerIdentifierTranslateToDeviceHandle(uint32_t identifier) {
    auto driverHandle = static_cast<L0::DriverHandleImp *>(L0::globalDriverHandles->front());
    if (identifier >= driverHandle->devicesToExpose.size()) {
        driverHandle->setErrorDescription("Invalid device identifier");
        return nullptr;
    }
    return driverHandle->devicesToExpose[identifier];
}

ze_result_t ZE_APICALL zeDeviceSynchronize(ze_device_handle_t hDevice) {
    auto device = L0::Device::fromHandle(hDevice);
    for (auto &engine : device->getNEODevice()->getAllEngines()) {
        auto waitStatus = engine.commandStreamReceiver->waitForTaskCountWithKmdNotifyFallback(
            engine.commandStreamReceiver->peekTaskCount(),
            engine.commandStreamReceiver->obtainCurrentFlushStamp(),
            false,
            NEO::QueueThrottle::MEDIUM);
        if (waitStatus == NEO::WaitStatus::gpuHang) {
            return ZE_RESULT_ERROR_DEVICE_LOST;
        }
    }
    for (auto &secondaryCsr : device->getNEODevice()->getSecondaryCsrs()) {
        auto waitStatus = secondaryCsr->waitForTaskCountWithKmdNotifyFallback(
            secondaryCsr->peekTaskCount(),
            secondaryCsr->obtainCurrentFlushStamp(),
            false,
            NEO::QueueThrottle::MEDIUM);
        if (waitStatus == NEO::WaitStatus::gpuHang) {
            return ZE_RESULT_ERROR_DEVICE_LOST;
        }
    }

    return ZE_RESULT_SUCCESS;
}