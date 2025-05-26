/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_center.h"

#include "shared/source/aub/aub_helper.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
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
    if (debugManager.flags.UseAubStream.get()) {
        auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
        auto devicesCount = GfxCoreHelper::getSubDevicesCount(hwInfo);
        auto releaseHelper = rootDeviceEnvironment.getReleaseHelper();
        auto memoryBankSize = AubHelper::getPerTileLocalMemorySize(hwInfo, releaseHelper);
        CommandStreamReceiverType type = obtainCsrTypeFromIntegerValue(debugManager.flags.SetCommandStreamReceiver.get(), csrType);

        aubStreamMode = getAubStreamMode(aubFileName, type);

        auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
        const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

        auto aubStreamProductFamily = productHelper.getAubStreamProductFamily();

        stepping = productHelper.getAubStreamSteppingFromHwRevId(*hwInfo);

        auto gmmHelper = rootDeviceEnvironment.getGmmHelper();
        aub_stream::MMIOList extraMmioList = gfxCoreHelper.getExtraMmioList(*hwInfo, *gmmHelper);
        aub_stream::MMIOList debugMmioList = AubHelper::getAdditionalMmioList();

        extraMmioList.insert(extraMmioList.end(), debugMmioList.begin(), debugMmioList.end());

        aub_stream::injectMMIOListLegacy(extraMmioList);

        AubHelper::setTbxConfiguration();
        aub_stream::setAubStreamCaller(aub_stream::caller::neo);

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
    subCaptureCommon = std::make_unique<AubSubCaptureCommon>();
    if (debugManager.flags.AUBDumpSubCaptureMode.get()) {
        this->subCaptureCommon->subCaptureMode = static_cast<AubSubCaptureCommon::SubCaptureMode>(debugManager.flags.AUBDumpSubCaptureMode.get());
        this->subCaptureCommon->subCaptureFilter.dumpKernelStartIdx = static_cast<uint32_t>(debugManager.flags.AUBDumpFilterKernelStartIdx.get());
        this->subCaptureCommon->subCaptureFilter.dumpKernelEndIdx = static_cast<uint32_t>(debugManager.flags.AUBDumpFilterKernelEndIdx.get());
        this->subCaptureCommon->subCaptureFilter.dumpNamedKernelStartIdx = static_cast<uint32_t>(debugManager.flags.AUBDumpFilterNamedKernelStartIdx.get());
        this->subCaptureCommon->subCaptureFilter.dumpNamedKernelEndIdx = static_cast<uint32_t>(debugManager.flags.AUBDumpFilterNamedKernelEndIdx.get());
        if (debugManager.flags.AUBDumpFilterKernelName.get() != "unk") {
            this->subCaptureCommon->subCaptureFilter.dumpKernelName = debugManager.flags.AUBDumpFilterKernelName.get();
        }
    }
}

AubCenter::AubCenter() {
    subCaptureCommon = std::make_unique<AubSubCaptureCommon>();
}

uint32_t AubCenter::getAubStreamMode(const std::string &aubFileName, CommandStreamReceiverType csrType) {
    uint32_t mode = aub_stream::mode::aubFile;

    switch (csrType) {
    case CommandStreamReceiverType::hardwareWithAub:
    case CommandStreamReceiverType::aub:
        mode = aub_stream::mode::aubFile;
        break;
    case CommandStreamReceiverType::tbx:
        mode = aub_stream::mode::tbx;
        break;
    case CommandStreamReceiverType::tbxWithAub:
        mode = aub_stream::mode::aubFileAndTbx;
        break;
    case CommandStreamReceiverType::nullAub:
        mode = aub_stream::mode::null;
        break;
    default:
        break;
    }

    return mode;
}
} // namespace NEO
