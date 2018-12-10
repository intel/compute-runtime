/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_center.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/debug_settings_manager.h"

namespace OCLRT {
AubCenter::AubCenter(const HardwareInfo *pHwInfo, bool localMemoryEnabled, const std::string &aubFileName) {
    if (DebugManager.flags.UseAubStream.get()) {
        aubManager.reset(createAubManager(pHwInfo->pPlatform->eRenderCoreFamily, 1, 1, localMemoryEnabled, pHwInfo->capabilityTable.aubDeviceId, aubFileName));
    }
    addressMapper = std::make_unique<AddressMapper>();
    streamProvider = std::make_unique<AubFileStreamProvider>();
}

AubCenter::AubCenter() {
    addressMapper = std::make_unique<AddressMapper>();
    streamProvider = std::make_unique<AubFileStreamProvider>();
}
} // namespace OCLRT
