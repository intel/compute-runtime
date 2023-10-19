/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_center.h"

#include "shared/source/aub/aub_helper.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"

#include "aubstream/aub_manager.h"
#include "aubstream/aubstream.h"
#include "aubstream/product_family.h"

namespace NEO {
extern aub_stream::AubManager *createAubManager(const aub_stream::AubManagerOptions &options);

AubCenter::AubCenter(const RootDeviceEnvironment &rootDeviceEnvironment, bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType) {
    if (DebugManager.flags.UseAubStream.get()) {
        auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
        bool subDevicesAsDevices = rootDeviceEnvironment.executionEnvironment.isExposingSubDevicesAsDevices();
        auto devicesCount = GfxCoreHelper::getSubDevicesCount(subDevicesAsDevices, hwInfo);
        auto memoryBankSize = AubHelper::getPerTileLocalMemorySize(subDevicesAsDevices, hwInfo);
        CommandStreamReceiverType type = csrType;
        if (DebugManager.flags.SetCommandStreamReceiver.get() >= CommandStreamReceiverType::CSR_HW) {
            type = static_cast<CommandStreamReceiverType>(DebugManager.flags.SetCommandStreamReceiver.get());
        }

        aubStreamMode = getAubStreamMode(aubFileName, type);

        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
        const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

        auto aubStreamProductFamily = productHelper.getAubStreamProductFamily();

        stepping = productHelper.getAubStreamSteppingFromHwRevId(*hwInfo);

        auto gmmHelper = rootDeviceEnvironment.getGmmHelper();
        aub_stream::MMIOList extraMmioList = gfxCoreHelper.getExtraMmioList(*hwInfo, *gmmHelper);
        aub_stream::MMIOList debugMmioList = AubHelper::getAdditionalMmioList();

        extraMmioList.insert(extraMmioList.end(), debugMmioList.begin(), debugMmioList.end());

        aub_stream::injectMMIOList(extraMmioList);

        AubHelper::setTbxConfiguration();

        aub_stream::AubManagerOptions options{};
        options.version = 1u;
        options.productFamily = static_cast<uint32_t>(aubStreamProductFamily.value_or(aub_stream::ProductFamily::MaxProduct));
        options.devicesCount = devicesCount;
        options.memoryBankSize = memoryBankSize;
        options.stepping = stepping;
        options.localMemorySupported = localMemoryEnabled;
        options.mode = aubStreamMode;
        options.gpuAddressSpace = hwInfo->capabilityTable.gpuAddressSpace;

        aubManager.reset(createAubManager(options));
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
