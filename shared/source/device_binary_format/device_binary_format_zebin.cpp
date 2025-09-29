/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/compiler_interface/linker.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/zebin/zebin_decoder.h"
#include "shared/source/device_binary_format/zebin/zebin_elf.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/program_info.h"

#include <tuple>

namespace NEO {

template <Elf::ElfIdentifierClass numBits>
SingleDeviceBinary unpackSingleZebin(const ArrayRef<const uint8_t> archive, const ConstStringRef requestedProductAbbreviation, const TargetDevice &requestedTargetDevice,
                                     std::string &outErrReason, std::string &outWarning) {
    if (1 == NEO::debugManager.flags.DumpZEBin.get()) {
        dumpFileIncrement(reinterpret_cast<const char *>(archive.begin()), archive.size(), "dumped_zebin_module", ".elf");
    }
    auto elf = Elf::decodeElf<numBits>(archive, outErrReason, outWarning);
    if (nullptr == elf.elfFileHeader) {
        return {};
    }

    switch (elf.elfFileHeader->type) {
    default:
        outErrReason.append("Unhandled elf type\n");
        return {};
    case NEO::Zebin::Elf::ET_ZEBIN_EXE:
        break;
    case NEO::Zebin::Elf::ET_REL:
        break;
    }

    SingleDeviceBinary ret;
    ret.deviceBinary = archive;
    ret.format = NEO::DeviceBinaryFormat::zebin;
    ret.targetDevice = requestedTargetDevice;

    for (size_t sectionId = 0U; sectionId < elf.sectionHeaders.size(); sectionId++) {
        auto &elfSH = elf.sectionHeaders[sectionId];
        if (elfSH.header->type == Zebin::Elf::SHT_ZEBIN_SPIRV) {
            ret.intermediateRepresentation = elfSH.data;
        } else if (elfSH.header->type == Zebin::Elf::SHT_ZEBIN_MISC &&
                   Zebin::Elf::SectionNames::buildOptions == elf.getSectionName(static_cast<uint32_t>(sectionId))) {
            ret.buildOptions = ConstStringRef(reinterpret_cast<const char *>(elfSH.data.begin()), elfSH.data.size());
        }
    }

    bool validForTarget = true;
    if (elf.elfFileHeader->machine == Elf::ElfMachine::EM_INTELGT) {
        validForTarget &= Zebin::validateTargetDevice(elf, requestedTargetDevice, outErrReason, outWarning, ret.generatorFeatureVersions, ret.generator);
    } else {
        Zebin::validateTargetDevice(elf, requestedTargetDevice, outErrReason, outWarning, ret.generatorFeatureVersions, ret.generator);
        const auto &flags = reinterpret_cast<const NEO::Zebin::Elf::ZebinTargetFlags &>(elf.elfFileHeader->flags);
        validForTarget &= flags.machineEntryUsesGfxCoreInsteadOfProductFamily
                              ? (requestedTargetDevice.coreFamily == static_cast<GFXCORE_FAMILY>(elf.elfFileHeader->machine))
                              : (requestedTargetDevice.productFamily == static_cast<PRODUCT_FAMILY>(elf.elfFileHeader->machine));
        validForTarget &= (0 == flags.validateRevisionId) | ((requestedTargetDevice.stepping >= flags.minHwRevisionId) & (requestedTargetDevice.stepping <= flags.maxHwRevisionId));
        validForTarget &= (requestedTargetDevice.maxPointerSizeInBytes >= static_cast<uint32_t>(numBits == Elf::EI_CLASS_32 ? 4 : 8));

        ret.generator = static_cast<GeneratorType>(flags.generatorId);
    }

    if (false == validForTarget) {
        if (false == ret.intermediateRepresentation.empty()) {
            ret.deviceBinary = {};
            outWarning.append("Invalid target device. Rebuilding from intermediate representation.\n");
        } else {
            outErrReason.append("Unhandled target device\n");
            return {};
        }
    }

    return ret;
}

template <>
SingleDeviceBinary unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(const ArrayRef<const uint8_t> archive, const ConstStringRef requestedProductAbbreviation, const TargetDevice &requestedTargetDevice,
                                                                            std::string &outErrReason, std::string &outWarning) {
    return Elf::isElf<Elf::EI_CLASS_32>(archive)
               ? unpackSingleZebin<Elf::EI_CLASS_32>(archive, requestedProductAbbreviation, requestedTargetDevice, outErrReason, outWarning)
               : unpackSingleZebin<Elf::EI_CLASS_64>(archive, requestedProductAbbreviation, requestedTargetDevice, outErrReason, outWarning);
}

template <Elf::ElfIdentifierClass numBits>
void prepareLinkerInputForZebin(ProgramInfo &programInfo, Elf::Elf<numBits> &elf) {
    programInfo.prepareLinkerInputStorage();

    LinkerInput::SectionNameToSegmentIdMap nameToKernelId;
    for (uint32_t id = 0; id < static_cast<uint32_t>(programInfo.kernelInfos.size()); id++) {
        const auto &kernelName = programInfo.kernelInfos[id]->kernelDescriptor.kernelMetadata.kernelName;
        nameToKernelId[kernelName] = id;
        if (kernelName == Zebin::Elf::SectionNames::externalFunctions) {
            programInfo.linkerInput->setExportedFunctionsSegmentId(static_cast<int32_t>(id));
        }
    }
    programInfo.linkerInput->decodeElfSymbolTableAndRelocations(elf, nameToKernelId);
}

template <Elf::ElfIdentifierClass numBits>
DecodeError decodeSingleZebin(ProgramInfo &dst, const SingleDeviceBinary &src, std::string &outErrReason, std::string &outWarning) {
    auto elf = Elf::decodeElf<numBits>(src.deviceBinary, outErrReason, outWarning);
    if (nullptr == elf.elfFileHeader) {
        return DecodeError::invalidBinary;
    }

    GeneratorFeatureVersions generatorFeatures = {};
    GeneratorType generator = {GeneratorType::igc};
    auto ret = Zebin::validateTargetDevice(elf, src.targetDevice, outErrReason, outWarning, generatorFeatures, generator);
    if (!ret && elf.elfFileHeader->machine == Elf::ElfMachine::EM_INTELGT) {
        return DecodeError::invalidBinary;
    }
    dst.grfSize = src.targetDevice.grfSize;
    dst.minScratchSpaceSize = src.targetDevice.minScratchSpaceSize;
    dst.indirectDetectionVersion = generatorFeatures.indirectMemoryAccessDetection;
    dst.indirectAccessBufferMajorVersion = generatorFeatures.indirectAccessBuffer;
    dst.samplerStateSize = src.targetDevice.samplerStateSize;
    dst.samplerBorderColorStateSize = src.targetDevice.samplerBorderColorStateSize;

    auto decodeError = NEO::Zebin::decodeZebin<numBits>(dst, elf, outErrReason, outWarning);
    if (DecodeError::success != decodeError) {
        return decodeError;
    }

    const bool isGeneratedByIgc = generator == GeneratorType::igc;

    for (auto &kernelInfo : dst.kernelInfos) {
        kernelInfo->kernelDescriptor.kernelMetadata.isGeneratedByIgc = isGeneratedByIgc;
        kernelInfo->kernelDescriptor.kernelMetadata.indirectAccessBuffer = generatorFeatures.indirectAccessBuffer;

        if (KernelDescriptor::isBindlessAddressingKernel(kernelInfo->kernelDescriptor)) {
            kernelInfo->kernelDescriptor.initBindlessOffsetToSurfaceState();
        }
    }

    prepareLinkerInputForZebin<numBits>(dst, elf);
    return decodeError;
}

template <>
DecodeError decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::zebin>(ProgramInfo &dst, const SingleDeviceBinary &src, std::string &outErrReason, std::string &outWarning, const GfxCoreHelper &gfxCoreHelper) {
    return Elf::isElf<Elf::EI_CLASS_32>(src.deviceBinary)
               ? decodeSingleZebin<Elf::EI_CLASS_32>(dst, src, outErrReason, outWarning)
               : decodeSingleZebin<Elf::EI_CLASS_64>(dst, src, outErrReason, outWarning);
}

} // namespace NEO
