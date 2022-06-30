/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/device_command_stream.inl"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/device_command_stream.inl"

namespace NEO {

template <typename GfxFamily>
CommandStreamReceiver *createCommandStreamReceiver(bool withAubDump,
                                                   ExecutionEnvironment &executionEnvironment,
                                                   uint32_t rootDeviceIndex,
                                                   const DeviceBitfield deviceBitfield) {
    auto rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex].get();
    if (rootDeviceEnvironment->osInterface->getDriverModel()->getDriverModelType() == DriverModelType::DRM) {
        return createDrmCommandStreamReceiver<GfxFamily>(withAubDump, executionEnvironment, rootDeviceIndex, deviceBitfield);
    } else {
        return createWddmCommandStreamReceiver<GfxFamily>(withAubDump, executionEnvironment, rootDeviceIndex, deviceBitfield);
    }
}
} // namespace NEO
