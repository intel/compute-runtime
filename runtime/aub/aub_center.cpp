/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_center.h"
#include "runtime/aub/aub_helper.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "third_party/aub_stream/headers/options.h"
#include "third_party/aub_stream/headers/aub_manager.h"

namespace OCLRT {
extern aub_stream::AubManager *createAubManager(uint32_t gfxFamily, uint32_t devicesCount, uint64_t memoryBankSize, bool localMemorySupported, const std::string &aubFileName);

AubCenter::AubCenter(const HardwareInfo *pHwInfo, bool localMemoryEnabled, const std::string &aubFileName) {
    if (DebugManager.flags.UseAubStream.get()) {
        auto devicesCount = AubHelper::getDevicesCount(pHwInfo);
        auto memoryBankSize = AubHelper::getMemBankSize();
        if (DebugManager.flags.AubDumpAddMmioRegistersList.get() != "unk") {
            aub_stream::injectMMIOList = AubHelper::getAdditionalMmioList();
        }

        aubManager.reset(createAubManager(pHwInfo->pPlatform->eRenderCoreFamily, devicesCount, memoryBankSize, localMemoryEnabled, aubFileName));
    }
    addressMapper = std::make_unique<AddressMapper>();
    streamProvider = std::make_unique<AubFileStreamProvider>();
}

AubCenter::AubCenter() {
    addressMapper = std::make_unique<AddressMapper>();
    streamProvider = std::make_unique<AubFileStreamProvider>();
}
} // namespace OCLRT
