/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/linux/peer_access_probe.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/query_peer_access.h"

namespace NEO {

bool queryPeerAccess(Device &device, Device &peerDevice, GraphicsAllocation **probeAllocPtr, uint64_t *handle) {
    if (device.getRootDeviceEnvironment().osInterface) {
        DriverModelType driverType = device.getRootDeviceEnvironment().osInterface->getDriverModel()->getDriverModelType();
        if (driverType == DriverModelType::drm) {
            return queryPeerAccessDrm(device, peerDevice, probeAllocPtr, handle);
        }
    }
    return false;
}

} // namespace NEO
