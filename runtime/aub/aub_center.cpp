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
AubCenter::AubCenter(const HardwareInfo *pHwInfo, bool localMemoryEnabled) {
    if (DebugManager.flags.UseAubStream.get()) {
        std::string filename("aub.aub");
        aubManager.reset(AubDump::AubManager::create(pHwInfo->pPlatform->eRenderCoreFamily, 1, 1, localMemoryEnabled, filename));
    }
    addressMapper = std::make_unique<AddressMapper>();
    streamProvider = std::make_unique<AubFileStreamProvider>();
}
} // namespace OCLRT
