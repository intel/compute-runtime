/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/windows/gmm_callbacks.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "gmm_client_context.h"
#include "gmm_memory.h"

namespace NEO {

bool Wddm::configureDeviceAddressSpace() {
    GMM_DEVICE_CALLBACKS_INT deviceCallbacks{};
    GMM_DEVICE_INFO deviceInfo{};
    deviceInfo.pDeviceCb = &deviceCallbacks;
    if (!gmmMemory->setDeviceInfo(&deviceInfo)) {
        return false;
    }

    maximumApplicationAddress = MemoryConstants::max64BitAppAddress;
    auto productFamily = gfxPlatform->eProductFamily;
    if (!hardwareInfoTable[productFamily]) {
        return false;
    }
    auto svmSize = hardwareInfoTable[productFamily]->capabilityTable.gpuAddressSpace >= MemoryConstants::max64BitAppAddress
                       ? maximumApplicationAddress + 1u
                       : 0u;

    bool obtainMinAddress = rootDeviceEnvironment.getHardwareInfo()->platform.eRenderCoreFamily == IGFX_GEN12LP_CORE;
    return gmmMemory->configureDevice(getAdapter(), device, getGdi()->escape, svmSize, featureTable->ftrL3IACoherency, minAddress, obtainMinAddress);
}

} // namespace NEO
