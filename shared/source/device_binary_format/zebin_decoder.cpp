/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/zebin_decoder.h"

#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/device_binary_format/elf/zeinfo_enum_lookup.h"
#include "shared/source/device_binary_format/yaml/yaml_parser.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/kernel/kernel_arg_descriptor_extended_vme.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/program_info.h"
#include "shared/source/utilities/stackvec.h"

#include <tuple>

namespace NEO {

void setKernelMiscInfoPosition(ConstStringRef metadata, NEO::ProgramInfo &dst) {
    dst.kernelMiscInfoPos = metadata.str().find(Elf::ZebinKernelMetadata::Tags::kernelMiscInfo.str());
}

template <>
bool isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Zebin>(const ArrayRef<const uint8_t> binary) {
    auto isValidZebinHeader = [](auto header) {
        return header != nullptr &&
               (header->type == NEO::Elf::ET_REL ||
                header->type == NEO::Elf::ET_ZEBIN_EXE);
    };
    return Elf::isElf<Elf::EI_CLASS_32>(binary)
               ? isValidZebinHeader(Elf::decodeElfFileHeader<Elf::EI_CLASS_32>(binary))
               : isValidZebinHeader(Elf::decodeElfFileHeader<Elf::EI_CLASS_64>(binary));
}

template bool validateTargetDevice<Elf::EI_CLASS_32>(const Elf::Elf<Elf::EI_CLASS_32> &elf, const TargetDevice &targetDevice, std::string &outErrReason, std::string &outWarning);
template bool validateTargetDevice<Elf::EI_CLASS_64>(const Elf::Elf<Elf::EI_CLASS_64> &elf, const TargetDevice &targetDevice, std::string &outErrReason, std::string &outWarning);
template <Elf::ELF_IDENTIFIER_CLASS numBits>
bool validateTargetDevice(const Elf::Elf<numBits> &elf, const TargetDevice &targetDevice, std::string &outErrReason, std::string &outWarning) {
    GFXCORE_FAMILY gfxCore = IGFX_UNKNOWN_CORE;
    PRODUCT_FAMILY productFamily = IGFX_UNKNOWN;
    Elf::ZebinTargetFlags targetMetadata = {};
    std::vector<Elf::IntelGTNote> intelGTNotes = {};
    auto decodeError = getIntelGTNotes(elf, intelGTNotes, outErrReason, outWarning);
    if (DecodeError::Success != decodeError) {
        return false;
    }
    for (const auto &intelGTNote : intelGTNotes) {
        switch (intelGTNote.type) {
        case Elf::IntelGTSectionType::ProductFamily: {
            DEBUG_BREAK_IF(sizeof(uint32_t) != intelGTNote.data.size());
            auto productFamilyData = reinterpret_cast<const uint32_t *>(intelGTNote.data.begin());
            productFamily = static_cast<PRODUCT_FAMILY>(*productFamilyData);
            break;
        }
        case Elf::IntelGTSectionType::GfxCore: {
            DEBUG_BREAK_IF(sizeof(uint32_t) != intelGTNote.data.size());
            auto gfxCoreData = reinterpret_cast<const uint32_t *>(intelGTNote.data.begin());
            gfxCore = static_cast<GFXCORE_FAMILY>(*gfxCoreData);
            break;
        }
        case Elf::IntelGTSectionType::TargetMetadata: {
            DEBUG_BREAK_IF(sizeof(uint32_t) != intelGTNote.data.size());
            auto targetMetadataPacked = reinterpret_cast<const uint32_t *>(intelGTNote.data.begin());
            targetMetadata.packed = static_cast<uint32_t>(*targetMetadataPacked);
            break;
        }
        case Elf::IntelGTSectionType::ZebinVersion: {
            auto zebinVersionData = reinterpret_cast<const char *>(intelGTNote.data.begin());
            ConstStringRef versionString(zebinVersionData);
            Elf::ZebinKernelMetadata::Types::Version receivedZeInfoVersion{0, 0};
            decodeError = populateZeInfoVersion(receivedZeInfoVersion, versionString, outErrReason);
            if (DecodeError::Success != decodeError) {
                return false;
            }
            decodeError = validateZeInfoVersion(receivedZeInfoVersion, outErrReason, outWarning);
            if (DecodeError::Success != decodeError) {
                return false;
            }
            break;
        }
        default:
            outWarning.append("DeviceBinaryFormat::Zebin : Unrecognized IntelGTNote type: " + std::to_string(intelGTNote.type) + "\n");
            break;
        }
    }
    bool validForTarget = (gfxCore != IGFX_UNKNOWN_CORE) | (productFamily != IGFX_UNKNOWN);
    validForTarget &= (gfxCore != IGFX_UNKNOWN_CORE) ? targetDevice.coreFamily == gfxCore : true;
    validForTarget &= (productFamily != IGFX_UNKNOWN) ? targetDevice.productFamily == productFamily : true;
    validForTarget &= (0 == targetMetadata.validateRevisionId) | ((targetDevice.stepping >= targetMetadata.minHwRevisionId) & (targetDevice.stepping <= targetMetadata.maxHwRevisionId));
    validForTarget &= (targetDevice.maxPointerSizeInBytes >= static_cast<uint32_t>(numBits == Elf::EI_CLASS_32 ? 4 : 8));
    return validForTarget;
}

DecodeError validateZeInfoVersion(const Elf::ZebinKernelMetadata::Types::Version &receivedZeInfoVersion, std::string &outErrReason, std::string &outWarning) {
    if (receivedZeInfoVersion.major != zeInfoDecoderVersion.major) {
        outErrReason.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Unhandled major version : " + std::to_string(receivedZeInfoVersion.major) + ", decoder is at : " + std::to_string(zeInfoDecoderVersion.major) + "\n");
        return DecodeError::UnhandledBinary;
    }
    if (receivedZeInfoVersion.minor > zeInfoDecoderVersion.minor) {
        outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Minor version : " + std::to_string(receivedZeInfoVersion.minor) + " is newer than available in decoder : " + std::to_string(zeInfoDecoderVersion.minor) + " - some features may be skipped\n");
    }
    return DecodeError::Success;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
DecodeError decodeIntelGTNoteSection(ArrayRef<const uint8_t> intelGTNotesSection, std::vector<Elf::IntelGTNote> &intelGTNotes, std::string &outErrReason, std::string &outWarning) {
    uint64_t currentPos = 0;
    auto sectionSize = intelGTNotesSection.size();
    while (currentPos < sectionSize) {
        auto intelGTNote = reinterpret_cast<const Elf::ElfNoteSection *>(intelGTNotesSection.begin() + currentPos);
        auto nameSz = intelGTNote->nameSize;
        auto descSz = intelGTNote->descSize;

        auto currOffset = sizeof(Elf::ElfNoteSection) + alignUp(nameSz, 4) + alignUp(descSz, 4);
        if (currentPos + currOffset > sectionSize) {
            intelGTNotes.clear();
            outErrReason.append("DeviceBinaryFormat::Zebin : Offseting will cause out-of-bound memory read! Section size: " + std::to_string(sectionSize) +
                                ", current section data offset: " + std::to_string(currentPos) + ", next offset : " + std::to_string(currOffset) + "\n");
            return DecodeError::InvalidBinary;
        }
        currentPos += currOffset;

        auto ownerName = reinterpret_cast<const char *>(ptrOffset(intelGTNote, sizeof(Elf::ElfNoteSection)));
        bool isValidGTNote = Elf::IntelGtNoteOwnerName.size() + 1 == nameSz;
        isValidGTNote &= Elf::IntelGtNoteOwnerName == ConstStringRef(ownerName, nameSz - 1);
        if (false == isValidGTNote) {
            if (0u == nameSz) {
                outWarning.append("DeviceBinaryFormat::Zebin : Empty owner name.\n");
            } else {
                std::string invalidOwnerName{ownerName, nameSz};
                invalidOwnerName.erase(std::remove_if(invalidOwnerName.begin(),
                                                      invalidOwnerName.end(),
                                                      [](unsigned char c) { return '\0' == c; }));
                outWarning.append("DeviceBinaryFormat::Zebin : Invalid owner name : " + invalidOwnerName + " for IntelGTNote - note will not be used.\n");
            }
            continue;
        }
        auto notesData = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(ptrOffset(ownerName, nameSz)), descSz);
        if (intelGTNote->type == Elf::IntelGTSectionType::ZebinVersion) {
            isValidGTNote &= notesData[descSz - 1] == '\0';
            if (false == isValidGTNote) {
                outWarning.append("DeviceBinaryFormat::Zebin :  Versioning string is not null-terminated: " + ConstStringRef(reinterpret_cast<const char *>(notesData.begin()), descSz).str() + " - note will not be used.\n");
                continue;
            }
        }
        intelGTNotes.push_back(Elf::IntelGTNote{static_cast<Elf::IntelGTSectionType>(intelGTNote->type), notesData});
    }
    return DecodeError::Success;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
DecodeError getIntelGTNotes(const Elf::Elf<numBits> &elf, std::vector<Elf::IntelGTNote> &intelGTNotes, std::string &outErrReason, std::string &outWarning) {
    for (size_t i = 0; i < elf.sectionHeaders.size(); i++) {
        auto section = elf.sectionHeaders[i];
        if (Elf::SHT_NOTE == section.header->type && Elf::SectionsNamesZebin::noteIntelGT == elf.getSectionName(static_cast<uint32_t>(i))) {
            return decodeIntelGTNoteSection<numBits>(section.data, intelGTNotes, outErrReason, outWarning);
        }
    }
    return DecodeError::Success;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
DecodeError extractZebinSections(NEO::Elf::Elf<numBits> &elf, ZebinSections<numBits> &out, std::string &outErrReason, std::string &outWarning) {
    if ((elf.elfFileHeader->shStrNdx >= elf.sectionHeaders.size()) || (NEO::Elf::SHN_UNDEF == elf.elfFileHeader->shStrNdx)) {
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid or missing shStrNdx in elf header\n");
        return DecodeError::InvalidBinary;
    }

    auto sectionHeaderNamesData = elf.sectionHeaders[elf.elfFileHeader->shStrNdx].data;
    ConstStringRef sectionHeaderNamesString(reinterpret_cast<const char *>(sectionHeaderNamesData.begin()), sectionHeaderNamesData.size());

    for (auto &elfSectionHeader : elf.sectionHeaders) {
        ConstStringRef sectionName = ConstStringRef(sectionHeaderNamesString.begin() + elfSectionHeader.header->name);
        switch (elfSectionHeader.header->type) {
        default:
            outErrReason.append("DeviceBinaryFormat::Zebin : Unhandled ELF section header type : " + std::to_string(elfSectionHeader.header->type) + "\n");
            return DecodeError::InvalidBinary;
        case Elf::SHT_PROGBITS:
            if (sectionName.startsWith(NEO::Elf::SectionsNamesZebin::textPrefix.data())) {
                out.textKernelSections.push_back(&elfSectionHeader);
            } else if (sectionName == NEO::Elf::SectionsNamesZebin::dataConst) {
                out.constDataSections.push_back(&elfSectionHeader);
            } else if (sectionName == NEO::Elf::SectionsNamesZebin::dataGlobalConst) {
                outWarning.append("Misspelled section name : " + sectionName.str() + ", should be : " + NEO::Elf::SectionsNamesZebin::dataConst.str() + "\n");
                out.constDataSections.push_back(&elfSectionHeader);
            } else if (sectionName == NEO::Elf::SectionsNamesZebin::dataGlobal) {
                out.globalDataSections.push_back(&elfSectionHeader);
            } else if (sectionName == NEO::Elf::SectionsNamesZebin::dataConstString) {
                out.constDataStringSections.push_back(&elfSectionHeader);
            } else if (sectionName.startsWith(NEO::Elf::SectionsNamesZebin::debugPrefix.data())) {
                // ignoring intentionally
            } else {
                outErrReason.append("DeviceBinaryFormat::Zebin : Unhandled SHT_PROGBITS section : " + sectionName.str() + " currently supports only : " + NEO::Elf::SectionsNamesZebin::textPrefix.str() + "KERNEL_NAME, " + NEO::Elf::SectionsNamesZebin::dataConst.str() + ", " + NEO::Elf::SectionsNamesZebin::dataGlobal.str() + " and " + NEO::Elf::SectionsNamesZebin::debugPrefix.str() + "* .\n");
                return DecodeError::InvalidBinary;
            }
            break;
        case NEO::Elf::SHT_ZEBIN_ZEINFO:
            out.zeInfoSections.push_back(&elfSectionHeader);
            break;
        case NEO::Elf::SHT_SYMTAB:
            out.symtabSections.push_back(&elfSectionHeader);
            break;
        case NEO::Elf::SHT_ZEBIN_SPIRV:
            out.spirvSections.push_back(&elfSectionHeader);
            break;
        case NEO::Elf::SHT_NOTE:
            if (sectionName == NEO::Elf::SectionsNamesZebin::noteIntelGT) {
                out.noteIntelGTSections.push_back(&elfSectionHeader);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin : Unhandled SHT_NOTE section : " + sectionName.str() + " currently supports only : " + NEO::Elf::SectionsNamesZebin::noteIntelGT.str() + ".\n");
            }
            break;
        case NEO::Elf::SHT_ZEBIN_MISC:
            if (sectionName == NEO::Elf::SectionsNamesZebin::buildOptions) {
                out.buildOptionsSection.push_back(&elfSectionHeader);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin : unhandled SHT_ZEBIN_MISC section : " + sectionName.str() + " currently supports only : " + NEO::Elf::SectionsNamesZebin::buildOptions.str() + ".\n");
            }
            break;
        case NEO::Elf::SHT_STRTAB:
            // ignoring intentionally - section header names
            continue;
        case NEO::Elf::SHT_REL:
        case NEO::Elf::SHT_RELA:
            // ignoring intentionally - rel/rela sections handled by Elf decoder
            continue;
        case NEO::Elf::SHT_ZEBIN_GTPIN_INFO:
            // ignoring intentionally - gtpin internal data
            continue;
        case NEO::Elf::SHT_ZEBIN_VISA_ASM:
            // ignoring intentionally - visa asm
            continue;
        case NEO::Elf::SHT_NULL:
            // ignoring intentionally, inactive section, probably UNDEF
            continue;
        }
    }

    return DecodeError::Success;
}

template <typename ContainerT>
bool validateZebinSectionsCountAtMost(const ContainerT &sectionsContainer, ConstStringRef sectionName, uint32_t max, std::string &outErrReason, std::string &outWarning) {
    if (sectionsContainer.size() <= max) {
        return true;
    }

    outErrReason.append("DeviceBinaryFormat::Zebin : Expected at most " + std::to_string(max) + " of " + sectionName.str() + " section, got : " + std::to_string(sectionsContainer.size()) + "\n");
    return false;
}

template <typename ContainerT>
bool validateZebinSectionsCountExactly(const ContainerT &sectionsContainer, ConstStringRef sectionName, uint32_t num, std::string &outErrReason, std::string &outWarning) {
    if (sectionsContainer.size() == num) {
        return true;
    }

    outErrReason.append("DeviceBinaryFormat::Zebin : Expected exactly " + std::to_string(num) + " of " + sectionName.str() + " section, got : " + std::to_string(sectionsContainer.size()) + "\n");
    return false;
}

template DecodeError validateZebinSectionsCount<Elf::EI_CLASS_32>(const ZebinSections<Elf::EI_CLASS_32> &sections, std::string &outErrReason, std::string &outWarning);
template DecodeError validateZebinSectionsCount<Elf::EI_CLASS_64>(const ZebinSections<Elf::EI_CLASS_64> &sections, std::string &outErrReason, std::string &outWarning);
template <Elf::ELF_IDENTIFIER_CLASS numBits>
DecodeError validateZebinSectionsCount(const ZebinSections<numBits> &sections, std::string &outErrReason, std::string &outWarning) {
    bool valid = validateZebinSectionsCountAtMost(sections.zeInfoSections, NEO::Elf::SectionsNamesZebin::zeInfo, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(sections.globalDataSections, NEO::Elf::SectionsNamesZebin::dataGlobal, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(sections.constDataSections, NEO::Elf::SectionsNamesZebin::dataConst, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(sections.constDataStringSections, NEO::Elf::SectionsNamesZebin::dataConstString, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(sections.symtabSections, NEO::Elf::SectionsNamesZebin::symtab, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(sections.spirvSections, NEO::Elf::SectionsNamesZebin::spv, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(sections.noteIntelGTSections, NEO::Elf::SectionsNamesZebin::noteIntelGT, 1U, outErrReason, outWarning);
    return valid ? DecodeError::Success : DecodeError::InvalidBinary;
}

void extractZeInfoKernelSections(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &kernelNd, ZeInfoKernelSections &outZeInfoKernelSections, ConstStringRef context, std::string &outWarning) {
    for (const auto &kernelMetadataNd : parser.createChildrenRange(kernelNd)) {
        auto key = parser.readKey(kernelMetadataNd);
        if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::name == key) {
            outZeInfoKernelSections.nameNd.push_back(&kernelMetadataNd);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::attributes == key) {
            outZeInfoKernelSections.attributesNd.push_back(&kernelMetadataNd);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::executionEnv == key) {
            outZeInfoKernelSections.executionEnvNd.push_back(&kernelMetadataNd);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::debugEnv == key) {
            outZeInfoKernelSections.debugEnvNd.push_back(&kernelMetadataNd);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::payloadArguments == key) {
            outZeInfoKernelSections.payloadArgumentsNd.push_back(&kernelMetadataNd);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::perThreadPayloadArguments == key) {
            outZeInfoKernelSections.perThreadPayloadArgumentsNd.push_back(&kernelMetadataNd);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::bindingTableIndices == key) {
            outZeInfoKernelSections.bindingTableIndicesNd.push_back(&kernelMetadataNd);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::perThreadMemoryBuffers == key) {
            outZeInfoKernelSections.perThreadMemoryBuffersNd.push_back(&kernelMetadataNd);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::experimentalProperties == key) {
            outZeInfoKernelSections.experimentalPropertiesNd.push_back(&kernelMetadataNd);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::inlineSamplers == key) {
            outZeInfoKernelSections.inlineSamplersNd.push_back(&kernelMetadataNd);
        } else {
            outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Unknown entry \"" + parser.readKey(kernelMetadataNd).str() + "\" in context of : " + context.str() + "\n");
        }
    }
}

DecodeError validateZeInfoKernelSectionsCount(const ZeInfoKernelSections &outZeInfoKernelSections, std::string &outErrReason, std::string &outWarning) {
    bool valid = validateZebinSectionsCountExactly(outZeInfoKernelSections.nameNd, NEO::Elf::ZebinKernelMetadata::Tags::Kernel::name, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountExactly(outZeInfoKernelSections.executionEnvNd, NEO::Elf::ZebinKernelMetadata::Tags::Kernel::executionEnv, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(outZeInfoKernelSections.attributesNd, NEO::Elf::ZebinKernelMetadata::Tags::Kernel::attributes, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(outZeInfoKernelSections.debugEnvNd, NEO::Elf::ZebinKernelMetadata::Tags::Kernel::debugEnv, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(outZeInfoKernelSections.payloadArgumentsNd, NEO::Elf::ZebinKernelMetadata::Tags::Kernel::payloadArguments, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(outZeInfoKernelSections.perThreadPayloadArgumentsNd, NEO::Elf::ZebinKernelMetadata::Tags::Kernel::perThreadPayloadArguments, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(outZeInfoKernelSections.bindingTableIndicesNd, NEO::Elf::ZebinKernelMetadata::Tags::Kernel::bindingTableIndices, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(outZeInfoKernelSections.perThreadMemoryBuffersNd, NEO::Elf::ZebinKernelMetadata::Tags::Kernel::perThreadMemoryBuffers, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(outZeInfoKernelSections.experimentalPropertiesNd, NEO::Elf::ZebinKernelMetadata::Tags::Kernel::experimentalProperties, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(outZeInfoKernelSections.inlineSamplersNd, NEO::Elf::ZebinKernelMetadata::Tags::Kernel::inlineSamplers, 1U, outErrReason, outWarning);

    return valid ? DecodeError::Success : DecodeError::InvalidBinary;
}

template <typename T>
bool readZeInfoValueChecked(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, T &outValue, ConstStringRef context, std::string &outErrReason) {
    if (parser.readValueChecked(node, outValue)) {
        return true;
    }
    outErrReason.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : could not read " + parser.readKey(node).str() + " from : [" + parser.readValue(node).str() + "] in context of : " + context.str() + "\n");
    return false;
}

template <typename DestinationT, size_t Len>
bool readZeInfoValueCollectionCheckedArr(std::array<DestinationT, Len> &vec, const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, ConstStringRef context, std::string &outErrReason) {
    auto collectionNodes = parser.createChildrenRange(node);
    size_t index = 0U;
    bool isValid = true;

    for (const auto &elementNd : collectionNodes) {
        isValid &= readZeInfoValueChecked(parser, elementNd, vec[index++], context, outErrReason);
    }

    if (index != Len) {
        outErrReason.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : wrong size of collection " + parser.readKey(node).str() + " in context of : " + context.str() + ". Got : " + std::to_string(index) + " expected : " + std::to_string(Len) + "\n");
        isValid = false;
    }
    return isValid;
}

template <typename DestinationT, size_t Len>
bool readZeInfoValueCollectionChecked(DestinationT (&vec)[Len], const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, ConstStringRef context, std::string &outErrReason) {
    auto &array = reinterpret_cast<std::array<DestinationT, Len> &>(vec);
    return readZeInfoValueCollectionCheckedArr(array, parser, node, context, outErrReason);
}

DecodeError readZeInfoExecutionEnvironment(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node,
                                           NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExecutionEnvBaseT &outExecEnv,
                                           ConstStringRef context,
                                           std::string &outErrReason, std::string &outWarning) {
    bool validExecEnv = true;
    for (const auto &execEnvMetadataNd : parser.createChildrenRange(node)) {
        auto key = parser.readKey(execEnvMetadataNd);
        if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::barrierCount == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.barrierCount, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::disableMidThreadPreemption == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.disableMidThreadPreemption, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::grfCount == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.grfCount, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::has4gbBuffers == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.has4GBBuffers, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::hasDpas == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hasDpas, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::hasFenceForImageAccess == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hasFenceForImageAccess, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::hasGlobalAtomics == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hasGlobalAtomics, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::hasMultiScratchSpaces == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hasMultiScratchSpaces, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::hasNoStatelessWrite == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hasNoStatelessWrite, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::hasStackCalls == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hasStackCalls, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::hwPreemptionMode == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.hwPreemptionMode, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::inlineDataPayloadSize == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.inlineDataPayloadSize, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::offsetToSkipPerThreadDataLoad == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.offsetToSkipPerThreadDataLoad, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::offsetToSkipSetFfidGp == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.offsetToSkipSetFfidGp, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::requiredSubGroupSize == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.requiredSubGroupSize, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::requiredWorkGroupSize == key) {
            validExecEnv = validExecEnv & readZeInfoValueCollectionChecked(outExecEnv.requiredWorkGroupSize, parser, execEnvMetadataNd, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::requireDisableEUFusion == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.requireDisableEUFusion, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::simdSize == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.simdSize, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::slmSize == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.slmSize, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::subgroupIndependentForwardProgress == key) {
            validExecEnv = validExecEnv & readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.subgroupIndependentForwardProgress, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::workGroupWalkOrderDimensions == key) {
            validExecEnv = validExecEnv & readZeInfoValueCollectionChecked(outExecEnv.workgroupWalkOrderDimensions, parser, execEnvMetadataNd, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::threadSchedulingMode == key) {
            validExecEnv &= readZeInfoEnumChecked(parser, execEnvMetadataNd, outExecEnv.threadSchedulingMode, context, outErrReason);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExecutionEnv::indirectStatelessCount == key) {
            validExecEnv &= readZeInfoValueChecked(parser, execEnvMetadataNd, outExecEnv.indirectStatelessCount, context, outErrReason);
        } else {
            outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Unknown entry \"" + key.str() + "\" in context of " + context.str() + "\n");
        }
    }
    return validExecEnv ? DecodeError::Success : DecodeError::InvalidBinary;
}

DecodeError readZeInfoAttributes(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, NEO::Elf::ZebinKernelMetadata::Types::Kernel::Attributes::AttributesBaseT &outAttributes, ConstStringRef context, std::string &outErrReason, std::string &outWarning) {
    namespace AttributeTypes = NEO::Elf::ZebinKernelMetadata::Types::Kernel::Attributes;
    bool validAttributes = true;
    for (const auto &attributesMetadataNd : parser.createChildrenRange(node)) {
        auto key = parser.readKey(attributesMetadataNd);
        if (key == NEO::Elf::ZebinKernelMetadata::Tags::Kernel::Attributes::intelReqdSubgroupSize) {
            outAttributes.intelReqdSubgroupSize = AttributeTypes::Defaults::intelReqdSubgroupSize;
            validAttributes &= readZeInfoValueChecked(parser, attributesMetadataNd, *outAttributes.intelReqdSubgroupSize, context, outErrReason);
        } else if (key == NEO::Elf::ZebinKernelMetadata::Tags::Kernel::Attributes::intelReqdWorkgroupWalkOrder) {
            outAttributes.intelReqdWorkgroupWalkOrder = AttributeTypes::Defaults::intelReqdWorkgroupWalkOrder;
            validAttributes &= readZeInfoValueCollectionCheckedArr(*outAttributes.intelReqdWorkgroupWalkOrder, parser, attributesMetadataNd, context, outErrReason);
        } else if (key == NEO::Elf::ZebinKernelMetadata::Tags::Kernel::Attributes::reqdWorkgroupSize) {
            outAttributes.reqdWorkgroupSize = AttributeTypes::Defaults::reqdWorkgroupSize;
            validAttributes &= readZeInfoValueCollectionCheckedArr(*outAttributes.reqdWorkgroupSize, parser, attributesMetadataNd, context, outErrReason);
        } else if (key == NEO::Elf::ZebinKernelMetadata::Tags::Kernel::Attributes::workgroupSizeHint) {
            outAttributes.workgroupSizeHint = AttributeTypes::Defaults::workgroupSizeHint;
            validAttributes &= readZeInfoValueCollectionCheckedArr(*outAttributes.workgroupSizeHint, parser, attributesMetadataNd, context, outErrReason);
        } else if (key == NEO::Elf::ZebinKernelMetadata::Tags::Kernel::Attributes::invalidKernel) {
            outAttributes.invalidKernel = parser.readValue(attributesMetadataNd);
        } else if (key == NEO::Elf::ZebinKernelMetadata::Tags::Kernel::Attributes::vecTypeHint) {
            outAttributes.vecTypeHint = parser.readValue(attributesMetadataNd);
        } else if (key.contains(NEO::Elf::ZebinKernelMetadata::Tags::Kernel::Attributes::hintSuffix.data())) {
            outAttributes.otherHints.push_back({key, parser.readValue(attributesMetadataNd)});
        } else {
            outErrReason.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Unknown attribute entry \"" + key.str() + "\" in context of " + context.str() + "\n");
            validAttributes = false;
        }
    }
    return validAttributes ? DecodeError::Success : DecodeError::InvalidBinary;
}

DecodeError readZeInfoDebugEnvironment(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node,
                                       NEO::Elf::ZebinKernelMetadata::Types::Kernel::DebugEnv::DebugEnvBaseT &outDebugEnv,
                                       ConstStringRef context,
                                       std::string &outErrReason, std::string &outWarning) {
    bool validDebugEnv = true;
    for (const auto &debugEnvNd : parser.createChildrenRange(node)) {
        auto key = parser.readKey(debugEnvNd);
        if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::DebugEnv::debugSurfaceBTI == key) {
            validDebugEnv = validDebugEnv & readZeInfoValueChecked(parser, debugEnvNd, outDebugEnv.debugSurfaceBTI, context, outErrReason);
        } else {
            outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Unknown entry \"" + key.str() + "\" in context of " + context.str() + "\n");
        }
    }
    return validDebugEnv ? DecodeError::Success : DecodeError::InvalidBinary;
}

DecodeError readZeInfoExperimentalProperties(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node,
                                             NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExperimentalPropertiesBaseT &outExperimentalProperties,
                                             ConstStringRef context,
                                             std::string &outErrReason, std::string &outWarning) {
    bool validExperimentalProperty = true;
    for (const auto &experimentalPropertyNd : parser.createChildrenRange(node)) {
        for (const auto &experimentalPropertyMemberNd : parser.createChildrenRange(experimentalPropertyNd)) {
            auto key = parser.readKey(experimentalPropertyMemberNd);
            if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExperimentalProperties::hasNonKernelArgLoad == key) {
                validExperimentalProperty = validExperimentalProperty & readZeInfoValueChecked(parser, experimentalPropertyMemberNd,
                                                                                               outExperimentalProperties.hasNonKernelArgLoad, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExperimentalProperties::hasNonKernelArgStore == key) {
                validExperimentalProperty = validExperimentalProperty & readZeInfoValueChecked(parser, experimentalPropertyMemberNd,
                                                                                               outExperimentalProperties.hasNonKernelArgStore, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::ExperimentalProperties::hasNonKernelArgAtomic == key) {
                validExperimentalProperty = validExperimentalProperty & readZeInfoValueChecked(parser, experimentalPropertyMemberNd,
                                                                                               outExperimentalProperties.hasNonKernelArgAtomic, context, outErrReason);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Unknown entry \"" + key.str() + "\" in context of " + context.str() + "\n");
                validExperimentalProperty = false;
            }
        }
    }

    return validExperimentalProperty ? DecodeError::Success : DecodeError::InvalidBinary;
}

template <typename T>
bool readEnumChecked(ConstStringRef enumString, T &outValue, ConstStringRef kernelName, std::string &outErrReason) {
    using EnumLooker = NEO::Zebin::ZeInfo::EnumLookup::EnumLooker<T>;
    auto enumVal = EnumLooker::members.find(enumString);
    outValue = enumVal.value_or(static_cast<T>(0));

    if (false == enumVal.has_value()) {
        outErrReason.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Unhandled \"" + enumString.str() + "\" " + EnumLooker::name.str() + " in context of " + kernelName.str() + "\n");
    }

    return enumVal.has_value();
}

template <typename T>
bool readZeInfoEnumChecked(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, T &outValue, ConstStringRef kernelName, std::string &outErrReason) {
    auto token = parser.getValueToken(node);
    if (nullptr == token) {
        return false;
    }
    auto tokenValue = token->cstrref();
    return readEnumChecked(tokenValue, outValue, kernelName, outErrReason);
}
template bool readZeInfoEnumChecked<NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::ArgTypeT>(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::ArgTypeT &outValue, ConstStringRef kernelName, std::string &outErrReason);

DecodeError readZeInfoPerThreadPayloadArguments(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node,
                                                ZeInfoPerThreadPayloadArguments &outPerThreadPayloadArguments,
                                                ConstStringRef context,
                                                std::string &outErrReason, std::string &outWarning) {
    bool validPerThreadPayload = true;
    for (const auto &perThredPayloadArgumentNd : parser.createChildrenRange(node)) {
        outPerThreadPayloadArguments.resize(outPerThreadPayloadArguments.size() + 1);
        auto &perThreadPayloadArgMetadata = *outPerThreadPayloadArguments.rbegin();
        ConstStringRef argTypeStr;
        for (const auto &perThreadPayloadArgumentMemberNd : parser.createChildrenRange(perThredPayloadArgumentNd)) {
            auto key = parser.readKey(perThreadPayloadArgumentMemberNd);
            if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadPayloadArgument::argType == key) {
                argTypeStr = parser.readValue(perThreadPayloadArgumentMemberNd);
                validPerThreadPayload &= readZeInfoEnumChecked(parser, perThreadPayloadArgumentMemberNd, perThreadPayloadArgMetadata.argType, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadPayloadArgument::size == key) {
                validPerThreadPayload &= readZeInfoValueChecked(parser, perThreadPayloadArgumentMemberNd, perThreadPayloadArgMetadata.size, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadPayloadArgument::offset == key) {
                validPerThreadPayload &= readZeInfoValueChecked(parser, perThreadPayloadArgumentMemberNd, perThreadPayloadArgMetadata.offset, context, outErrReason);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Unknown entry \"" + key.str() + "\" for per-thread payload argument in context of " + context.str() + "\n");
            }
        }
        if (0 == perThreadPayloadArgMetadata.size) {
            outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Skippinig 0-size per-thread argument of type : " + argTypeStr.str() + " in context of " + context.str() + "\n");
            outPerThreadPayloadArguments.pop_back();
        }
    }

    return validPerThreadPayload ? DecodeError::Success : DecodeError::InvalidBinary;
}

DecodeError readZeInfoPayloadArguments(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node,
                                       ZeInfoPayloadArguments &ouPayloadArguments,
                                       int32_t &outMaxPayloadArgumentIndex,
                                       int32_t &outMaxSamplerIndex,
                                       ConstStringRef context,
                                       std::string &outErrReason, std::string &outWarning) {
    bool validPayload = true;
    for (const auto &payloadArgumentNd : parser.createChildrenRange(node)) {
        ouPayloadArguments.resize(ouPayloadArguments.size() + 1);
        auto &payloadArgMetadata = *ouPayloadArguments.rbegin();
        for (const auto &payloadArgumentMemberNd : parser.createChildrenRange(payloadArgumentNd)) {
            auto key = parser.readKey(payloadArgumentMemberNd);
            if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::argType == key) {
                validPayload &= readZeInfoEnumChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.argType, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::argIndex == key) {
                validPayload &= parser.readValueChecked(payloadArgumentMemberNd, payloadArgMetadata.argIndex);
                outMaxPayloadArgumentIndex = std::max<int32_t>(outMaxPayloadArgumentIndex, payloadArgMetadata.argIndex);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::offset == key) {
                validPayload &= readZeInfoValueChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.offset, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::size == key) {
                validPayload &= readZeInfoValueChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.size, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::addrmode == key) {
                validPayload &= readZeInfoEnumChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.addrmode, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::addrspace == key) {
                validPayload &= readZeInfoEnumChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.addrspace, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::accessType == key) {
                validPayload &= readZeInfoEnumChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.accessType, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::samplerIndex == key) {
                validPayload &= parser.readValueChecked(payloadArgumentMemberNd, payloadArgMetadata.samplerIndex);
                outMaxSamplerIndex = std::max<int32_t>(outMaxSamplerIndex, payloadArgMetadata.samplerIndex);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::sourceOffset == key) {
                validPayload &= readZeInfoValueChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.sourceOffset, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::slmArgAlignment == key) {
                validPayload &= readZeInfoValueChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.slmArgAlignment, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::imageType == key) {
                validPayload &= readZeInfoEnumChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.imageType, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::imageTransformable == key) {
                validPayload &= readZeInfoValueChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.imageTransformable, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::samplerType == key) {
                validPayload &= readZeInfoEnumChecked(parser, payloadArgumentMemberNd, payloadArgMetadata.samplerType, context, outErrReason);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Unknown entry \"" + key.str() + "\" for payload argument in context of " + context.str() + "\n");
            }
        }
    }
    return validPayload ? DecodeError::Success : DecodeError::InvalidBinary;
}

DecodeError readZeInfoInlineSamplers(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, ZeInfoInlineSamplers &outInlineSamplers, int32_t &outMaxSamplerIndex, ConstStringRef context, std::string &outErrReason, std::string &outWarning) {
    bool validInlineSamplers = true;
    for (const auto &inlineSamplerNd : parser.createChildrenRange(node)) {
        outInlineSamplers.resize(outInlineSamplers.size() + 1);
        auto &inlineSampler = *outInlineSamplers.rbegin();
        for (const auto &inlineSamplerMemberNd : parser.createChildrenRange(inlineSamplerNd)) {
            namespace Tags = NEO::Elf::ZebinKernelMetadata::Tags::Kernel::InlineSamplers;
            auto key = parser.readKey(inlineSamplerMemberNd);
            if (Tags::samplerIndex == key) {
                validInlineSamplers &= readZeInfoValueChecked(parser, inlineSamplerMemberNd, inlineSampler.samplerIndex, context, outErrReason);
                outMaxSamplerIndex = std::max<int32_t>(outMaxSamplerIndex, inlineSampler.samplerIndex);
            } else if (Tags::addrMode == key) {
                validInlineSamplers &= readZeInfoEnumChecked(parser, inlineSamplerMemberNd, inlineSampler.addrMode, context, outErrReason);
            } else if (Tags::filterMode == key) {
                validInlineSamplers &= readZeInfoEnumChecked(parser, inlineSamplerMemberNd, inlineSampler.filterMode, context, outErrReason);
            } else if (Tags::normalized == key) {
                validInlineSamplers &= readZeInfoValueChecked(parser, inlineSamplerMemberNd, inlineSampler.normalized, context, outErrReason);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Unknown entry \"" + key.str() + "\" for inline sampler in context of " + context.str() + "\n");
            }
        }
    }

    return validInlineSamplers ? DecodeError::Success : DecodeError::InvalidBinary;
}

DecodeError readZeInfoBindingTableIndices(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node,
                                          ZeInfoBindingTableIndices &outBindingTableIndices, ZeInfoBindingTableIndices::value_type &outMaxBindingTableIndex,
                                          ConstStringRef context,
                                          std::string &outErrReason, std::string &outWarning) {
    bool validBindingTableEntries = true;
    for (const auto &bindingTableIndexNd : parser.createChildrenRange(node)) {
        outBindingTableIndices.resize(outBindingTableIndices.size() + 1);
        auto &bindingTableIndexMetadata = *outBindingTableIndices.rbegin();
        for (const auto &bindingTableIndexMemberNd : parser.createChildrenRange(bindingTableIndexNd)) {
            auto key = parser.readKey(bindingTableIndexMemberNd);
            if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::BindingTableIndex::argIndex == key) {
                validBindingTableEntries &= readZeInfoValueChecked(parser, bindingTableIndexMemberNd, bindingTableIndexMetadata.argIndex, context, outErrReason);
                outMaxBindingTableIndex.argIndex = std::max<uint32_t>(outMaxBindingTableIndex.argIndex, bindingTableIndexMetadata.argIndex);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::BindingTableIndex::btiValue == key) {
                validBindingTableEntries &= readZeInfoValueChecked(parser, bindingTableIndexMemberNd, bindingTableIndexMetadata.btiValue, context, outErrReason);
                outMaxBindingTableIndex.btiValue = std::max<uint32_t>(outMaxBindingTableIndex.btiValue, bindingTableIndexMetadata.btiValue);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Unknown entry \"" + key.str() + "\" for binding table index in context of " + context.str() + "\n");
            }
        }
    }

    return validBindingTableEntries ? DecodeError::Success : DecodeError::InvalidBinary;
}

DecodeError readZeInfoPerThreadMemoryBuffers(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node,
                                             ZeInfoPerThreadMemoryBuffers &outPerThreadMemoryBuffers,
                                             ConstStringRef context,
                                             std::string &outErrReason, std::string &outWarning) {
    bool validBuffer = true;
    for (const auto &perThreadMemoryBufferNd : parser.createChildrenRange(node)) {
        outPerThreadMemoryBuffers.resize(outPerThreadMemoryBuffers.size() + 1);
        auto &perThreadMemoryBufferMetadata = *outPerThreadMemoryBuffers.rbegin();
        for (const auto &perThreadMemoryBufferMemberNd : parser.createChildrenRange(perThreadMemoryBufferNd)) {
            auto key = parser.readKey(perThreadMemoryBufferMemberNd);
            if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadMemoryBuffer::allocationType == key) {
                validBuffer &= readZeInfoEnumChecked(parser, perThreadMemoryBufferMemberNd, perThreadMemoryBufferMetadata.allocationType, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadMemoryBuffer::memoryUsage == key) {
                validBuffer &= readZeInfoEnumChecked(parser, perThreadMemoryBufferMemberNd, perThreadMemoryBufferMetadata.memoryUsage, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadMemoryBuffer::size == key) {
                validBuffer &= readZeInfoValueChecked(parser, perThreadMemoryBufferMemberNd, perThreadMemoryBufferMetadata.size, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadMemoryBuffer::isSimtThread == key) {
                validBuffer &= readZeInfoValueChecked(parser, perThreadMemoryBufferMemberNd, perThreadMemoryBufferMetadata.isSimtThread, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadMemoryBuffer::slot == key) {
                validBuffer &= readZeInfoValueChecked(parser, perThreadMemoryBufferMemberNd, perThreadMemoryBufferMetadata.slot, context, outErrReason);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Unknown entry \"" + key.str() + "\" for per-thread memory buffer in context of " + context.str() + "\n");
            }
        }
    }
    return validBuffer ? DecodeError::Success : DecodeError::InvalidBinary;
}

DecodeError readZeInfoGlobalHostAceessTable(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node,
                                            ZeInfoGlobalHostAccessTables &outDeviceNameToHostTable,
                                            ConstStringRef context,
                                            std::string &outErrReason, std::string &outWarning) {
    bool validTable = true;
    for (const auto &globalHostAccessNameNd : parser.createChildrenRange(node)) {
        outDeviceNameToHostTable.resize(outDeviceNameToHostTable.size() + 1);
        for (const auto &globalHostAccessNameMemberNd : parser.createChildrenRange(globalHostAccessNameNd)) {
            auto &globalHostAccessMetadata = *outDeviceNameToHostTable.rbegin();
            auto key = parser.readKey(globalHostAccessNameMemberNd);
            if (NEO::Elf::ZebinKernelMetadata::Tags::GlobalHostAccessTable::deviceName == key) {
                validTable &= readZeInfoValueChecked(parser, globalHostAccessNameMemberNd, globalHostAccessMetadata.deviceName, context, outErrReason);
            } else if (NEO::Elf::ZebinKernelMetadata::Tags::GlobalHostAccessTable::hostName == key) {
                validTable &= readZeInfoValueChecked(parser, globalHostAccessNameMemberNd, globalHostAccessMetadata.hostName, context, outErrReason);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Unknown entry \"" + key.str() + "\" for payload argument in context of " + context.str() + "\n");
            }
        }
    }
    return validTable ? DecodeError::Success : DecodeError::InvalidBinary;
}

template <typename ElSize, size_t Len>
bool setVecArgIndicesBasedOnSize(CrossThreadDataOffset (&vec)[Len], size_t vecSize, CrossThreadDataOffset baseOffset) {
    switch (vecSize) {
    default:
        return false;
    case sizeof(ElSize) * 3:
        vec[2] = static_cast<CrossThreadDataOffset>(baseOffset + 2 * sizeof(ElSize));
        [[fallthrough]];
    case sizeof(ElSize) * 2:
        vec[1] = static_cast<CrossThreadDataOffset>(baseOffset + 1 * sizeof(ElSize));
        [[fallthrough]];
    case sizeof(ElSize) * 1:
        vec[0] = static_cast<CrossThreadDataOffset>(baseOffset + 0 * sizeof(ElSize));
        break;
    }
    return true;
}

NEO::DecodeError populateArgDescriptor(const NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadPayloadArgument::PerThreadPayloadArgumentBaseT &src, NEO::KernelDescriptor &dst, uint32_t grfSize,
                                       std::string &outErrReason, std::string &outWarning) {
    switch (src.argType) {
    default:
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid arg type in per-thread data section in context of : " + dst.kernelMetadata.kernelName + ".\n");
        return DecodeError::InvalidBinary;
    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeLocalId: {
        if (src.offset != 0) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid offset for argument of type " + NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadPayloadArgument::ArgType::localId.str() + " in context of : " + dst.kernelMetadata.kernelName + ". Expected 0.\n");
            return DecodeError::InvalidBinary;
        }
        using LocalIdT = uint16_t;

        uint32_t singleChannelIndicesCount = (dst.kernelAttributes.simdSize == 32 ? 32 : 16);
        uint32_t singleChannelBytes = singleChannelIndicesCount * sizeof(LocalIdT);
        UNRECOVERABLE_IF(0 == grfSize);
        singleChannelBytes = alignUp(singleChannelBytes, grfSize);
        auto tupleSize = (src.size / singleChannelBytes);
        switch (tupleSize) {
        default:
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid size for argument of type " + NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadPayloadArgument::ArgType::localId.str() + " in context of : " + dst.kernelMetadata.kernelName + ". For simd=" + std::to_string(dst.kernelAttributes.simdSize) + " expected : " + std::to_string(singleChannelBytes) + " or " + std::to_string(singleChannelBytes * 2) + " or " + std::to_string(singleChannelBytes * 3) + ". Got : " + std::to_string(src.size) + " \n");
            return DecodeError::InvalidBinary;
        case 1:
        case 2:
        case 3:
            dst.kernelAttributes.numLocalIdChannels = static_cast<uint8_t>(tupleSize);
            break;
        }
        dst.kernelAttributes.localId[0] = tupleSize > 0;
        dst.kernelAttributes.localId[1] = tupleSize > 1;
        dst.kernelAttributes.localId[2] = tupleSize > 2;
        dst.kernelAttributes.perThreadDataSize = dst.kernelAttributes.simdSize;
        dst.kernelAttributes.perThreadDataSize *= sizeof(LocalIdT);
        dst.kernelAttributes.perThreadDataSize = alignUp(dst.kernelAttributes.perThreadDataSize, grfSize);
        dst.kernelAttributes.perThreadDataSize *= dst.kernelAttributes.numLocalIdChannels;
        break;
    }
    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypePackedLocalIds: {
        if (src.offset != 0) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Unhandled offset for argument of type " + NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadPayloadArgument::ArgType::packedLocalIds.str() + " in context of : " + dst.kernelMetadata.kernelName + ". Expected 0.\n");
            return DecodeError::InvalidBinary;
        }
        using LocalIdT = uint16_t;
        auto tupleSize = src.size / sizeof(LocalIdT);
        switch (tupleSize) {
        default:
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid size for argument of type " + NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadPayloadArgument::ArgType::packedLocalIds.str() + " in context of : " + dst.kernelMetadata.kernelName + ". Expected : " + std::to_string(sizeof(LocalIdT)) + " or " + std::to_string(sizeof(LocalIdT) * 2) + " or " + std::to_string(sizeof(LocalIdT) * 3) + ". Got : " + std::to_string(src.size) + " \n");
            return DecodeError::InvalidBinary;

        case 1:
        case 2:
        case 3:
            dst.kernelAttributes.numLocalIdChannels = static_cast<uint8_t>(tupleSize);
            break;
        }
        dst.kernelAttributes.localId[0] = tupleSize > 0;
        dst.kernelAttributes.localId[1] = tupleSize > 1;
        dst.kernelAttributes.localId[2] = tupleSize > 2;
        dst.kernelAttributes.simdSize = 1;
        dst.kernelAttributes.perThreadDataSize = dst.kernelAttributes.simdSize;
        dst.kernelAttributes.perThreadDataSize *= dst.kernelAttributes.numLocalIdChannels;
        dst.kernelAttributes.perThreadDataSize *= sizeof(LocalIdT);
        break;
    }
    }

    return DecodeError::Success;
}

NEO::DecodeError populateArgDescriptor(const NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::PayloadArgumentBaseT &src, NEO::KernelDescriptor &dst, uint32_t &crossThreadDataSize,
                                       std::string &outErrReason, std::string &outWarning) {
    crossThreadDataSize = std::max<uint32_t>(crossThreadDataSize, src.offset + src.size);

    auto &explicitArgs = dst.payloadMappings.explicitArgs;
    auto getVmeDescriptor = [&src, &dst]() {
        auto &argsExt = dst.payloadMappings.explicitArgsExtendedDescriptors;
        argsExt.resize(dst.payloadMappings.explicitArgs.size());
        if (argsExt[src.argIndex] == nullptr) {
            argsExt[src.argIndex] = std::make_unique<ArgDescVme>();
        }
        return static_cast<ArgDescVme *>(argsExt[src.argIndex].get());
    };

    switch (src.argType) {
    default:
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid arg type in cross thread data section in context of : " + dst.kernelMetadata.kernelName + ".\n");
        return DecodeError::InvalidBinary; // unsupported

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypePrivateBaseStateless: {
        dst.payloadMappings.implicitArgs.privateMemoryAddress.stateless = src.offset;
        dst.payloadMappings.implicitArgs.privateMemoryAddress.pointerSize = src.size;
        break;
    }

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeArgBypointer: {
        auto &arg = dst.payloadMappings.explicitArgs[src.argIndex];
        auto &argTraits = arg.getTraits();
        switch (src.addrspace) {
        default:
            UNRECOVERABLE_IF(NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AddressSpaceUnknown != src.addrspace);
            argTraits.addressQualifier = KernelArgMetadata::AddrUnknown;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(true);
            break;
        case NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AddressSpaceGlobal:
            argTraits.addressQualifier = KernelArgMetadata::AddrGlobal;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(true);
            break;
        case NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AddressSpaceLocal:
            argTraits.addressQualifier = KernelArgMetadata::AddrLocal;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(true);
            break;
        case NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AddressSpaceConstant:
            argTraits.addressQualifier = KernelArgMetadata::AddrConstant;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(true);
            break;
        case NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AddressSpaceImage: {
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescImage>(true);
            auto &extendedInfo = dst.payloadMappings.explicitArgs[src.argIndex].getExtendedTypeInfo();
            extendedInfo.isMediaImage = (src.imageType == NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::ImageType::ImageType2DMedia);
            extendedInfo.isMediaBlockImage = (src.imageType == NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::ImageType::ImageType2DMediaBlock);
            extendedInfo.isTransformable = src.imageTransformable;
            dst.kernelAttributes.flags.usesImages = true;
        } break;
        case NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AddressSpaceSampler: {
            using SamplerType = NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::SamplerType;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescSampler>(true);
            auto &extendedInfo = arg.getExtendedTypeInfo();
            extendedInfo.isAccelerator = (src.samplerType == SamplerType::SamplerTypeVME) ||
                                         (src.samplerType == SamplerType::SamplerTypeVE) ||
                                         (src.samplerType == SamplerType::SamplerTypeVD);
            const bool usesVme = src.samplerType == SamplerType::SamplerTypeVME;
            extendedInfo.hasVmeExtendedDescriptor = usesVme;
            dst.kernelAttributes.flags.usesVme = usesVme;
            dst.kernelAttributes.flags.usesSamplers = true;
        } break;
        }

        switch (src.accessType) {
        default:
            UNRECOVERABLE_IF(argTraits.accessQualifier != NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AccessTypeUnknown);
            argTraits.accessQualifier = KernelArgMetadata::AccessUnknown;
            break;
        case NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AccessTypeReadonly:
            argTraits.accessQualifier = KernelArgMetadata::AccessReadOnly;
            break;
        case NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AccessTypeReadwrite:
            argTraits.accessQualifier = KernelArgMetadata::AccessReadWrite;
            break;
        case NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::AccessTypeWriteonly:
            argTraits.accessQualifier = KernelArgMetadata::AccessWriteOnly;
            break;
        }

        argTraits.argByValSize = sizeof(void *);
        if (dst.payloadMappings.explicitArgs[src.argIndex].is<NEO::ArgDescriptor::ArgTPointer>()) {
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>().accessedUsingStatelessAddressingMode = false;
        }
        switch (src.addrmode) {
        default:
            outErrReason.append("Invalid or missing memory addressing mode for arg idx : " + std::to_string(src.argIndex) + " in context of : " + dst.kernelMetadata.kernelName + ".\n");
            return DecodeError::InvalidBinary;
        case NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::MemoryAddressingModeStateful:
            if (dst.payloadMappings.explicitArgs[src.argIndex].is<NEO::ArgDescriptor::ArgTSampler>()) {
                static constexpr auto maxSamplerStateSize = 16U;
                static constexpr auto maxIndirectSamplerStateSize = 64U;
                auto &sampler = dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescSampler>();
                sampler.bindful = maxIndirectSamplerStateSize + maxSamplerStateSize * src.samplerIndex;
            }
            break;
        case NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::MemoryAddressingModeStateless:
            if (false == dst.payloadMappings.explicitArgs[src.argIndex].is<NEO::ArgDescriptor::ArgTPointer>()) {
                outErrReason.append("Invalid or missing memory addressing " + NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::MemoryAddressingMode::stateless.str() + " for arg idx : " + std::to_string(src.argIndex) + " in context of : " + dst.kernelMetadata.kernelName + ".\n");
                return DecodeError::InvalidBinary;
            }
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(false).stateless = src.offset;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(false).pointerSize = src.size;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(false).accessedUsingStatelessAddressingMode = true;
            break;
        case NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::MemoryAddressingModeBindless:
            if (dst.payloadMappings.explicitArgs[src.argIndex].is<NEO::ArgDescriptor::ArgTPointer>()) {
                dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(false).bindless = src.offset;
            } else if (dst.payloadMappings.explicitArgs[src.argIndex].is<NEO::ArgDescriptor::ArgTImage>()) {
                dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescImage>(false).bindless = src.offset;
            } else {
                dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescSampler>(false).bindless = src.offset;
            }
            break;
        case NEO::Elf::ZebinKernelMetadata::Types::Kernel::PayloadArgument::MemoryAddressingModeSharedLocalMemory:
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(false).slmOffset = src.offset;
            dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(false).requiredSlmAlignment = src.slmArgAlignment;
            break;
        }
        break;
    }

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeArgByvalue: {
        auto &argAsValue = dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescValue>(true);
        ArgDescValue::Element valueElement;
        valueElement.sourceOffset = 0;
        if (src.sourceOffset != -1) {
            valueElement.sourceOffset = src.sourceOffset;
        } else if (argAsValue.elements.empty() == false) {
            outErrReason.append("Missing source offset value for element in argByValue\n");
            return DecodeError::InvalidBinary;
        }
        valueElement.offset = src.offset;
        valueElement.size = src.size;
        argAsValue.elements.push_back(valueElement);
        break;
    }

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeLocalSize: {
        using LocalSizeT = uint32_t;
        if (false == setVecArgIndicesBasedOnSize<LocalSizeT>(dst.payloadMappings.dispatchTraits.localWorkSize, src.size, src.offset)) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid size for argument of type " + NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::ArgType::localSize.str() + " in context of : " + dst.kernelMetadata.kernelName + ". Expected 4 or 8 or 12. Got : " + std::to_string(src.size) + "\n");
            return DecodeError::InvalidBinary;
        }
        break;
    }

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeGlobalIdOffset: {
        using GlovaIdOffsetT = uint32_t;
        if (false == setVecArgIndicesBasedOnSize<GlovaIdOffsetT>(dst.payloadMappings.dispatchTraits.globalWorkOffset, src.size, src.offset)) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid size for argument of type " + NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::ArgType::globalIdOffset.str() + " in context of : " + dst.kernelMetadata.kernelName + ". Expected 4 or 8 or 12. Got : " + std::to_string(src.size) + "\n");
            return DecodeError::InvalidBinary;
        }
        break;
    }
    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeGroupCount: {
        using GroupSizeT = uint32_t;
        if (false == setVecArgIndicesBasedOnSize<GroupSizeT>(dst.payloadMappings.dispatchTraits.numWorkGroups, src.size, src.offset)) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid size for argument of type " + NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::ArgType::groupCount.str() + " in context of : " + dst.kernelMetadata.kernelName + ". Expected 4 or 8 or 12. Got : " + std::to_string(src.size) + "\n");
            return DecodeError::InvalidBinary;
        }
        break;
    }
    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeGlobalSize: {
        using GroupSizeT = uint32_t;
        if (false == setVecArgIndicesBasedOnSize<GroupSizeT>(dst.payloadMappings.dispatchTraits.globalWorkSize, src.size, src.offset)) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid size for argument of type " + NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::ArgType::globalSize.str() + " in context of : " + dst.kernelMetadata.kernelName + ". Expected 4 or 8 or 12. Got : " + std::to_string(src.size) + "\n");
            return DecodeError::InvalidBinary;
        }
        break;
    }

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeEnqueuedLocalSize: {
        using GroupSizeT = uint32_t;
        if (false == setVecArgIndicesBasedOnSize<GroupSizeT>(dst.payloadMappings.dispatchTraits.enqueuedLocalWorkSize, src.size, src.offset)) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid size for argument of type " + NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::ArgType::enqueuedLocalSize.str() + " in context of : " + dst.kernelMetadata.kernelName + ". Expected 4 or 8 or 12. Got : " + std::to_string(src.size) + "\n");
            return DecodeError::InvalidBinary;
        }
        break;
    }

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeBufferAddress: {
        auto &argAsPtr = dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(true);
        argAsPtr.stateless = src.offset;
        argAsPtr.pointerSize = src.size;
        break;
    }

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeBufferOffset: {
        if (4 != src.size) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid size for argument of type " + NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::ArgType::bufferOffset.str() + " in context of : " + dst.kernelMetadata.kernelName + ". Expected 4. Got : " + std::to_string(src.size) + "\n");
            return DecodeError::InvalidBinary;
        }
        auto &argAsPointer = dst.payloadMappings.explicitArgs[src.argIndex].as<ArgDescPointer>(true);
        argAsPointer.bufferOffset = src.offset;
        break;
    }

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypePrintfBuffer: {
        dst.kernelAttributes.flags.usesPrintf = true;
        dst.payloadMappings.implicitArgs.printfSurfaceAddress.stateless = src.offset;
        dst.payloadMappings.implicitArgs.printfSurfaceAddress.pointerSize = src.size;
        break;
    }

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeWorkDimensions: {
        if (4 != src.size) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid size for argument of type " + NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PayloadArgument::ArgType::workDimensions.str() + " in context of : " + dst.kernelMetadata.kernelName + ". Expected 4. Got : " + std::to_string(src.size) + "\n");
            return DecodeError::InvalidBinary;
        }
        dst.payloadMappings.dispatchTraits.workDim = src.offset;
        break;
    }
    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeImplicitArgBuffer: {
        dst.payloadMappings.implicitArgs.implicitArgsBuffer = src.offset;
        dst.kernelAttributes.flags.requiresImplicitArgs = true;
        break;
    }

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeImageHeight:
        explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.imgHeight = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeImageWidth:
        explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.imgWidth = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeImageDepth:
        explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.imgDepth = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeImageChannelDataType:
        explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.channelDataType = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeImageChannelOrder:
        explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.channelOrder = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeImageArraySize:
        explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.arraySize = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeImageNumSamples:
        explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.numSamples = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeImageMipLevels:
        explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.numMipLevels = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeImageFlatBaseOffset:
        explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.flatBaseOffset = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeImageFlatWidth:
        explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.flatWidth = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeImageFlatHeight:
        explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.flatHeight = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeImageFlatPitch:
        explicitArgs[src.argIndex].as<ArgDescImage>(true).metadataPayload.flatPitch = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeSamplerAddrMode:
        explicitArgs[src.argIndex].as<ArgDescSampler>(true).metadataPayload.samplerAddressingMode = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeSamplerNormCoords:
        explicitArgs[src.argIndex].as<ArgDescSampler>(true).metadataPayload.samplerNormalizedCoords = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeSamplerSnapWa:
        explicitArgs[src.argIndex].as<ArgDescSampler>(true).metadataPayload.samplerSnapWa = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeVmeMbBlockType:
        getVmeDescriptor()->mbBlockType = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeVmeSubpixelMode:
        getVmeDescriptor()->subpixelMode = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeVmeSadAdjustMode:
        getVmeDescriptor()->sadAdjustMode = src.offset;
        break;

    case NEO::Elf::ZebinKernelMetadata::Types::Kernel::ArgTypeVmeSearchPathType:
        getVmeDescriptor()->searchPathType = src.offset;
        break;
    }
    return DecodeError::Success;
}

NEO::DecodeError populateInlineSamplers(const NEO::Elf::ZebinKernelMetadata::Types::Kernel::InlineSamplers::InlineSamplerBaseT &src, NEO::KernelDescriptor &dst, std::string &outErrReason, std::string &outWarning) {
    NEO::KernelDescriptor::InlineSampler inlineSampler = {};

    if (src.samplerIndex == -1) {
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid inline sampler index (must be >= 0) in context of : " + dst.kernelMetadata.kernelName + ".\n");
        return DecodeError::InvalidBinary;
    }
    inlineSampler.samplerIndex = src.samplerIndex;

    using AddrModeZeInfo = NEO::Elf::ZebinKernelMetadata::Types::Kernel::InlineSamplers::AddrModeT;
    using AddrModeDescriptor = NEO::KernelDescriptor::InlineSampler::AddrMode;
    constexpr LookupArray<AddrModeZeInfo, AddrModeDescriptor, 5> addrModes({{{AddrModeZeInfo::None, AddrModeDescriptor::None},
                                                                             {AddrModeZeInfo::Repeat, AddrModeDescriptor::Repeat},
                                                                             {AddrModeZeInfo::ClampEdge, AddrModeDescriptor::ClampEdge},
                                                                             {AddrModeZeInfo::ClampBorder, AddrModeDescriptor::ClampBorder},
                                                                             {AddrModeZeInfo::Mirror, AddrModeDescriptor::Mirror}}});
    auto addrMode = addrModes.find(src.addrMode);
    if (addrMode.has_value() == false) {
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid inline sampler addressing mode in context of : " + dst.kernelMetadata.kernelName + "\n");
        return DecodeError::InvalidBinary;
    }
    inlineSampler.addrMode = *addrMode;

    using FilterModeZeInfo = NEO::Elf::ZebinKernelMetadata::Types::Kernel::InlineSamplers::FilterModeT;
    using FilterModeDescriptor = NEO::KernelDescriptor::InlineSampler::FilterMode;
    constexpr LookupArray<FilterModeZeInfo, FilterModeDescriptor, 2> filterModes({{{FilterModeZeInfo::Nearest, FilterModeDescriptor::Nearest},
                                                                                   {FilterModeZeInfo::Linear, FilterModeDescriptor::Linear}}});
    auto filterMode = filterModes.find(src.filterMode);
    if (filterMode.has_value() == false) {
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid inline sampler filterMode mode in context of : " + dst.kernelMetadata.kernelName + "\n");
        return DecodeError::InvalidBinary;
    }
    inlineSampler.filterMode = *filterMode;

    inlineSampler.isNormalized = src.normalized;

    dst.inlineSamplers.push_back(inlineSampler);

    return DecodeError::Success;
}

NEO::DecodeError populateKernelDescriptor(const NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer::PerThreadMemoryBufferBaseT &src, NEO::KernelDescriptor &dst, uint32_t minScratchSpaceSize,
                                          std::string &outErrReason, std::string &outWarning) {
    using namespace NEO::Elf::ZebinKernelMetadata::Types::Kernel::PerThreadMemoryBuffer;
    using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadMemoryBuffer::AllocationType;
    using namespace NEO::Elf::ZebinKernelMetadata::Tags::Kernel::PerThreadMemoryBuffer::MemoryUsage;
    if (src.size <= 0) {
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid per-thread memory buffer allocation size (size must be greater than 0) in context of : " + dst.kernelMetadata.kernelName + ".\n");
        return DecodeError::InvalidBinary;
    }

    auto size = src.size;
    if (src.isSimtThread) {
        size *= dst.kernelAttributes.simdSize;
    }
    switch (src.allocationType) {
    default:
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid per-thread memory buffer allocation type in context of : " + dst.kernelMetadata.kernelName + ".\n");
        return DecodeError::InvalidBinary;
    case AllocationTypeGlobal:
        if (MemoryUsagePrivateSpace != src.memoryUsage) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid per-thread memory buffer memory usage type for " + global.str() + " allocation type in context of : " + dst.kernelMetadata.kernelName + ". Expected : " + privateSpace.str() + ".\n");
            return DecodeError::InvalidBinary;
        }

        dst.kernelAttributes.perHwThreadPrivateMemorySize = size;
        break;
    case AllocationTypeScratch:
        if (src.slot > 1) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid scratch buffer slot " + std::to_string(src.slot) + " in context of : " + dst.kernelMetadata.kernelName + ". Expected 0 or 1.\n");
            return DecodeError::InvalidBinary;
        }
        if (0 != dst.kernelAttributes.perThreadScratchSize[src.slot]) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Invalid duplicated scratch buffer entry " + std::to_string(src.slot) + " in context of : " + dst.kernelMetadata.kernelName + ".\n");
            return DecodeError::InvalidBinary;
        }
        uint32_t scratchSpaceSize = std::max(static_cast<uint32_t>(size), minScratchSpaceSize);
        scratchSpaceSize = Math::isPow2(scratchSpaceSize) ? scratchSpaceSize : Math::nextPowerOfTwo(scratchSpaceSize);
        dst.kernelAttributes.perThreadScratchSize[src.slot] = scratchSpaceSize;
        break;
    }
    return DecodeError::Success;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
NEO::DecodeError populateKernelDescriptor(NEO::ProgramInfo &dst, NEO::Elf::Elf<numBits> &elf, NEO::ZebinSections<numBits> &zebinSections,
                                          NEO::Yaml::YamlParser &yamlParser, const NEO::Yaml::Node &kernelNd, std::string &outErrReason, std::string &outWarning) {
    auto kernelInfo = std::make_unique<NEO::KernelInfo>();
    auto &kernelDescriptor = kernelInfo->kernelDescriptor;

    ZeInfoKernelSections zeInfokernelSections;
    extractZeInfoKernelSections(yamlParser, kernelNd, zeInfokernelSections, NEO::Elf::SectionsNamesZebin::zeInfo, outWarning);
    auto extractError = validateZeInfoKernelSectionsCount(zeInfokernelSections, outErrReason, outWarning);
    if (DecodeError::Success != extractError) {
        return extractError;
    }

    kernelDescriptor.kernelMetadata.kernelName = yamlParser.readValueNoQuotes(*zeInfokernelSections.nameNd[0]).str();

    NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExecutionEnvBaseT execEnv;
    auto execEnvErr = readZeInfoExecutionEnvironment(yamlParser, *zeInfokernelSections.executionEnvNd[0], execEnv, kernelInfo->kernelDescriptor.kernelMetadata.kernelName, outErrReason, outWarning);
    if (DecodeError::Success != execEnvErr) {
        return execEnvErr;
    }

    NEO::Elf::ZebinKernelMetadata::Types::Kernel::Attributes::AttributesBaseT attributes;
    if (false == zeInfokernelSections.attributesNd.empty()) {
        auto attributeErr = readZeInfoAttributes(yamlParser, *zeInfokernelSections.attributesNd[0], attributes, kernelInfo->kernelDescriptor.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != attributeErr) {
            return attributeErr;
        }
        populateKernelSourceAttributes(kernelDescriptor, attributes);
    }

    NEO::Elf::ZebinKernelMetadata::Types::Kernel::DebugEnv::DebugEnvBaseT debugEnv;
    if (false == zeInfokernelSections.debugEnvNd.empty()) {
        auto debugEnvErr = readZeInfoDebugEnvironment(yamlParser, *zeInfokernelSections.debugEnvNd[0], debugEnv, kernelInfo->kernelDescriptor.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != debugEnvErr) {
            return debugEnvErr;
        }

        if (debugEnv.debugSurfaceBTI == 0) {
            kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0U;
        }
    }

    ZeInfoPerThreadPayloadArguments perThreadPayloadArguments;
    if (false == zeInfokernelSections.perThreadPayloadArgumentsNd.empty()) {
        auto perThreadPayloadArgsErr = readZeInfoPerThreadPayloadArguments(yamlParser, *zeInfokernelSections.perThreadPayloadArgumentsNd[0], perThreadPayloadArguments,
                                                                           kernelDescriptor.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != perThreadPayloadArgsErr) {
            return perThreadPayloadArgsErr;
        }
    }

    int32_t maxArgumentIndex = -1;
    int32_t maxSamplerIndex = -1;
    ZeInfoPayloadArguments payloadArguments;
    if (false == zeInfokernelSections.payloadArgumentsNd.empty()) {
        auto payloadArgsErr = readZeInfoPayloadArguments(yamlParser, *zeInfokernelSections.payloadArgumentsNd[0], payloadArguments, maxArgumentIndex, maxSamplerIndex,
                                                         kernelDescriptor.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != payloadArgsErr) {
            return payloadArgsErr;
        }
    }

    ZeInfoInlineSamplers inlineSamplers;
    if (false == zeInfokernelSections.inlineSamplersNd.empty()) {
        auto decodeErr = readZeInfoInlineSamplers(yamlParser, *zeInfokernelSections.inlineSamplersNd[0], inlineSamplers, maxSamplerIndex,
                                                  kernelDescriptor.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != decodeErr) {
            return decodeErr;
        }
    }

    ZeInfoPerThreadMemoryBuffers perThreadMemoryBuffers;
    if (false == zeInfokernelSections.perThreadMemoryBuffersNd.empty()) {
        auto perThreadMemoryBuffersErr = readZeInfoPerThreadMemoryBuffers(yamlParser, *zeInfokernelSections.perThreadMemoryBuffersNd[0], perThreadMemoryBuffers,
                                                                          kernelDescriptor.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != perThreadMemoryBuffersErr) {
            return perThreadMemoryBuffersErr;
        }
    }

    NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ExperimentalPropertiesBaseT outExperimentalProperties;
    if (false == zeInfokernelSections.experimentalPropertiesNd.empty()) {
        auto experimentalPropertiesErr = readZeInfoExperimentalProperties(yamlParser, *zeInfokernelSections.experimentalPropertiesNd[0], outExperimentalProperties,
                                                                          kernelInfo->kernelDescriptor.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != experimentalPropertiesErr) {
            return experimentalPropertiesErr;
        }

        kernelDescriptor.kernelAttributes.hasNonKernelArgLoad = outExperimentalProperties.hasNonKernelArgLoad;
        kernelDescriptor.kernelAttributes.hasNonKernelArgStore = outExperimentalProperties.hasNonKernelArgStore;
        kernelDescriptor.kernelAttributes.hasNonKernelArgAtomic = outExperimentalProperties.hasNonKernelArgAtomic;
    }
    kernelDescriptor.kernelAttributes.binaryFormat = DeviceBinaryFormat::Zebin;

    kernelDescriptor.entryPoints.skipPerThreadDataLoad = execEnv.offsetToSkipPerThreadDataLoad;
    kernelDescriptor.entryPoints.skipSetFFIDGP = execEnv.offsetToSkipSetFfidGp;
    kernelDescriptor.kernelAttributes.flags.passInlineData = (execEnv.inlineDataPayloadSize != 0);
    kernelDescriptor.kernelAttributes.flags.requiresDisabledMidThreadPreemption = execEnv.disableMidThreadPreemption;
    kernelDescriptor.kernelAttributes.flags.requiresSubgroupIndependentForwardProgress = execEnv.subgroupIndependentForwardProgress;
    kernelDescriptor.kernelAttributes.flags.requiresDisabledEUFusion = execEnv.requireDisableEUFusion;
    kernelDescriptor.kernelAttributes.flags.useGlobalAtomics = execEnv.hasGlobalAtomics;
    kernelDescriptor.kernelAttributes.flags.useStackCalls = execEnv.hasStackCalls;
    kernelDescriptor.kernelAttributes.flags.usesFencesForReadWriteImages = execEnv.hasFenceForImageAccess;
    kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode = execEnv.hasDpas;
    kernelDescriptor.kernelAttributes.flags.usesStatelessWrites = (false == execEnv.hasNoStatelessWrite);
    kernelDescriptor.kernelAttributes.barrierCount = execEnv.barrierCount;
    kernelDescriptor.kernelAttributes.bufferAddressingMode = (execEnv.has4GBBuffers) ? KernelDescriptor::Stateless : KernelDescriptor::BindfulAndStateless;
    kernelDescriptor.kernelAttributes.inlineDataPayloadSize = static_cast<uint16_t>(execEnv.inlineDataPayloadSize);
    kernelDescriptor.kernelAttributes.numGrfRequired = execEnv.grfCount;
    kernelDescriptor.kernelAttributes.requiredWorkgroupSize[0] = static_cast<uint16_t>(execEnv.requiredWorkGroupSize[0]);
    kernelDescriptor.kernelAttributes.requiredWorkgroupSize[1] = static_cast<uint16_t>(execEnv.requiredWorkGroupSize[1]);
    kernelDescriptor.kernelAttributes.requiredWorkgroupSize[2] = static_cast<uint16_t>(execEnv.requiredWorkGroupSize[2]);
    kernelDescriptor.kernelAttributes.simdSize = execEnv.simdSize;
    kernelDescriptor.kernelAttributes.slmInlineSize = execEnv.slmSize;
    kernelDescriptor.kernelAttributes.workgroupWalkOrder[0] = static_cast<uint8_t>(execEnv.workgroupWalkOrderDimensions[0]);
    kernelDescriptor.kernelAttributes.workgroupWalkOrder[1] = static_cast<uint8_t>(execEnv.workgroupWalkOrderDimensions[1]);
    kernelDescriptor.kernelAttributes.workgroupWalkOrder[2] = static_cast<uint8_t>(execEnv.workgroupWalkOrderDimensions[2]);
    kernelDescriptor.kernelAttributes.hasIndirectStatelessAccess = (execEnv.indirectStatelessCount > 0);
    kernelDescriptor.kernelMetadata.requiredSubGroupSize = execEnv.requiredSubGroupSize;

    using ThreadSchedulingMode = NEO::Elf::ZebinKernelMetadata::Types::Kernel::ExecutionEnv::ThreadSchedulingMode;
    switch (execEnv.threadSchedulingMode) {
    default:
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
        break;
    case ThreadSchedulingMode::ThreadSchedulingModeAgeBased:
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::AgeBased;
        break;
    case ThreadSchedulingMode::ThreadSchedulingModeRoundRobin:
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;
        break;
    case ThreadSchedulingMode::ThreadSchedulingModeRoundRobinStall:
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobinAfterDependency;
        break;
    }

    if ((kernelDescriptor.kernelAttributes.simdSize != 1) && (kernelDescriptor.kernelAttributes.simdSize != 8) && (kernelDescriptor.kernelAttributes.simdSize != 16) && (kernelDescriptor.kernelAttributes.simdSize != 32)) {
        outErrReason.append("DeviceBinaryFormat::Zebin : Invalid simd size : " + std::to_string(kernelDescriptor.kernelAttributes.simdSize) + " in context of : " + kernelDescriptor.kernelMetadata.kernelName + ". Expected 1, 8, 16 or 32. Got : " + std::to_string(kernelDescriptor.kernelAttributes.simdSize) + "\n");
        return DecodeError::InvalidBinary;
    }

    for (const auto &arg : perThreadPayloadArguments) {
        auto decodeErr = populateArgDescriptor(arg, kernelDescriptor, dst.grfSize, outErrReason, outWarning);
        if (DecodeError::Success != decodeErr) {
            return decodeErr;
        }
    }

    if (!payloadArguments.empty()) {
        kernelDescriptor.payloadMappings.explicitArgs.resize(maxArgumentIndex + 1);
        kernelDescriptor.kernelAttributes.numArgsToPatch = maxArgumentIndex + 1;
    }

    uint32_t crossThreadDataSize = 0;
    for (const auto &arg : payloadArguments) {
        auto decodeErr = populateArgDescriptor(arg, kernelDescriptor, crossThreadDataSize, outErrReason, outWarning);
        if (DecodeError::Success != decodeErr) {
            return decodeErr;
        }
    }

    for (const auto &inlineSampler : inlineSamplers) {
        auto decodeErr = populateInlineSamplers(inlineSampler, kernelDescriptor, outErrReason, outWarning);
        if (DecodeError::Success != decodeErr) {
            return decodeErr;
        }
    }

    for (const auto &memBuff : perThreadMemoryBuffers) {
        auto decodeErr = populateKernelDescriptor(memBuff, kernelDescriptor, dst.minScratchSpaceSize, outErrReason, outWarning);
        if (DecodeError::Success != decodeErr) {
            return decodeErr;
        }
    }

    if (NEO::DebugManager.flags.ZebinAppendElws.get()) {
        kernelDescriptor.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[0] = alignDown(crossThreadDataSize + 12, 32);
        kernelDescriptor.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[1] = kernelDescriptor.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[0] + 4;
        kernelDescriptor.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[2] = kernelDescriptor.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[1] + 4;
        crossThreadDataSize = kernelDescriptor.payloadMappings.dispatchTraits.enqueuedLocalWorkSize[2] + 4;
    }
    kernelDescriptor.kernelAttributes.crossThreadDataSize = static_cast<uint16_t>(alignUp(crossThreadDataSize, 32));

    ZeInfoBindingTableIndices bindingTableIndices;
    ZeInfoBindingTableIndices::value_type maximumBindingTableEntry;
    if (false == zeInfokernelSections.bindingTableIndicesNd.empty()) {
        auto btisErr = readZeInfoBindingTableIndices(yamlParser, *zeInfokernelSections.bindingTableIndicesNd[0], bindingTableIndices, maximumBindingTableEntry,
                                                     kernelDescriptor.kernelMetadata.kernelName, outErrReason, outWarning);
        if (DecodeError::Success != btisErr) {
            return btisErr;
        }
    }

    auto generatedSshPos = kernelDescriptor.generatedHeaps.size();
    uint32_t generatedSshSize = 0U;
    if (false == bindingTableIndices.empty() ||
        NEO::isValidOffset(kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful)) {
        static constexpr auto maxSurfaceStateSize = 64U;
        static constexpr auto btiSize = sizeof(int);
        auto numEntries = maximumBindingTableEntry.btiValue + 1;
        kernelDescriptor.generatedHeaps.resize(alignUp(generatedSshPos, maxSurfaceStateSize), 0U);
        generatedSshPos = kernelInfo->kernelDescriptor.generatedHeaps.size();

        // make room for surface states
        kernelDescriptor.generatedHeaps.resize(generatedSshPos + numEntries * maxSurfaceStateSize, 0U);

        auto generatedBindingTablePos = kernelDescriptor.generatedHeaps.size();
        kernelDescriptor.generatedHeaps.resize(generatedBindingTablePos + numEntries * btiSize, 0U);

        auto bindingTableIt = reinterpret_cast<int *>(kernelDescriptor.generatedHeaps.data() + generatedBindingTablePos);
        for (int i = 0; i < numEntries; ++i) {
            *bindingTableIt = i * maxSurfaceStateSize;
            ++bindingTableIt;
        }

        for (auto &bti : bindingTableIndices) {
            auto &explicitArg = kernelDescriptor.payloadMappings.explicitArgs[bti.argIndex];
            switch (explicitArg.type) {
            default:
                outErrReason.append("DeviceBinaryFormat::Zebin::.ze_info : Invalid binding table entry for non-pointer and non-image argument idx : " + std::to_string(bti.argIndex) + ".\n");
                return DecodeError::InvalidBinary;
            case ArgDescriptor::ArgTImage: {
                explicitArg.as<ArgDescImage>().bindful = bti.btiValue * maxSurfaceStateSize;
                break;
            }
            case ArgDescriptor::ArgTPointer: {
                explicitArg.as<ArgDescPointer>().bindful = bti.btiValue * maxSurfaceStateSize;
                break;
            }
            }
        }
        kernelDescriptor.generatedHeaps.resize(alignUp(kernelDescriptor.generatedHeaps.size(), maxSurfaceStateSize), 0U);
        generatedSshSize = static_cast<uint32_t>(kernelDescriptor.generatedHeaps.size() - generatedSshPos);

        kernelDescriptor.payloadMappings.bindingTable.numEntries = numEntries;
        kernelDescriptor.payloadMappings.bindingTable.tableOffset = static_cast<SurfaceStateHeapOffset>(generatedBindingTablePos - generatedSshPos);
    }

    auto generatedDshPos = kernelDescriptor.generatedHeaps.size();
    uint32_t generatedDshSize = 0U;
    if (maxSamplerIndex >= 0) {
        constexpr auto maxSamplerStateSize = 16U;
        constexpr auto maxIndirectSamplerStateSize = 64U;

        kernelDescriptor.kernelAttributes.flags.usesSamplers = true;
        auto &samplerTable = kernelDescriptor.payloadMappings.samplerTable;

        samplerTable.borderColor = 0U;
        samplerTable.tableOffset = maxIndirectSamplerStateSize;
        samplerTable.numSamplers = maxSamplerIndex + 1;

        generatedDshSize = maxIndirectSamplerStateSize + kernelDescriptor.payloadMappings.samplerTable.numSamplers * maxSamplerStateSize;
        generatedDshSize = alignUp(generatedDshSize, MemoryConstants::cacheLineSize);
        kernelDescriptor.generatedHeaps.resize(kernelDescriptor.generatedHeaps.size() + generatedDshSize);
    }

    typename ZebinSections<numBits>::SectionHeaderData *correspondingTextSegment = nullptr;
    auto sectionHeaderNamesData = elf.sectionHeaders[elf.elfFileHeader->shStrNdx].data;
    ConstStringRef sectionHeaderNamesString(reinterpret_cast<const char *>(sectionHeaderNamesData.begin()), sectionHeaderNamesData.size());
    for (auto *textSection : zebinSections.textKernelSections) {
        ConstStringRef sectionName = ConstStringRef(sectionHeaderNamesString.begin() + textSection->header->name);
        auto sufix = sectionName.substr(static_cast<int>(NEO::Elf::SectionsNamesZebin::textPrefix.length()));
        if (sufix == kernelDescriptor.kernelMetadata.kernelName) {
            correspondingTextSegment = textSection;
        }
    }

    if (nullptr == correspondingTextSegment) {
        outErrReason.append("DeviceBinaryFormat::Zebin : Could not find text section for kernel " + kernelDescriptor.kernelMetadata.kernelName + "\n");
        return DecodeError::InvalidBinary;
    }

    kernelInfo->heapInfo.pKernelHeap = correspondingTextSegment->data.begin();
    kernelInfo->heapInfo.KernelHeapSize = static_cast<uint32_t>(correspondingTextSegment->data.size());
    kernelInfo->heapInfo.KernelUnpaddedSize = static_cast<uint32_t>(correspondingTextSegment->data.size());
    kernelInfo->heapInfo.pSsh = kernelDescriptor.generatedHeaps.data() + generatedSshPos;
    kernelInfo->heapInfo.SurfaceStateHeapSize = generatedSshSize;
    kernelInfo->heapInfo.pDsh = kernelDescriptor.generatedHeaps.data() + generatedDshPos;
    kernelInfo->heapInfo.DynamicStateHeapSize = generatedDshSize;

    dst.kernelInfos.push_back(kernelInfo.release());
    return DecodeError::Success;
}

NEO::DecodeError readZeInfoVersionFromZeInfo(NEO::Elf::ZebinKernelMetadata::Types::Version &dst,
                                             NEO::Yaml::YamlParser &yamlParser, const NEO::Yaml::Node &versionNd, std::string &outErrReason, std::string &outWarning) {
    if (nullptr == yamlParser.getValueToken(versionNd)) {
        outErrReason.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Invalid version format - expected \'MAJOR.MINOR\' string\n");
        return NEO::DecodeError::InvalidBinary;
    }
    auto versionStr = yamlParser.readValueNoQuotes(versionNd);
    return populateZeInfoVersion(dst, versionStr, outErrReason);
}

NEO::DecodeError populateZeInfoVersion(NEO::Elf::ZebinKernelMetadata::Types::Version &dst, ConstStringRef &versionStr, std::string &outErrReason) {
    StackVec<char, 32> nullTerminated{versionStr.begin(), versionStr.end()};
    nullTerminated.push_back('\0');
    auto separator = std::find(nullTerminated.begin(), nullTerminated.end(), '.');
    if ((nullTerminated.end() == separator) || (nullTerminated.begin() == separator) || (&*nullTerminated.rbegin() == separator + 1)) {
        outErrReason.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Invalid version format - expected 'MAJOR.MINOR' string, got : " + std::string{versionStr} + "\n");
        return NEO::DecodeError::InvalidBinary;
    }
    *separator = 0;
    dst.major = atoi(nullTerminated.begin());
    dst.minor = atoi(separator + 1);
    return NEO::DecodeError::Success;
}

NEO::DecodeError populateExternalFunctionsMetadata(NEO::ProgramInfo &dst, NEO::Yaml::YamlParser &yamlParser, const NEO::Yaml::Node &functionNd, std::string &outErrReason, std::string &outWarning) {
    ConstStringRef functionName;
    NEO::Elf::ZebinKernelMetadata::Types::Function::ExecutionEnv::ExecutionEnvBaseT execEnv = {};
    bool isValid = true;

    for (const auto &functionMetadataNd : yamlParser.createChildrenRange(functionNd)) {
        auto key = yamlParser.readKey(functionMetadataNd);
        if (NEO::Elf::ZebinKernelMetadata::Tags::Function::name == key) {
            functionName = yamlParser.readValueNoQuotes(functionMetadataNd);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::Function::executionEnv == key) {
            auto execEnvErr = readZeInfoExecutionEnvironment(yamlParser, functionMetadataNd, execEnv, "external functions", outErrReason, outWarning);
            if (execEnvErr != DecodeError::Success) {
                isValid = false;
            }
        } else {
            outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Unknown entry \"" + yamlParser.readKey(functionMetadataNd).str() + "\" in context of : external functions\n");
        }
    }

    if (isValid) {
        NEO::ExternalFunctionInfo extFunInfo;
        extFunInfo.functionName = functionName.str();
        extFunInfo.barrierCount = static_cast<uint8_t>(execEnv.barrierCount);
        extFunInfo.numGrfRequired = static_cast<uint16_t>(execEnv.grfCount);
        extFunInfo.simdSize = static_cast<uint8_t>(execEnv.simdSize);
        dst.externalFunctions.push_back(extFunInfo);
        return DecodeError::Success;
    } else {
        return DecodeError::InvalidBinary;
    }
}

std::string attributeToString(const int32_t &attribute) {
    return std::to_string(attribute);
}
std::string attributeToString(const std::array<int32_t, 3> &attribute) {
    return std::to_string(attribute[0]) + ", " + std::to_string(attribute[1]) + ", " + std::to_string(attribute[2]);
}
std::string attributeToString(ConstStringRef attribute) {
    return attribute.str();
}
void appendAttribute(std::string &dst, const std::string &attributeName, const std::string &attributeValue) {
    if (dst.empty() == false) {
        dst.append(" ");
    }
    dst.append(attributeName + "(" + attributeValue + ")");
}
template <typename T>
void appendAttributeIfSet(std::string &dst, ConstStringRef attributeName, std::optional<T> &attributeValue) {
    if (attributeValue) {
        appendAttribute(dst, attributeName.str(), attributeToString(*attributeValue));
    }
}

NEO::DecodeError populateKernelSourceAttributes(NEO::KernelDescriptor &dst, NEO::Elf::ZebinKernelMetadata::Types::Kernel::Attributes::AttributesBaseT &attributes) {
    namespace AttributeTags = NEO::Elf::ZebinKernelMetadata::Tags::Kernel::Attributes;
    namespace AttributeTypes = NEO::Elf::ZebinKernelMetadata::Types::Kernel::Attributes;
    auto &languageAttributes = dst.kernelMetadata.kernelLanguageAttributes;

    for (auto &hint : attributes.otherHints) {
        appendAttribute(languageAttributes, hint.first.str(), hint.second.str());
    }

    appendAttributeIfSet(languageAttributes, AttributeTags::intelReqdSubgroupSize, attributes.intelReqdSubgroupSize);
    appendAttributeIfSet(languageAttributes, AttributeTags::intelReqdWorkgroupWalkOrder, attributes.intelReqdWorkgroupWalkOrder);
    appendAttributeIfSet(languageAttributes, AttributeTags::reqdWorkgroupSize, attributes.reqdWorkgroupSize);
    appendAttributeIfSet(languageAttributes, AttributeTags::workgroupSizeHint, attributes.workgroupSizeHint);
    appendAttributeIfSet(languageAttributes, AttributeTags::vecTypeHint, attributes.vecTypeHint);
    appendAttributeIfSet(languageAttributes, AttributeTags::invalidKernel, attributes.invalidKernel);
    dst.kernelAttributes.flags.isInvalid = attributes.invalidKernel.has_value();

    return DecodeError::Success;
}

NEO::DecodeError readKernelMiscArgumentInfos(const NEO::Yaml::YamlParser &parser, const NEO::Yaml::Node &node, KernelMiscArgInfos &kernelMiscArgInfosVec, std::string &outErrReason, std::string &outWarning) {
    bool validArgInfo = true;
    for (const auto &argInfoMemberNode : parser.createChildrenRange(node)) {
        kernelMiscArgInfosVec.resize(kernelMiscArgInfosVec.size() + 1);
        auto &metadataExtended = *kernelMiscArgInfosVec.rbegin();
        for (const auto &singleArgInfoMember : parser.createChildrenRange(argInfoMemberNode)) {
            auto key = parser.readKey(singleArgInfoMember);
            if (key == Elf::ZebinKernelMetadata::Tags::KernelMiscInfo::ArgsInfo::name) {
                validArgInfo &= readZeInfoValueChecked(parser, singleArgInfoMember, metadataExtended.argName, Elf::ZebinKernelMetadata::Tags::kernelMiscInfo, outErrReason);
            } else if (key == Elf::ZebinKernelMetadata::Tags::KernelMiscInfo::ArgsInfo::accessQualifier) {
                validArgInfo &= readZeInfoValueChecked(parser, singleArgInfoMember, metadataExtended.accessQualifier, Elf::ZebinKernelMetadata::Tags::kernelMiscInfo, outErrReason);
            } else if (key == Elf::ZebinKernelMetadata::Tags::KernelMiscInfo::ArgsInfo::addressQualifier) {
                validArgInfo &= readZeInfoValueChecked(parser, singleArgInfoMember, metadataExtended.addressQualifier, Elf::ZebinKernelMetadata::Tags::kernelMiscInfo, outErrReason);
            } else if (key == Elf::ZebinKernelMetadata::Tags::KernelMiscInfo::ArgsInfo::index) {
                validArgInfo &= parser.readValueChecked(singleArgInfoMember, metadataExtended.index);
            } else if (key == Elf::ZebinKernelMetadata::Tags::KernelMiscInfo::ArgsInfo::typeName) {
                validArgInfo &= readZeInfoValueChecked(parser, singleArgInfoMember, metadataExtended.typeName, Elf::ZebinKernelMetadata::Tags::kernelMiscInfo, outErrReason);
            } else if (key == Elf::ZebinKernelMetadata::Tags::KernelMiscInfo::ArgsInfo::typeQualifiers) {
                validArgInfo &= readZeInfoValueChecked(parser, singleArgInfoMember, metadataExtended.typeQualifiers, Elf::ZebinKernelMetadata::Tags::kernelMiscInfo, outErrReason);
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin : KernelMiscInfo : Unrecognized argsInfo member " + key.str() + "\n");
            }
        }
    }
    return validArgInfo ? DecodeError::Success : DecodeError::InvalidBinary;
}

void populateKernelMiscInfo(KernelDescriptor &dst, KernelMiscArgInfos &kernelMiscArgInfosVec, std::string &outErrReason, std::string &outWarning) {
    auto populateIfNotEmpty = [](std::string &src, std::string &dst, ConstStringRef context, std::string &warnings) {
        if (false == src.empty()) {
            dst = std::move(src);
        } else {
            warnings.append("DeviceBinaryFormat::Zebin : KernelMiscInfo : ArgInfo member \"" + context.str() + "\" missing. Ignoring.\n");
        }
    };

    dst.explicitArgsExtendedMetadata.resize(kernelMiscArgInfosVec.size());
    for (auto &srcMetadata : kernelMiscArgInfosVec) {
        ArgTypeMetadataExtended dstMetadata;
        populateIfNotEmpty(srcMetadata.argName, dstMetadata.argName, Elf::ZebinKernelMetadata::Tags::KernelMiscInfo::ArgsInfo::name, outWarning);
        populateIfNotEmpty(srcMetadata.accessQualifier, dstMetadata.accessQualifier, Elf::ZebinKernelMetadata::Tags::KernelMiscInfo::ArgsInfo::accessQualifier, outWarning);
        populateIfNotEmpty(srcMetadata.addressQualifier, dstMetadata.addressQualifier, Elf::ZebinKernelMetadata::Tags::KernelMiscInfo::ArgsInfo::addressQualifier, outWarning);
        populateIfNotEmpty(srcMetadata.typeName, dstMetadata.type, Elf::ZebinKernelMetadata::Tags::KernelMiscInfo::ArgsInfo::typeName, outWarning);
        populateIfNotEmpty(srcMetadata.typeQualifiers, dstMetadata.typeQualifiers, Elf::ZebinKernelMetadata::Tags::KernelMiscInfo::ArgsInfo::typeQualifiers, outWarning);

        dst.explicitArgsExtendedMetadata.at(srcMetadata.index) = std::move(dstMetadata);
    }
}

NEO::DecodeError decodeAndPopulateKernelMiscInfo(ProgramInfo &dst, ConstStringRef metadataString, std::string &outErrReason, std::string &outWarning) {
    if (std::string::npos == dst.kernelMiscInfoPos) {
        outErrReason.append("DeviceBinaryFormat::Zebin : Position of " + Elf::ZebinKernelMetadata::Tags::kernelMiscInfo.str() + " not set - may be missing in zeInfo.\n");
        return DecodeError::InvalidBinary;
    }
    ConstStringRef kernelMiscInfoString(reinterpret_cast<const char *>(metadataString.begin() + dst.kernelMiscInfoPos), metadataString.size() - dst.kernelMiscInfoPos);
    NEO::KernelInfo *kernelInfo = nullptr;

    NEO::Yaml::YamlParser parser;
    bool parseSuccess = parser.parse(kernelMiscInfoString, outErrReason, outWarning);
    if (false == parseSuccess) {
        return DecodeError::InvalidBinary;
    }

    auto kernelMiscInfoSectionNode = parser.createChildrenRange(*parser.getRoot());
    auto validMetadata = true;
    using KernelArgsMiscInfoVec = std::vector<std::pair<std::string, KernelMiscArgInfos>>;
    KernelArgsMiscInfoVec kernelArgsMiscInfoVec;

    for (const auto &kernelMiscInfoNode : parser.createChildrenRange(*kernelMiscInfoSectionNode.begin())) {
        std::string kernelName{};
        KernelMiscArgInfos miscArgInfosVec;
        for (const auto &kernelMiscInfoNodeMetadata : parser.createChildrenRange(kernelMiscInfoNode)) {
            auto key = parser.readKey(kernelMiscInfoNodeMetadata);
            if (key == Elf::ZebinKernelMetadata::Tags::KernelMiscInfo::name) {
                validMetadata &= readZeInfoValueChecked(parser, kernelMiscInfoNodeMetadata, kernelName, Elf::ZebinKernelMetadata::Tags::kernelMiscInfo, outErrReason);
            } else if (key == Elf::ZebinKernelMetadata::Tags::KernelMiscInfo::argsInfo) {
                validMetadata &= (DecodeError::Success == readKernelMiscArgumentInfos(parser, kernelMiscInfoNodeMetadata, miscArgInfosVec, outErrReason, outWarning));
            } else {
                outWarning.append("DeviceBinaryFormat::Zebin : Unrecognized entry: " + key.str() + " in " + Elf::ZebinKernelMetadata::Tags::kernelMiscInfo.str() + " zeInfo's section.\n");
            }
        }
        if (kernelName.empty()) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Error : Missing kernel name in " + Elf::ZebinKernelMetadata::Tags::kernelMiscInfo.str() + " section.\n");
            validMetadata = false;
        }
        kernelArgsMiscInfoVec.emplace_back(std::make_pair(std::move(kernelName), miscArgInfosVec));
    }
    if (false == validMetadata) {
        return DecodeError::InvalidBinary;
    }
    for (auto &[kName, miscInfos] : kernelArgsMiscInfoVec) {
        for (auto dstKernelInfo : dst.kernelInfos) {
            if (dstKernelInfo->kernelDescriptor.kernelMetadata.kernelName == kName) {
                kernelInfo = dstKernelInfo;
                break;
            }
        }
        if (nullptr == kernelInfo) {
            outErrReason.append("DeviceBinaryFormat::Zebin : Error : Cannot find kernel info for kernel " + kName + ".\n");
            return DecodeError::InvalidBinary;
        }
        populateKernelMiscInfo(kernelInfo->kernelDescriptor, miscInfos, outErrReason, outWarning);
    }
    return DecodeError::Success;
}

template NEO::DecodeError decodeZebin<Elf::EI_CLASS_32>(ProgramInfo &dst, NEO::Elf::Elf<Elf::EI_CLASS_32> &elf, std::string &outErrReason, std::string &outWarning);
template NEO::DecodeError decodeZebin<Elf::EI_CLASS_64>(ProgramInfo &dst, NEO::Elf::Elf<Elf::EI_CLASS_64> &elf, std::string &outErrReason, std::string &outWarning);
template <Elf::ELF_IDENTIFIER_CLASS numBits>
NEO::DecodeError decodeZebin(ProgramInfo &dst, NEO::Elf::Elf<numBits> &elf, std::string &outErrReason, std::string &outWarning) {
    ZebinSections<numBits> zebinSections;
    auto extractError = extractZebinSections(elf, zebinSections, outErrReason, outWarning);
    if (DecodeError::Success != extractError) {
        return extractError;
    }

    extractError = validateZebinSectionsCount(zebinSections, outErrReason, outWarning);
    if (DecodeError::Success != extractError) {
        return extractError;
    }

    if (false == zebinSections.globalDataSections.empty()) {
        dst.globalVariables.initData = zebinSections.globalDataSections[0]->data.begin();
        dst.globalVariables.size = zebinSections.globalDataSections[0]->data.size();
    }

    if (false == zebinSections.constDataSections.empty()) {
        dst.globalConstants.initData = zebinSections.constDataSections[0]->data.begin();
        dst.globalConstants.size = zebinSections.constDataSections[0]->data.size();
    }

    if (false == zebinSections.constDataStringSections.empty()) {
        dst.globalStrings.initData = zebinSections.constDataStringSections[0]->data.begin();
        dst.globalStrings.size = zebinSections.constDataStringSections[0]->data.size();
    }

    if (false == zebinSections.symtabSections.empty()) {
        outWarning.append("DeviceBinaryFormat::Zebin : Ignoring symbol table\n");
    }

    if (zebinSections.zeInfoSections.empty()) {
        outWarning.append("DeviceBinaryFormat::Zebin : Expected at least one " + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " section, got 0\n");
        return DecodeError::Success;
    }

    auto metadataSectionData = zebinSections.zeInfoSections[0]->data;

    ConstStringRef metadataString(reinterpret_cast<const char *>(metadataSectionData.begin()), metadataSectionData.size());
    setKernelMiscInfoPosition(metadataString, dst);
    if (std::string::npos != dst.kernelMiscInfoPos) {
        metadataString = metadataString.substr(static_cast<size_t>(0), dst.kernelMiscInfoPos);
    }

    NEO::Yaml::YamlParser yamlParser;
    bool parseSuccess = yamlParser.parse(metadataString, outErrReason, outWarning);
    if (false == parseSuccess) {
        return DecodeError::InvalidBinary;
    }

    if (yamlParser.empty()) {
        outWarning.append("DeviceBinaryFormat::Zebin : Empty kernels metadata section (" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + ")\n");
        return DecodeError::Success;
    }

    UniqueNode kernelsSectionNodes;
    UniqueNode versionSectionNodes;
    UniqueNode globalHostAccessTableNodes;
    UniqueNode functionsSectionNodes;
    for (const auto &globalScopeNd : yamlParser.createChildrenRange(*yamlParser.getRoot())) {
        auto key = yamlParser.readKey(globalScopeNd);
        if (NEO::Elf::ZebinKernelMetadata::Tags::kernels == key) {
            kernelsSectionNodes.push_back(&globalScopeNd);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::version == key) {
            versionSectionNodes.push_back(&globalScopeNd);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::globalHostAccessTable == key) {
            globalHostAccessTableNodes.push_back(&globalScopeNd);
        } else if (NEO::Elf::ZebinKernelMetadata::Tags::functions == key) {
            functionsSectionNodes.push_back(&globalScopeNd);
        } else {
            outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Unknown entry \"" + yamlParser.readKey(globalScopeNd).str() + "\" in global scope of " + NEO::Elf::SectionsNamesZebin::zeInfo.str() + "\n");
        }
    }

    if (versionSectionNodes.size() > 1U) {
        outErrReason.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Expected at most one " + NEO::Elf::ZebinKernelMetadata::Tags::version.str() + " entry in global scope of " + NEO::Elf::SectionsNamesZebin::zeInfo.str() + ", got : " + std::to_string(versionSectionNodes.size()) + "\n");
        return DecodeError::InvalidBinary;
    }

    NEO::Elf::ZebinKernelMetadata::Types::Version zeInfoVersion = zeInfoDecoderVersion;
    if (versionSectionNodes.empty()) {
        outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : No version info provided (i.e. no " + NEO::Elf::ZebinKernelMetadata::Tags::version.str() + " entry in global scope of DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + ") - will use decoder's default : \'" + std::to_string(zeInfoDecoderVersion.major) + "." + std::to_string(zeInfoDecoderVersion.minor) + "\'\n");
        zeInfoVersion = zeInfoDecoderVersion;
    } else {
        auto zeInfoErr = readZeInfoVersionFromZeInfo(zeInfoVersion, yamlParser, *versionSectionNodes[0], outErrReason, outWarning);
        if (DecodeError::Success != zeInfoErr) {
            return zeInfoErr;
        }
    }

    auto zeInfoVersionError = validateZeInfoVersion(zeInfoVersion, outErrReason, outWarning);
    if (DecodeError::Success != zeInfoVersionError) {
        return zeInfoVersionError;
    }

    if (kernelsSectionNodes.size() > 1U) {
        outErrReason.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Expected at most one " + NEO::Elf::ZebinKernelMetadata::Tags::kernels.str() + " entry in global scope of " + NEO::Elf::SectionsNamesZebin::zeInfo.str() + ", got : " + std::to_string(kernelsSectionNodes.size()) + "\n");
        return DecodeError::InvalidBinary;
    }

    if (kernelsSectionNodes.empty()) {
        outWarning.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Expected one " + NEO::Elf::ZebinKernelMetadata::Tags::kernels.str() + " entry in global scope of " + NEO::Elf::SectionsNamesZebin::zeInfo.str() + ", got : " + std::to_string(kernelsSectionNodes.size()) + "\n");
        return DecodeError::Success;
    }

    for (const auto &kernelNd : yamlParser.createChildrenRange(*kernelsSectionNodes[0])) {
        auto zeInfoErr = populateKernelDescriptor(dst, elf, zebinSections, yamlParser, kernelNd, outErrReason, outWarning);
        if (DecodeError::Success != zeInfoErr) {
            return zeInfoErr;
        }
    }

    if (false == globalHostAccessTableNodes.empty()) {
        ZeInfoGlobalHostAccessTables globalHostAccessMapping;
        auto zeInfoErr = readZeInfoGlobalHostAceessTable(yamlParser, *globalHostAccessTableNodes[0], globalHostAccessMapping, "globalHostAccessTable", outErrReason, outWarning);
        if (DecodeError::Success != zeInfoErr) {
            return zeInfoErr;
        }
        dst.globalsDeviceToHostNameMap.reserve(globalHostAccessMapping.size());
        for (auto it = globalHostAccessMapping.begin(); it != globalHostAccessMapping.end(); it++) {
            dst.globalsDeviceToHostNameMap[it->deviceName] = it->hostName;
        }
    }

    if (functionsSectionNodes.size() > 1U) {
        outErrReason.append("DeviceBinaryFormat::Zebin::" + NEO::Elf::SectionsNamesZebin::zeInfo.str() + " : Expected at most one " + NEO::Elf::ZebinKernelMetadata::Tags::functions.str() + " entry in global scope of " + NEO::Elf::SectionsNamesZebin::zeInfo.str() + ", got : " + std::to_string(functionsSectionNodes.size()) + "\n");
        return DecodeError::InvalidBinary;
    }

    if (false == functionsSectionNodes.empty()) {
        for (const auto &functionNd : yamlParser.createChildrenRange(*functionsSectionNodes[0])) {
            auto zeInfoErr = populateExternalFunctionsMetadata(dst, yamlParser, functionNd, outErrReason, outWarning);
            if (DecodeError::Success != zeInfoErr) {
                return zeInfoErr;
            }
        }
    }

    return DecodeError::Success;
}

} // namespace NEO
