/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debugger/debugger.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/sip_external_lib/sip_external_lib.h"
#include "shared/source/utilities/io_functions.h"

#include "common/StateSaveAreaHeader.h"

namespace NEO {

SipClassType SipKernel::classType = SipClassType::init;

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
    UNRECOVERABLE_IF(size == -1);
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

SipKernel::SipKernel(SipKernelType type, GraphicsAllocation *sipAlloc, std::vector<char> ssah, std::vector<char> binary) : stateSaveAreaHeader(std::move(ssah)), binary(std::move(binary)), sipAllocation(sipAlloc), type(type) {
}

GraphicsAllocation *SipKernel::getSipAllocation() const {
    return sipAllocation;
}

const std::vector<char> &SipKernel::getStateSaveAreaHeader() const {
    return stateSaveAreaHeader;
}

const std::vector<char> &SipKernel::getBinary() const {
    return binary;
}

size_t SipKernel::getStateSaveAreaSize(Device *device) const {
    auto &hwInfo = device->getHardwareInfo();
    const auto &stateSaveAreaHeader = getStateSaveAreaHeader();
    if (stateSaveAreaHeader.empty()) {
        return 0u;
    }

    if (strcmp(stateSaveAreaHeader.data(), "tssarea")) {
        return 0u;
    }

    auto hdr = reinterpret_cast<const NEO::StateSaveAreaHeader *>(stateSaveAreaHeader.data());

    auto numSlices = std::max(hwInfo.gtSystemInfo.MaxSlicesSupported, NEO::GfxCoreHelper::getHighestEnabledSlice(hwInfo));
    size_t stateSaveAreaSize = 0;
    if (hdr->versionHeader.version.major == 4) {
        if (debugManager.flags.ForceTotalWMTPDataSize.get() > -1) {
            stateSaveAreaSize = static_cast<size_t>(debugManager.flags.ForceTotalWMTPDataSize.get());
        } else {
            stateSaveAreaSize = static_cast<size_t>(hdr->totalWmtpDataSize);
        }
    } else if (hdr->versionHeader.version.major == 3) {
        stateSaveAreaSize = numSlices *
                                hdr->regHeaderV3.num_subslices_per_slice *
                                hdr->regHeaderV3.num_eus_per_subslice *
                                hdr->regHeaderV3.num_threads_per_eu *
                                hdr->regHeaderV3.state_save_size +
                            hdr->versionHeader.size * 8 + hdr->regHeaderV3.state_area_offset;
        stateSaveAreaSize += hdr->regHeaderV3.fifo_size * sizeof(SIP::fifo_node);

    } else if (hdr->versionHeader.version.major < 3) {
        stateSaveAreaSize = numSlices *
                                hdr->regHeader.num_subslices_per_slice *
                                hdr->regHeader.num_eus_per_subslice *
                                hdr->regHeader.num_threads_per_eu *
                                hdr->regHeader.state_save_size +
                            hdr->versionHeader.size * 8 + hdr->regHeader.state_area_offset;
    }
    return alignUp(stateSaveAreaSize, MemoryConstants::pageSize);
}

SipKernelType SipKernel::getSipKernelType(Device &device) {
    if (device.getDebugger() != nullptr) {
        auto &compilerProductHelper = device.getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
        if (compilerProductHelper.isHeaplessModeEnabled(device.getHardwareInfo())) {
            return SipKernelType::dbgHeapless;
        } else {
            return SipKernelType::dbgBindless;
        }
    }
    bool debuggingEnabled = device.getDebugger() != nullptr;
    return getSipKernelType(device, debuggingEnabled);
}

SipKernelType SipKernel::getSipKernelType(Device &device, bool debuggingEnabled) {
    auto &gfxCoreHelper = device.getGfxCoreHelper();
    return gfxCoreHelper.getSipKernelType(debuggingEnabled);
}

bool SipKernel::initBuiltinsSipKernel(SipKernelType type, Device &device, OsContext *context) {
    if (context) {
        device.getBuiltIns()->getSipKernel(device, context);
    } else {
        device.getBuiltIns()->getSipKernel(type, device);
    }
    return true;
}

bool SipKernel::initRawBinaryFromFileKernel(SipKernelType type, Device &device, std::string &fileName) {
    uint32_t sipIndex = static_cast<uint32_t>(type);
    uint32_t rootDeviceIndex = device.getRootDeviceIndex();

    auto &rootDeviceEnvironment = device.getRootDeviceEnvironment();

    if (rootDeviceEnvironment.sipKernels[sipIndex].get() != nullptr) {
        return true;
    }

    size_t bytesRead = 0u;
    auto fileData = readFile(fileName, bytesRead);

    if (bytesRead) {
        void *alignedBuffer = alignedMalloc(bytesRead, MemoryConstants::pageSize);
        memcpy_s(alignedBuffer, bytesRead, fileData.data(), bytesRead);

        const auto allocType = AllocationType::kernelIsaInternal;
        AllocationProperties properties = {rootDeviceIndex, bytesRead, allocType, device.getDeviceBitfield()};
        properties.flags.use32BitFrontWindow = false;

        auto sipAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
        if (sipAllocation == nullptr) {
            alignedFree(alignedBuffer);
            return false;
        }

        auto &productHelper = device.getProductHelper();

        MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *sipAllocation),
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
    auto &rootDeviceEnvironment = device.getRootDeviceEnvironment();
    auto &gfxCoreHelper = device.getGfxCoreHelper();

    gfxCoreHelper.setSipKernelData(sipKernelBinary, kernelBinarySize, rootDeviceEnvironment);
    const auto allocType = AllocationType::kernelIsaInternal;
    AllocationProperties properties = {rootDeviceIndex, kernelBinarySize, allocType, device.getDeviceBitfield()};
    properties.flags.use32BitFrontWindow = false;

    auto sipAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    if (sipAllocation == nullptr) {
        return false;
    }
    auto &productHelper = device.getProductHelper();
    MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *sipAllocation),
                                                     device, sipAllocation, 0, sipKernelBinary,
                                                     kernelBinarySize);

    std::vector<char> emptyStateSaveAreaHeader;
    device.getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->sipKernels[sipIndex] =
        std::make_unique<SipKernel>(type, sipAllocation, std::move(emptyStateSaveAreaHeader));

    return true;
}

bool SipKernel::initSipKernelFromExternalLib(SipKernelType type, Device &device) {
    uint32_t sipIndex = static_cast<uint32_t>(type);
    uint32_t rootDeviceIndex = device.getRootDeviceIndex();
    auto sipKenel = device.getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->sipKernels[sipIndex].get();
    if (sipKenel != nullptr) {
        return true;
    }

    std::vector<char> sipBinary;
    std::vector<char> stateSaveAreaHeader;
    auto &rootDeviceEnvironment = device.getRootDeviceEnvironment();

    auto ret = device.getSipExternalLibInterface()->getSipKernelBinary(device, type, sipBinary, stateSaveAreaHeader);
    if (ret != 0) {
        return false;
    }
    UNRECOVERABLE_IF(sipBinary.size() == 0);

    const auto allocType = AllocationType::kernelIsaInternal;
    AllocationProperties properties = {device.getRootDeviceIndex(), sipBinary.size(), allocType, device.getDeviceBitfield()};
    properties.flags.use32BitFrontWindow = false;

    auto sipAllocation = device.getMemoryManager()->allocateGraphicsMemoryWithProperties(properties);
    if (sipAllocation == nullptr) {
        return false;
    }
    auto &productHelper = device.getProductHelper();
    MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, *sipAllocation),
                                                     device, sipAllocation, 0, sipBinary.data(),
                                                     sipBinary.size());

    device.getExecutionEnvironment()->rootDeviceEnvironments[rootDeviceIndex]->sipKernels[sipIndex] =
        std::make_unique<SipKernel>(type, sipAllocation, std::move(stateSaveAreaHeader), std::move(sipBinary));

    if (type == SipKernelType::csr) {
        rootDeviceEnvironment.getMutableHardwareInfo()->capabilityTable.requiredPreemptionSurfaceSize = device.getBuiltIns()->getSipKernel(type, device).getStateSaveAreaSize(&device);
    }
    return true;
}

void SipKernel::selectSipClassType(std::string &fileName, Device &device) {
    const GfxCoreHelper &gfxCoreHelper = device.getGfxCoreHelper();
    const std::string unknown("unk");
    if (fileName.compare(unknown) == 0) {
        bool debuggingEnabled = device.getDebugger() != nullptr;
        if (debuggingEnabled) {
            if (device.getSipExternalLibInterface() != nullptr) {
                SipKernel::classType = SipClassType::externalLib;
            } else {
                SipKernel::classType = SipClassType::builtins;
            }
        } else {
            SipKernel::classType = gfxCoreHelper.isSipKernelAsHexadecimalArrayPreferred()
                                       ? SipClassType::hexadecimalHeaderFile
                                       : SipClassType::builtins;
        }
    } else {
        SipKernel::classType = SipClassType::rawBinaryFromFile;
    }
    if (debugManager.flags.ForceSipClass.get() != -1) {
        SipKernel::classType = static_cast<SipClassType>(debugManager.flags.ForceSipClass.get());
    }
}

bool SipKernel::initSipKernelImpl(SipKernelType type, Device &device, OsContext *context) {
    std::string fileName = debugManager.flags.LoadBinarySipFromFile.get();
    SipKernel::selectSipClassType(fileName, device);

    switch (SipKernel::classType) {
    case SipClassType::rawBinaryFromFile:
        return SipKernel::initRawBinaryFromFileKernel(type, device, fileName);
    case SipClassType::hexadecimalHeaderFile:
        return SipKernel::initHexadecimalArraySipKernel(type, device);
    case SipClassType::externalLib:
        return SipKernel::initSipKernelFromExternalLib(type, device);
    default:
        return SipKernel::initBuiltinsSipKernel(type, device, context);
    }
}

const SipKernel &SipKernel::getSipKernelImpl(Device &device) {
    auto sipType = SipKernel::getSipKernelType(device);

    switch (SipKernel::classType) {
    case SipClassType::rawBinaryFromFile:
    case SipClassType::hexadecimalHeaderFile:
        return *device.getRootDeviceEnvironment().sipKernels[static_cast<uint32_t>(sipType)].get();
    default:
        return device.getBuiltIns()->getSipKernel(sipType, device);
    }
}

const SipKernel &SipKernel::getDebugSipKernel(Device &device) {
    SipKernelType debugSipType;
    auto &compilerProductHelper = device.getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    if (compilerProductHelper.isHeaplessModeEnabled(device.getHardwareInfo())) {
        debugSipType = SipKernelType::dbgHeapless;
    } else {
        debugSipType = SipKernelType::dbgBindless;
    }
    SipKernel::initSipKernelImpl(debugSipType, device, nullptr);

    switch (SipKernel::classType) {
    case SipClassType::rawBinaryFromFile:
        return *device.getRootDeviceEnvironment().sipKernels[static_cast<uint32_t>(debugSipType)].get();
    default:
        return device.getBuiltIns()->getSipKernel(debugSipType, device);
    }
}

const SipKernel &SipKernel::getDebugSipKernel(Device &device, OsContext *context) {
    SipKernelType debugSipType;
    auto &compilerProductHelper = device.getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    if (compilerProductHelper.isHeaplessModeEnabled(device.getHardwareInfo())) {
        debugSipType = SipKernelType::dbgHeapless;
    } else {
        debugSipType = SipKernelType::dbgBindless;
    }
    SipKernel::initSipKernelImpl(debugSipType, device, context);

    switch (SipKernel::classType) {
    case SipClassType::rawBinaryFromFile:
        return *device.getRootDeviceEnvironment().sipKernels[static_cast<uint32_t>(debugSipType)].get();
    default:
        return device.getBuiltIns()->getSipKernel(device, context);
    }
}

void SipKernel::parseBinaryForContextId() {
    const int pattern = 0xCAFEBEAD;
    auto memory = reinterpret_cast<const int *>(binary.data());
    const auto sizeInDwords = binary.size() / sizeof(int);
    for (size_t i = 1; i < sizeInDwords; i++) {
        auto currentDword = memory + i;
        if (*currentDword == pattern) {
            for (size_t j = 1; (j < 16) && (i + j < sizeInDwords); j++) {

                if (*(currentDword + j) == pattern) {
                    contextIdOffsets[0] = i;
                    contextIdOffsets[1] = i + j;
                    break;
                }
            }
            break;
        }
    }
}

} // namespace NEO
