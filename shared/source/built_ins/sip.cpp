/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/io_functions.h"

#include "common/StateSaveAreaHeader.h"

namespace NEO {

SipClassType SipKernel::classType = SipClassType::Init;

std::vector<char> readFile(const std::string &fileName, size_t &retSize) {
    std::vector<char> retBuf;
    FILE *fileDescriptor = nullptr;
    long int size = 0;
    size_t bytesRead = 0u;
    retSize = 0;

    fileDescriptor = IoFunctions::fopenPtr(fileName.c_str(), "rb");
    if (fileDescriptor == NULL) {
        return retBuf;
    }

    IoFunctions::fseekPtr(fileDescriptor, 0, SEEK_END);
    size = IoFunctions::ftellPtr(fileDescriptor);
    IoFunctions::rewindPtr(fileDescriptor);

    retBuf.resize(size);

    bytesRead = IoFunctions::freadPtr(retBuf.data(), 1, size, fileDescriptor);
    IoFunctions::fclosePtr(fileDescriptor);

    if (static_cast<long int>(bytesRead) != size || bytesRead == 0u) {
        retBuf.clear();
    } else {
        retSize = bytesRead;
    }

    return retBuf;
}

SipKernel::~SipKernel() = default;

SipKernel::SipKernel(SipKernelType type, GraphicsAllocation *sipAlloc, std::vector<char> ssah) : stateSaveAreaHeader(std::move(ssah)), sipAllocation(sipAlloc), type(type) {
}

GraphicsAllocation *SipKernel::getSipAllocation() const {
    return sipAllocation;
}

const std::vector<char> &SipKernel::getStateSaveAreaHeader() const {
    return stateSaveAreaHeader;
}

size_t SipKernel::getStateSaveAreaSize(Device *device) const {
    auto &hwInfo = device->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto maxDbgSurfaceSize = hwHelper.getSipKernelMaxDbgSurfaceSize(hwInfo);
    const auto &stateSaveAreaHeader = getStateSaveAreaHeader();
    if (stateSaveAreaHeader.empty()) {
        return maxDbgSurfaceSize;
    }

    if (strcmp(stateSaveAreaHeader.data(), "tssarea")) {
        return maxDbgSurfaceSize;
    }

    auto hdr = reinterpret_cast<const SIP::StateSaveAreaHeader *>(stateSaveAreaHeader.data());
    DEBUG_BREAK_IF(hdr->versionHeader.size * 8 != sizeof(SIP::StateSaveAreaHeader));

    auto stateSaveAreaSize = hdr->regHeader.num_slices *
                                 hdr->regHeader.num_subslices_per_slice *
                                 hdr->regHeader.num_eus_per_subslice *
                                 hdr->regHeader.num_threads_per_eu *
                                 hdr->regHeader.state_save_size +
                             hdr->versionHeader.size * 8 + hdr->regHeader.state_area_offset;
    return alignUp(stateSaveAreaSize, MemoryConstants::pageSize);
}

SipKernelType SipKernel::getSipKernelType(Device &device) {
    if (device.getDebugger() != nullptr && !device.getDebugger()->isLegacy()) {
        return SipKernelType::DbgBindless;
    }
    bool debuggingEnabled = device.getDebugger() != nullptr || device.isDebuggerActive();
    return getSipKernelType(device, debuggingEnabled);
}

SipKernelType SipKernel::getSipKernelType(Device &device, bool debuggingEnabled) {
    auto &hwHelper = HwHelper::get(device.getHardwareInfo().platform.eRenderCoreFamily);
    return hwHelper.getSipKernelType(debuggingEnabled);
}

bool SipKernel::initBuiltinsSipKernel(SipKernelType type, Device &device) {
    device.getBuiltIns()->getSipKernel(type, device);
    return true;
}

bool SipKernel::initRawBinaryFromFileKernel(SipKernelType type, Device &device, std::string &fileName) {
    uint32_t sipIndex = static_cast<uint32_t>(type);
    uint32_t rootDeviceIndex = device.getRootDeviceIndex();

    if (device.getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->sipKernels[sipIndex].get() != nullptr) {
        return true;
    }

    size_t bytesRead = 0u;
    auto fileData = readFile(fileName, bytesRead);

    if (bytesRead) {
        void *alignedBuffer = alignedMalloc(bytesRead, MemoryConstants::pageSize);
        memcpy_s(alignedBuffer, bytesRead, fileData.data(), bytesRead);

        const auto allocType = AllocationType::KERNEL_ISA_INTERNAL;
        AllocationProperties properties = {rootDeviceIndex, bytesRead, allocType, device.getDeviceBitfield()};
        properties.flags.use32BitFrontWindow = false;

        auto sipAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
        if (sipAllocation == nullptr) {
            alignedFree(alignedBuffer);
            return false;
        }

        auto &hwInfo = device.getHardwareInfo();
        auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

        MemoryTransferHelper::transferMemoryToAllocation(hwInfoConfig.isBlitCopyRequiredForLocalMemory(hwInfo, *sipAllocation),
                                                         device, sipAllocation, 0, alignedBuffer,
                                                         bytesRead);

        alignedFree(alignedBuffer);

        auto headerFilename = createHeaderFilename(fileName);
        std::vector<char> stateSaveAreaHeader = readStateSaveAreaHeaderFromFile(headerFilename);

        device.getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->sipKernels[sipIndex] =
            std::make_unique<SipKernel>(type, sipAllocation, std::move(stateSaveAreaHeader));
        return true;
    }
    return false;
}

std::vector<char> SipKernel::readStateSaveAreaHeaderFromFile(const std::string &fileName) {
    std::vector<char> data;
    size_t bytesRead = 0u;
    data = readFile(fileName, bytesRead);
    return data;
}

std::string SipKernel::createHeaderFilename(const std::string &fileName) {
    std::string_view coreName(fileName);
    auto extensionPos = coreName.find('.');
    std::string ext = "";

    if (extensionPos != coreName.npos) {
        ext = coreName.substr(extensionPos, coreName.size() - extensionPos);
        coreName.remove_suffix(coreName.size() - extensionPos);
    }

    std::string headerFilename(coreName);
    headerFilename += "_header" + ext;
    return headerFilename;
}

bool SipKernel::initHexadecimalArraySipKernel(SipKernelType type, Device &device) {
    uint32_t sipIndex = static_cast<uint32_t>(type);
    uint32_t rootDeviceIndex = device.getRootDeviceIndex();
    auto sipKenel = device.getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->sipKernels[sipIndex].get();
    if (sipKenel != nullptr) {
        return true;
    }

    uint32_t *sipKernelBinary = nullptr;
    size_t kernelBinarySize = 0u;
    auto &hwInfo = device.getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    hwHelper.setSipKernelData(sipKernelBinary, kernelBinarySize);
    const auto allocType = AllocationType::KERNEL_ISA_INTERNAL;
    AllocationProperties properties = {rootDeviceIndex, kernelBinarySize, allocType, device.getDeviceBitfield()};
    properties.flags.use32BitFrontWindow = false;

    auto sipAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    if (sipAllocation == nullptr) {
        return false;
    }
    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    MemoryTransferHelper::transferMemoryToAllocation(hwInfoConfig.isBlitCopyRequiredForLocalMemory(hwInfo, *sipAllocation),
                                                     device, sipAllocation, 0, sipKernelBinary,
                                                     kernelBinarySize);

    std::vector<char> emptyStateSaveAreaHeader;
    device.getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->sipKernels[sipIndex] =
        std::make_unique<SipKernel>(type, sipAllocation, std::move(emptyStateSaveAreaHeader));

    return true;
}

void SipKernel::freeSipKernels(RootDeviceEnvironment *rootDeviceEnvironment, MemoryManager *memoryManager) {
    for (auto &sipKernel : rootDeviceEnvironment->sipKernels) {
        if (sipKernel.get()) {
            memoryManager->freeGraphicsMemory(sipKernel->getSipAllocation());
        }
    }
}

void SipKernel::selectSipClassType(std::string &fileName, const HardwareInfo &hwInfo) {
    const std::string unknown("unk");
    if (fileName.compare(unknown) == 0) {
        SipKernel::classType = HwHelper::get(hwInfo.platform.eRenderCoreFamily).isSipKernelAsHexadecimalArrayPreferred()
                                   ? SipClassType::HexadecimalHeaderFile
                                   : SipClassType::Builtins;
    } else {
        SipKernel::classType = SipClassType::RawBinaryFromFile;
    }
}

bool SipKernel::initSipKernelImpl(SipKernelType type, Device &device) {
    std::string fileName = DebugManager.flags.LoadBinarySipFromFile.get();
    SipKernel::selectSipClassType(fileName, *device.getRootDeviceEnvironment().getHardwareInfo());

    switch (SipKernel::classType) {
    case SipClassType::RawBinaryFromFile:
        return SipKernel::initRawBinaryFromFileKernel(type, device, fileName);
    case SipClassType::HexadecimalHeaderFile:
        return SipKernel::initHexadecimalArraySipKernel(type, device);
    default:
        return SipKernel::initBuiltinsSipKernel(type, device);
    }
}

const SipKernel &SipKernel::getSipKernelImpl(Device &device) {
    auto sipType = SipKernel::getSipKernelType(device);

    switch (SipKernel::classType) {
    case SipClassType::RawBinaryFromFile:
    case SipClassType::HexadecimalHeaderFile:
        return *device.getRootDeviceEnvironment().sipKernels[static_cast<uint32_t>(sipType)].get();
    default:
        return device.getBuiltIns()->getSipKernel(sipType, device);
    }
}

const SipKernel &SipKernel::getBindlessDebugSipKernel(Device &device) {
    auto debugSipType = SipKernelType::DbgBindless;
    SipKernel::initSipKernelImpl(debugSipType, device);

    switch (SipKernel::classType) {
    case SipClassType::RawBinaryFromFile:
        return *device.getRootDeviceEnvironment().sipKernels[static_cast<uint32_t>(debugSipType)].get();
    default:
        return device.getBuiltIns()->getSipKernel(debugSipType, device);
    }
}

} // namespace NEO
