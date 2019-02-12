/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_center.h"
#include "runtime/aub/aub_helper.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/debug_settings_manager.h"

#include "third_party/aub_stream/headers/aub_manager.h"
#include "third_party/aub_stream/headers/options.h"
#include "third_party/aub_stream/headers/modes.h"

namespace OCLRT {
extern aub_stream::AubManager *createAubManager(uint32_t productFamily, uint32_t devicesCount, uint64_t memoryBankSize, bool localMemorySupported, const std::string &aubFileName, uint32_t streamMode);

AubCenter::AubCenter(const HardwareInfo *pHwInfo, bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) {
    if (DebugManager.flags.UseAubStream.get()) {
        auto devicesCount = AubHelper::getDevicesCount(pHwInfo);
        auto memoryBankSize = AubHelper::getMemBankSize(pHwInfo);
        CommandStreamReceiverType type = static_cast<CommandStreamReceiverType>(DebugManager.flags.SetCommandStreamReceiver.get() != CommandStreamReceiverType::CSR_HW
                                                                                    ? DebugManager.flags.SetCommandStreamReceiver.get()
                                                                                    : csrType);
        aubStreamMode = getAubStreamMode(aubFileName, type);

        if (DebugManager.flags.AubDumpAddMmioRegistersList.get() != "unk") {
            aub_stream::injectMMIOList = AubHelper::getAdditionalMmioList();
        }
        aub_stream::tbxServerIp = DebugManager.flags.TbxServer.get();
        aub_stream::tbxServerPort = DebugManager.flags.TbxPort.get();

        aubManager.reset(createAubManager(pHwInfo->pPlatform->eProductFamily, devicesCount, memoryBankSize, localMemoryEnabled, aubFileName, aubStreamMode));
    }
    addressMapper = std::make_unique<AddressMapper>();
    streamProvider = std::make_unique<AubFileStreamProvider>();
}

AubCenter::AubCenter() {
    addressMapper = std::make_unique<AddressMapper>();
    streamProvider = std::make_unique<AubFileStreamProvider>();
}

uint32_t AubCenter::getAubStreamMode(const std::string &aubFileName, uint32_t csrType) {
    uint32_t mode = aub_stream::mode::aubFile;

    switch (csrType) {
    case CommandStreamReceiverType::CSR_HW_WITH_AUB:
    case CommandStreamReceiverType::CSR_AUB:
        mode = aub_stream::mode::aubFile;
        break;
    case CommandStreamReceiverType::CSR_TBX:
        mode = aub_stream::mode::tbx;
        break;
    case CommandStreamReceiverType::CSR_TBX_WITH_AUB:
        mode = aub_stream::mode::aubFileAndTbx;
        break;
    default:
        break;
    }

    return mode;
}
} // namespace OCLRT
