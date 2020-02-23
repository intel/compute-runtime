/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/utilities/compiler_support.h"

#include <tuple>

namespace NEO {

template <>
bool isDeviceBinaryFormat<NEO::DeviceBinaryFormat::OclElf>(const ArrayRef<const uint8_t> binary) {
    auto header = Elf::decodeElfFileHeader<Elf::EI_CLASS_64>(binary);
    if (nullptr == header) {
        return false;
    }

    switch (header->type) {
    default:
        return false;
    case Elf::ET_OPENCL_EXECUTABLE:
        CPP_ATTRIBUTE_FALLTHROUGH;
    case Elf::ET_OPENCL_LIBRARY:
        CPP_ATTRIBUTE_FALLTHROUGH;
    case Elf::ET_OPENCL_OBJECTS:
        return true;
    }
}

template <>
SingleDeviceBinary unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(const ArrayRef<const uint8_t> archive, const ConstStringRef requestedProductAbbreviation, const TargetDevice &requestedTargetDevice,
                                                                             std::string &outErrReason, std::string &outWarning) {
    auto elf = Elf::decodeElf<Elf::EI_CLASS_64>(archive, outErrReason, outWarning);
    if (nullptr == elf.elfFileHeader) {
        return {};
    }

    SingleDeviceBinary ret;
    switch (elf.elfFileHeader->type) {
    default:
        outErrReason = "Not OCL ELF file type";
        return {};

    case Elf::ET_OPENCL_EXECUTABLE:
        ret.format = NEO::DeviceBinaryFormat::Patchtokens;
        break;

    case Elf::ET_OPENCL_LIBRARY:
        ret.format = NEO::DeviceBinaryFormat::OclLibrary;
        break;

    case Elf::ET_OPENCL_OBJECTS:
        ret.format = NEO::DeviceBinaryFormat::OclCompiledObject;
        break;
    }

    for (auto &elfSectionHeader : elf.sectionHeaders) {
        auto sectionData = elfSectionHeader.data;
        switch (elfSectionHeader.header->type) {
        case Elf::SHT_OPENCL_SPIRV:
            CPP_ATTRIBUTE_FALLTHROUGH;
        case Elf::SHT_OPENCL_LLVM_BINARY:
            ret.intermediateRepresentation = sectionData;
            break;

        case Elf::SHT_OPENCL_DEV_BINARY:
            ret.deviceBinary = sectionData;
            break;

        case Elf::SHT_OPENCL_OPTIONS:
            ret.buildOptions = ConstStringRef(reinterpret_cast<const char *>(sectionData.begin()), sectionData.size());
            break;

        case Elf::SHT_OPENCL_DEV_DEBUG:
            ret.debugData = sectionData;
            break;

        case Elf::SHT_STRTAB:
            // ignoring intentionally - identifying sections by types
            continue;

        case Elf::SHT_NULL:
            // ignoring intentionally, inactive section, probably UNDEF
            continue;

        default:
            outErrReason = "Unhandled ELF section";
            return {};
        }
    }

    if (NEO::DeviceBinaryFormat::Patchtokens != ret.format) {
        ret.deviceBinary.clear();
        return ret;
    }

    if (false == ret.deviceBinary.empty()) {
        auto unpackedOclDevBin = unpackSingleDeviceBinary<NEO::DeviceBinaryFormat::Patchtokens>(ret.deviceBinary, requestedProductAbbreviation, requestedTargetDevice,
                                                                                                outErrReason, outWarning);
        if (unpackedOclDevBin.deviceBinary.empty()) {
            ret.deviceBinary.clear();
            ret.debugData.clear();
        } else {
            ret.targetDevice = unpackedOclDevBin.targetDevice;
        }
    }

    return ret;
}

template <>
DecodeError decodeSingleDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(ProgramInfo &dst, const SingleDeviceBinary &src, std::string &outErrReason, std::string &outWarning) {
    // packed binary format
    outErrReason = "Device binary format is packed";
    return DecodeError::InvalidBinary;
}

template <>
std::vector<uint8_t> packDeviceBinary<NEO::DeviceBinaryFormat::OclElf>(const SingleDeviceBinary binary, std::string &outErrReason, std::string &outWarning) {
    using namespace NEO::Elf;
    NEO::Elf::ElfEncoder<EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = ET_OPENCL_EXECUTABLE;
    if (binary.buildOptions.empty() == false) {
        elfEncoder.appendSection(SHT_OPENCL_OPTIONS, SectionNamesOpenCl::buildOptions,
                                 ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(binary.buildOptions.data()), binary.buildOptions.size()));
    }

    if (binary.intermediateRepresentation.empty() == false) {
        if (NEO::isSpirVBitcode(binary.intermediateRepresentation)) {
            elfEncoder.appendSection(SHT_OPENCL_SPIRV, SectionNamesOpenCl::spirvObject, binary.intermediateRepresentation);
        } else if (NEO::isLlvmBitcode(binary.intermediateRepresentation)) {
            elfEncoder.appendSection(SHT_OPENCL_LLVM_BINARY, SectionNamesOpenCl::llvmObject, binary.intermediateRepresentation);
        } else {
            outErrReason = "Unknown intermediate representation format";
            return {};
        }
    }

    if (binary.debugData.empty() == false) {
        elfEncoder.appendSection(SHT_OPENCL_DEV_DEBUG, SectionNamesOpenCl::deviceDebug, binary.debugData);
    }

    if (binary.deviceBinary.empty() == false) {
        elfEncoder.appendSection(SHT_OPENCL_DEV_BINARY, SectionNamesOpenCl::deviceBinary, binary.deviceBinary);
    }

    return elfEncoder.encode();
}

} // namespace NEO
