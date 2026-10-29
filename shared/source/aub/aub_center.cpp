/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_center.h"

#include "shared/source/aub/aub_helper.h"
#include "shared/source/aub/aub_kernel_info_helper.h"
#include "shared/source/aub/tbx_in_process_simulator.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/constants.h"
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
        const auto &releaseHelper = rootDeviceEnvironment.getReleaseHelper();
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

        AubHelper::setTbxConfiguration(rootDeviceEnvironment.getRootDeviceIndex());
        aub_stream::setAubStreamCaller(aub_stream::caller::neo);

        aub_stream::AubManagerOptions options{};
        options.version = 1u;
        options.productFamily = static_cast<uint32_t>(aubStreamProductFamily.value_or(aub_stream::ProductFamily::MaxProduct));
        options.devicesCount = devicesCount;
        options.memoryBankSize = memoryBankSize;
        options.stepping = stepping;
        options.localMemorySupported = localMemoryEnabled;
        const bool inProcessSimulatorRequested = debugManager.flags.TbxInProcessSimulatorLibrary.get() != "unk";
        auto sharedMemoryAubStreamMode = getSharedMemoryAubStreamMode(aubStreamMode, inProcessSimulatorRequested);
        if (sharedMemoryAubStreamMode != aubStreamMode && enableTbxSharedMemory(rootDeviceEnvironment, inProcessSimulatorRequested, options)) {
            aubStreamMode = sharedMemoryAubStreamMode;
        }
        options.mode = aubStreamMode;
        options.gpuAddressSpace = hwInfo->capabilityTable.gpuAddressSpace;
        options.programAdditionalEngineMMIO = productHelper.isProgramAdditionalEngineMMIORequired(*hwInfo);

        aubManager.reset(createAubManager(options));
        aubManager->setCCSMode(hwInfo->gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
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

AubCenter::~AubCenter() = default;

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

uint32_t AubCenter::getSharedMemoryAubStreamMode(uint32_t aubStreamMode, bool translationTableMode) {
    switch (aubStreamMode) {
    case aub_stream::mode::tbx:
        return translationTableMode ? aub_stream::mode::tbxShm4 : aub_stream::mode::tbxShm;
    case aub_stream::mode::aubFileAndTbx:
        return translationTableMode ? aub_stream::mode::aubFileAndShm4 : aub_stream::mode::aubFileAndShm;
    default:
        return aubStreamMode;
    }
}

bool AubCenter::isAllocationMemorySharedWithSimulator() const {
    return this->aubStreamMode == aub_stream::mode::tbxShm4;
}

bool AubCenter::enableTbxSharedMemory(const RootDeviceEnvironment &rootDeviceEnvironment, bool inProcessSimulatorRequested, aub_stream::AubManagerOptions &options) {
    const auto &systemMemoryFileSetting = debugManager.flags.TbxSharedSystemMemoryFile.get();
    if (systemMemoryFileSetting == "unk") {
        PRINT_STRING(inProcessSimulatorRequested && debugManager.flags.PrintDebugMessages.get(), stderr, "TbxInProcessSimulatorLibrary requires TbxSharedSystemMemoryFile, falling back to TBX socket mode\n");
        return false;
    }
    const auto rootDeviceIndex = rootDeviceEnvironment.getRootDeviceIndex();
    const auto systemMemoryFile = AubHelper::getTbxSharedMemoryFileName(systemMemoryFileSetting, rootDeviceIndex);
    const auto &localMemoryFileSetting = debugManager.flags.TbxSharedLocalMemoryFile.get();
    const auto localMemoryFile = localMemoryFileSetting == "unk" ? std::string() : AubHelper::getTbxSharedMemoryFileName(localMemoryFileSetting, rootDeviceIndex);
    if (!mapTbxSharedMemory(systemMemoryFile, localMemoryFile, options.sharedMemoryInfo) || !isTbxSharedMemorySizeSufficient(inProcessSimulatorRequested, options)) {
        unmapTbxSharedMemory(options.sharedMemoryInfo);
        return false;
    }
    if (!inProcessSimulatorRequested) {
        return true;
    }

    const auto &simulatorLibrary = debugManager.flags.TbxInProcessSimulatorLibrary.get();
    const auto &simulatorArgumentsSetting = debugManager.flags.TbxInProcessSimulatorArguments.get();
    auto simulatorArguments = TbxInProcessSimulator::splitArguments(simulatorArgumentsSetting == "unk" ? std::string() : simulatorArgumentsSetting);
    if (rootDeviceIndex > 0) {
        std::vector<std::string> sharedMemoryFiles;
        if (!localMemoryFile.empty()) {
            sharedMemoryFiles.push_back(localMemoryFile);
        }
        sharedMemoryFiles.push_back(systemMemoryFile);
        TbxInProcessSimulator::replaceRootDeviceArguments(simulatorArguments, AubHelper::getTbxPort(rootDeviceIndex), sharedMemoryFiles);
    }
    this->inProcessSimulator = TbxInProcessSimulator::start(simulatorLibrary, simulatorArguments);
    if (this->inProcessSimulator) {
        return true;
    }

    PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "Failed to start TBX in-process simulator: %s, falling back to TBX socket mode\n", simulatorLibrary.c_str());
    unmapTbxSharedMemory(options.sharedMemoryInfo);
    return false;
}

bool AubCenter::mapTbxSharedMemory(const std::string &systemMemoryFile, const std::string &localMemoryFile, aub_stream::SharedMemoryInfo &sharedMemoryInfo) {
    this->sharedSystemMemory = MappedFile::open(systemMemoryFile);
    if (!this->sharedSystemMemory) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "Failed to map TBX shared system memory file: %s, falling back to TBX socket mode\n", systemMemoryFile.c_str());
        return false;
    }
    sharedMemoryInfo.sysMemBase = this->sharedSystemMemory->getBase();
    sharedMemoryInfo.sysMemSize = this->sharedSystemMemory->getSize();

    if (!localMemoryFile.empty()) {
        this->sharedLocalMemory = MappedFile::open(localMemoryFile);
        if (!this->sharedLocalMemory) {
            PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "Failed to map TBX shared local memory file: %s, falling back to TBX socket mode\n", localMemoryFile.c_str());
            return false;
        }
        sharedMemoryInfo.localMemBase = this->sharedLocalMemory->getBase();
        sharedMemoryInfo.localMemSize = this->sharedLocalMemory->getSize();
    }
    return true;
}

bool AubCenter::isTbxSharedMemorySizeSufficient(bool translationTableMode, const aub_stream::AubManagerOptions &options) const {
    const uint64_t physicalMemorySize = static_cast<uint64_t>(options.devicesCount) * options.memoryBankSize;
    const uint64_t requiredFileSize = translationTableMode ? physicalMemorySize / MemoryConstants::pageSize * sizeof(uint64_t) : physicalMemorySize;

    if (options.localMemorySupported) {
        if (!this->sharedLocalMemory) {
            PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "TbxSharedLocalMemoryFile is required with local memory, falling back to TBX socket mode\n");
            return false;
        }
        if (this->sharedLocalMemory->getSize() < requiredFileSize) {
            PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "TBX shared local memory file is too small: %llu bytes, %llu required, falling back to TBX socket mode\n",
                         static_cast<unsigned long long>(this->sharedLocalMemory->getSize()), static_cast<unsigned long long>(requiredFileSize));
            return false;
        }
        return true;
    }

    if (this->sharedSystemMemory->getSize() < requiredFileSize) {
        PRINT_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "TBX shared system memory file is too small: %llu bytes, %llu required, falling back to TBX socket mode\n",
                     static_cast<unsigned long long>(this->sharedSystemMemory->getSize()), static_cast<unsigned long long>(requiredFileSize));
        return false;
    }
    return true;
}

void AubCenter::unmapTbxSharedMemory(aub_stream::SharedMemoryInfo &sharedMemoryInfo) {
    sharedMemoryInfo = {};
    this->sharedLocalMemory.reset();
    this->sharedSystemMemory.reset();
}

void AubCenter::addImplicitArgsInfoToAubComments(uint32_t implicitArgsVersion) {

    std::call_once(addImplicitArgsInfoOnceFlag, [implicitArgsVersion, this]() {
        aubManager->addComment(NEO::AubComment::printImplicitArgsLayouts(implicitArgsVersion).c_str());
    });
}
} // namespace NEO
