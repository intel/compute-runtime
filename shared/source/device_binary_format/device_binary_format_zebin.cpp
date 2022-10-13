/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/device_binary_format/zebin_decoder.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/program_info.h"

#include <tuple>

namespace NEO {

template <Elf::ELF_IDENTIFIER_CLASS numBits>
SingleDeviceBinary unpackSingleZebin(const ArrayRef<const uint8_t> archive, const ConstStringRef requestedProductAbbreviation, const TargetDevice &requestedTargetDevice,
                                     std::string &outErrReason, std::string &outWarning) {

    auto elf = Elf::decodeElf<numBits>(archive, outErrReason, outWarning);
    if (nullptr == elf.elfFileHeader) {
        return {};
    }

    switch (elf.elfFileHeader->type) {
    default:
        outErrReason.append("Unhandled elf type\n");
        return {};
    case NEO::Elf::ET_ZEBIN_EXE:
        break;
    case NEO::Elf::ET_REL:
        break;
    }

    SingleDeviceBinary ret;
    ret.deviceBinary = archive;
    ret.format = NEO::DeviceBinaryFormat::Zebin;
    ret.targetDevice = requestedTargetDevice;

    for (size_t sectionId = 0U; sectionId < elf.sectionHeaders.size(); sectionId++) {
        auto &elfSH = elf.sectionHeaders[sectionId];
        if (elfSH.header->type == Elf::SHT_ZEBIN_SPIRV) {
            ret.intermediateRepresentation = elfSH.data;
        } else if (elfSH.header->type == Elf::SHT_ZEBIN_MISC &&
                   Elf::SectionsNamesZebin::buildOptions == elf.getSectionName(static_cast<uint32_t>(sectionId))) {
            ret.buildOptions = ConstStringRef(reinterpret_cast<const char *>(elfSH.data.begin()), elfSH.data.size());
        }
    }

    bool validForTarget = true;
    if (elf.elfFileHeader->machine == Elf::ELF_MACHINE::EM_INTELGT) {
        validForTarget &= validateTargetDevice(elf, requestedTargetDevice, outErrReason, outWarning);
    } else {
        const auto &flags = reinterpret_cast<const NEO::Elf::ZebinTargetFlags &>(elf.elfFileHeader->flags);
        validForTarget &= flags.machineEntryUsesGfxCoreInsteadOfProductFamily
                              ? (requestedTargetDevice.coreFamily == static_cast<GFXCORE_FAMILY>(elf.elfFileHeader->machine))
                              : (requestedTargetDevice.productFamily == static_cast<PRODUCT_FAMILY>(elf.elfFileHeader->machine));
        validForTarget &= (0 == flags.validateRevisionId) | ((requestedTargetDevice.stepping >= flags.minHwRevisionId) & (requestedTargetDevice.stepping <= flags.maxHwRevisionId));
        validForTarget &= (requestedTargetDevice.maxPointerSizeInBytes >= static_cast<uint32_t>(numBits == Elf::EI_CLASS_32 ? 4 : 8));
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
SingleDeviceBinary unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(const ArrayRef<const uint8_t> archive, const ConstStringRef requestedProductAbbreviation, const TargetDevice &requestedTargetDevice,
                                                                            std::string &outErrReason, std::string &outWarning) {
    return Elf::isElf<Elf::EI_CLASS_32>(archive)
               ? unpackSingleZebin<Elf::EI_CLASS_32>(archive, requestedProductAbbreviation, requestedTargetDevice, outErrReason, outWarning)
               : unpackSingleZebin<Elf::EI_CLASS_64>(archive, requestedProductAbbreviation, requestedTargetDevice, outErrReason, outWarning);
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
void prepareLinkerInputForZebin(ProgramInfo &programInfo, Elf::Elf<numBits> &elf) {
    programInfo.prepareLinkerInputStorage();

    LinkerInput::SectionNameToSegmentIdMap nameToKernelId;
    for (uint32_t id = 0; id < static_cast<uint32_t>(programInfo.kernelInfos.size()); id++) {
        nameToKernelId[programInfo.kernelInfos[id]->kernelDescriptor.kernelMetadata.kernelName] = id;
    }
    programInfo.linkerInput->decodeElfSymbolTableAndRelocations(elf, nameToKernelId);
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
DecodeError decodeSingleZebin(ProgramInfo &dst, const SingleDeviceBinary &src, std::string &outErrReason, std::string &outWarning) {
    auto elf = Elf::decodeElf<numBits>(src.deviceBinary, outErrReason, outWarning);
    if (nullptr == elf.elfFileHeader) {
        return DecodeError::InvalidBinary;
    }

    dst.grfSize = src.targetDevice.grfSize;
    dst.minScratchSpaceSize = src.targetDevice.minScratchSpaceSize;
    auto decodeError = decodeZebin<numBits>(dst, elf, outErrReason, outWarning);
    prepareLinkerInputForZebin<numBits>(dst, elf);
    return decodeError;
}

template <>
DecodeError decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::Zebin>(ProgramInfo &dst, const SingleDeviceBinary &src, std::string &outErrReason, std::string &outWarning) {
    return Elf::isElf<Elf::EI_CLASS_32>(src.deviceBinary)
               ? decodeSingleZebin<Elf::EI_CLASS_32>(dst, src, outErrReason, outWarning)
               : decodeSingleZebin<Elf::EI_CLASS_64>(dst, src, outErrReason, outWarning);
}

} // namespace NEO
