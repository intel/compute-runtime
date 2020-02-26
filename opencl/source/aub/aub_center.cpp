/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/aub/aub_center.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"

#include "opencl/source/aub/aub_helper.h"

#include "third_party/aub_stream/headers/aub_manager.h"
#include "third_party/aub_stream/headers/aubstream.h"

namespace NEO {
extern aub_stream::AubManager *createAubManager(uint32_t productFamily, uint32_t devicesCount, uint64_t memoryBankSize, bool localMemorySupported, uint32_t streamMode, uint64_t gpuAddressSpace);

AubCenter::AubCenter(const HardwareInfo *pHwInfo, bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) {
    if (DebugManager.flags.UseAubStream.get()) {
        auto devicesCount = HwHelper::getSubDevicesCount(pHwInfo);
        auto memoryBankSize = AubHelper::getMemBankSize(pHwInfo);
        CommandStreamReceiverType type = csrType;
        if (DebugManager.flags.SetCommandStreamReceiver.get() >= CommandStreamReceiverType::CSR_HW) {
            type = static_cast<CommandStreamReceiverType>(DebugManager.flags.SetCommandStreamReceiver.get());
        }

        aubStreamMode = getAubStreamMode(aubFileName, type);

        AubHelper::setAdditionalMmioList();
        if (DebugManager.flags.AubDumpAddMmioRegistersList.get() != "unk") {
            aub_stream::injectMMIOList(AubHelper::getAdditionalMmioList());
        }
        aub_stream::setTbxServerIp(DebugManager.flags.TbxServer.get());
        aub_stream::setTbxServerPort(DebugManager.flags.TbxPort.get());

        aubManager.reset(createAubManager(pHwInfo->platform.eProductFamily, devicesCount, memoryBankSize, localMemoryEnabled, aubStreamMode, pHwInfo->capabilityTable.gpuAddressSpace));
    }
    addressMapper = std::make_unique<AddressMapper>();
    streamProvider = std::make_unique<AubFileStreamProvider>();
    subCaptureCommon = std::make_unique<AubSubCaptureCommon>();
    if (DebugManager.flags.AUBDumpSubCaptureMode.get()) {
        this->subCaptureCommon->subCaptureMode = static_cast<AubSubCaptureCommon::SubCaptureMode>(DebugManager.flags.AUBDumpSubCaptureMode.get());
        this->subCaptureCommon->subCaptureFilter.dumpKernelStartIdx = static_cast<uint32_t>(DebugManager.flags.AUBDumpFilterKernelStartIdx.get());
        this->subCaptureCommon->subCaptureFilter.dumpKernelEndIdx = static_cast<uint32_t>(DebugManager.flags.AUBDumpFilterKernelEndIdx.get());
        this->subCaptureCommon->subCaptureFilter.dumpNamedKernelStartIdx = static_cast<uint32_t>(DebugManager.flags.AUBDumpFilterNamedKernelStartIdx.get());
        this->subCaptureCommon->subCaptureFilter.dumpNamedKernelEndIdx = static_cast<uint32_t>(DebugManager.flags.AUBDumpFilterNamedKernelEndIdx.get());
        if (DebugManager.flags.AUBDumpFilterKernelName.get() != "unk") {
            this->subCaptureCommon->subCaptureFilter.dumpKernelName = DebugManager.flags.AUBDumpFilterKernelName.get();
        }
    }
}

AubCenter::AubCenter() {
    addressMapper = std::make_unique<AddressMapper>();
    streamProvider = std::make_unique<AubFileStreamProvider>();
    subCaptureCommon = std::make_unique<AubSubCaptureCommon>();
}

AubCenter::~AubCenter() {
    if (DebugManager.flags.UseAubStream.get()) {
        aub_stream::injectMMIOList(MMIOList{});
    }
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
} // namespace NEO
