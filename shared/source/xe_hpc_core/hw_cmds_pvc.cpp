/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"

#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"

#include <algorithm>

namespace NEO {

bool PVC::isXl(const HardwareInfo &hwInfo) {
    auto it = std::find(pvcXlDeviceIds.begin(), pvcXlDeviceIds.end(), hwInfo.platform.usDeviceID);
    return it != pvcXlDeviceIds.end();
}

bool PVC::isXt(const HardwareInfo &hwInfo) {
    auto it = std::find(pvcXtDeviceIds.begin(), pvcXtDeviceIds.end(), hwInfo.platform.usDeviceID);
    return it != pvcXtDeviceIds.end();
}

bool PVC::isXtVg(const HardwareInfo &hwInfo) {
    auto it = std::find(pvcXtVgDeviceIds.begin(), pvcXtVgDeviceIds.end(), hwInfo.platform.usDeviceID);
    return it != pvcXtVgDeviceIds.end();
}

} // namespace NEO
