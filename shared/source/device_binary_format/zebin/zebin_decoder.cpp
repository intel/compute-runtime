/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device_binary_format/zebin/zebin_decoder.h"

#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/zebin/zebin_elf.h"
#include "shared/source/device_binary_format/zebin/zeinfo_decoder.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/program/kernel_info.h"
#include "shared/source/program/program_info.h"
#include "shared/source/utilities/logger.h"

#include "neo_aot_platforms.h"

namespace NEO {
template <>
bool isDeviceBinaryFormat<NEO::DeviceBinaryFormat::zebin>(const ArrayRef<const uint8_t> binary) {
    return Zebin::isZebin<Elf::EI_CLASS_64>(binary) || Zebin::isZebin<Elf::EI_CLASS_32>(binary);
};

namespace Zebin {

void setKernelMiscInfoPosition(ConstStringRef metadata, NEO::ProgramInfo &dst) {
    dst.kernelMiscInfoPos = metadata.str().find(ZeInfo::Tags::kernelMiscInfo.str());
}

template bool isZebin<Elf::EI_CLASS_32>(ArrayRef<const uint8_t> binary);
template bool isZebin<Elf::EI_CLASS_64>(ArrayRef<const uint8_t> binary);
template <Elf::ElfIdentifierClass numBits>
bool isZebin(ArrayRef<const uint8_t> binary) {
    auto fileHeader = Elf::decodeElfFileHeader<numBits>(binary);
    return fileHeader != nullptr &&
           (fileHeader->type == Elf::ET_REL ||
            fileHeader->type == Elf::ET_ZEBIN_EXE);
}

bool isTargetProductConfigCompatibleWithProductConfig(const AOT::PRODUCT_CONFIG &targetDeviceProductConfig,
                                                      const AOT::PRODUCT_CONFIG &productConfig) {
    auto compatProdConfPairItr = AOT::getCompatibilityMapping().find(productConfig);
    if (compatProdConfPairItr != AOT::getCompatibilityMapping().end()) {
        for (auto &compatibleConfig : compatProdConfPairItr->second)
            if (targetDeviceProductConfig == compatibleConfig)
                return true;
    }
    return false;
}

bool validateTargetDevice(const TargetDevice &targetDevice, Elf::ElfIdentifierClass numBits, PRODUCT_FAMILY productFamily, GFXCORE_FAMILY gfxCore, AOT::PRODUCT_CONFIG productConfig, Elf::ZebinTargetFlags targetMetadata) {
    if (debugManager.flags.ForceCompatibilityMode.get()) {
        return true;
    }
    if (targetDevice.maxPointerSizeInBytes == 4 && static_cast<uint32_t>(numBits == Elf::EI_CLASS_64)) {
        return false;
    }

    if (productConfig != AOT::UNKNOWN_ISA) {
        auto targetDeviceProductConfig = static_cast<AOT::PRODUCT_CONFIG>(targetDevice.aotConfig.value);
        if (targetDeviceProductConfig == productConfig)
            return true;
        else if (debugManager.flags.EnableCompatibilityMode.get() == true) {
            return isTargetProductConfigCompatibleWithProductConfig(targetDeviceProductConfig, productConfig);
        } else
            return false;
    }

    if (gfxCore == IGFX_UNKNOWN_CORE && productFamily == IGFX_UNKNOWN) {
        return false;
    }

    if (gfxCore != IGFX_UNKNOWN_CORE) {
        if (targetDevice.coreFamily != gfxCore) {
            return false;
        }
    }
    if (productFamily != IGFX_UNKNOWN) {
        if (targetDevice.productFamily != productFamily) {
            return false;
        }
    }
    if (targetMetadata.validateRevisionId) {
        bool isValidStepping = (targetDevice.stepping >= targetMetadata.minHwRevisionId) && (targetDevice.stepping <= targetMetadata.maxHwRevisionId);
        if (false == isValidStepping) {
            return false;
        }
    }
    return true;
}

template bool validateTargetDevice<Elf::EI_CLASS_32>(const Elf::Elf<Elf::EI_CLASS_32> &elf, const TargetDevice &targetDevice, std::string &outErrReason, std::string &outWarning, GeneratorFeatureVersions &generatorFeatures, GeneratorType &generator);
template bool validateTargetDevice<Elf::EI_CLASS_64>(const Elf::Elf<Elf::EI_CLASS_64> &elf, const TargetDevice &targetDevice, std::string &outErrReason, std::string &outWarning, GeneratorFeatureVersions &generatorFeatures, GeneratorType &generator);
template <Elf::ElfIdentifierClass numBits>
bool validateTargetDevice(const Elf::Elf<numBits> &elf, const TargetDevice &targetDevice, std::string &outErrReason, std::string &outWarning, GeneratorFeatureVersions &generatorFeatures, GeneratorType &generator) {
    GFXCORE_FAMILY gfxCore = IGFX_UNKNOWN_CORE;
    PRODUCT_FAMILY productFamily = IGFX_UNKNOWN;
    AOT::PRODUCT_CONFIG productConfig = AOT::UNKNOWN_ISA;
    Elf::ZebinTargetFlags targetMetadata = {};
    std::vector<Elf::IntelGTNote> intelGTNotes = {};
    auto decodeError = getIntelGTNotes(elf, intelGTNotes, outErrReason, outWarning);
    if (DecodeError::success != decodeError) {
        return false;
    }
    for (const auto &intelGTNote : intelGTNotes) {
        switch (intelGTNote.type) {
        case Elf::IntelGTSectionType::productFamily: {
            DEBUG_BREAK_IF(sizeof(uint32_t) != intelGTNote.data.size());
            auto productFamilyData = reinterpret_cast<const uint32_t *>(intelGTNote.data.begin());
            productFamily = static_cast<PRODUCT_FAMILY>(*productFamilyData);
            break;
        }
        case Elf::IntelGTSectionType::gfxCore: {
            DEBUG_BREAK_IF(sizeof(uint32_t) != intelGTNote.data.size());
            auto gfxCoreData = reinterpret_cast<const uint32_t *>(intelGTNote.data.begin());
            gfxCore = static_cast<GFXCORE_FAMILY>(*gfxCoreData);
            break;
        }
        case Elf::IntelGTSectionType::targetMetadata: {
            DEBUG_BREAK_IF(sizeof(uint32_t) != intelGTNote.data.size());
            auto targetMetadataPacked = reinterpret_cast<const uint32_t *>(intelGTNote.data.begin());
            targetMetadata.packed = static_cast<uint32_t>(*targetMetadataPacked);
            generator = static_cast<GeneratorType>(targetMetadata.generatorId);
            break;
        }
        case Elf::IntelGTSectionType::zebinVersion: {
            auto zebinVersionData = reinterpret_cast<const char *>(intelGTNote.data.begin());
            ConstStringRef versionString(zebinVersionData);
            ZeInfo::Types::Version receivedZeInfoVersion{0, 0};
            decodeError = ZeInfo::populateZeInfoVersion(receivedZeInfoVersion, versionString, outErrReason);
            if (DecodeError::success != decodeError) {
                return false;
            }
            decodeError = ZeInfo::validateZeInfoVersion(receivedZeInfoVersion, outErrReason, outWarning);
            if (DecodeError::success != decodeError) {
                return false;
            }
            break;
        }
        case Elf::IntelGTSectionType::productConfig: {
            if (false == targetDevice.applyValidationWorkaround) {
                DEBUG_BREAK_IF(sizeof(uint32_t) != intelGTNote.data.size());
                auto productConfigData = reinterpret_cast<const uint32_t *>(intelGTNote.data.begin());
                productConfig = static_cast<AOT::PRODUCT_CONFIG>(*productConfigData);
                break;
            }
            break;
        }
        case Elf::IntelGTSectionType::vISAAbiVersion: {
            break;
        }
        case Elf::IntelGTSectionType::indirectAccessDetectionVersion: {
            DEBUG_BREAK_IF(sizeof(uint32_t) != intelGTNote.data.size());
            auto indirectDetectionVersion = reinterpret_cast<const uint32_t *>(intelGTNote.data.begin());
            generatorFeatures.indirectMemoryAccessDetection = static_cast<uint32_t>(*indirectDetectionVersion);
            break;
        }
        case Elf::IntelGTSectionType::indirectAccessBufferMajorVersion: {
            DEBUG_BREAK_IF(sizeof(uint32_t) != intelGTNote.data.size());
            auto indirectDetectionVersion = reinterpret_cast<const uint32_t *>(intelGTNote.data.begin());
            generatorFeatures.indirectAccessBuffer = static_cast<uint32_t>(*indirectDetectionVersion);
            break;
        }
        default:
            outWarning.append("DeviceBinaryFormat::zebin : Unrecognized IntelGTNote type: " + std::to_string(intelGTNote.type) + "\n");
            break;
        }
    }
    return validateTargetDevice(targetDevice, numBits, productFamily, gfxCore, productConfig, targetMetadata);
}

template <Elf::ElfIdentifierClass numBits>
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
            outErrReason.append("DeviceBinaryFormat::zebin : Offsetting will cause out-of-bound memory read! Section size: " + std::to_string(sectionSize) +
                                ", current section data offset: " + std::to_string(currentPos) + ", next offset : " + std::to_string(currOffset) + "\n");
            return DecodeError::invalidBinary;
        }
        currentPos += currOffset;

        auto ownerName = reinterpret_cast<const char *>(ptrOffset(intelGTNote, sizeof(Elf::ElfNoteSection)));
        bool isValidGTNote = Elf::intelGTNoteOwnerName.size() + 1 == nameSz;
        isValidGTNote &= Elf::intelGTNoteOwnerName == ConstStringRef(ownerName, nameSz - 1);
        if (false == isValidGTNote) {
            if (0u == nameSz) {
                outWarning.append("DeviceBinaryFormat::zebin : Empty owner name.\n");
            } else {
                std::string invalidOwnerName{ownerName, nameSz};
                invalidOwnerName.erase(std::remove_if(invalidOwnerName.begin(),
                                                      invalidOwnerName.end(),
                                                      [](unsigned char c) { return '\0' == c; }));
                outWarning.append("DeviceBinaryFormat::zebin : Invalid owner name : " + invalidOwnerName + " for IntelGTNote - note will not be used.\n");
            }
            continue;
        }
        auto notesData = ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(ptrOffset(ownerName, nameSz)), descSz);
        if (intelGTNote->type == Elf::IntelGTSectionType::zebinVersion) {
            isValidGTNote &= notesData[descSz - 1] == '\0';
            if (false == isValidGTNote) {
                outWarning.append("DeviceBinaryFormat::zebin :  Versioning string is not null-terminated: " + ConstStringRef(reinterpret_cast<const char *>(notesData.begin()), descSz).str() + " - note will not be used.\n");
                continue;
            }
        }
        intelGTNotes.push_back(Elf::IntelGTNote{static_cast<Elf::IntelGTSectionType>(intelGTNote->type), notesData});
    }
    return DecodeError::success;
}

template <Elf::ElfIdentifierClass numBits>
DecodeError getIntelGTNotes(const Elf::Elf<numBits> &elf, std::vector<Elf::IntelGTNote> &intelGTNotes, std::string &outErrReason, std::string &outWarning) {
    for (size_t i = 0; i < elf.sectionHeaders.size(); i++) {
        auto section = elf.sectionHeaders[i];
        if (Elf::SHT_NOTE == section.header->type && Elf::SectionNames::noteIntelGT == elf.getSectionName(static_cast<uint32_t>(i))) {
            return decodeIntelGTNoteSection<numBits>(section.data, intelGTNotes, outErrReason, outWarning);
        }
    }
    return DecodeError::success;
}

template <Elf::ElfIdentifierClass numBits>
DecodeError extractZebinSections(NEO::Elf::Elf<numBits> &elf, ZebinSections<numBits> &out, std::string &outErrReason, std::string &outWarning) {
    if ((elf.elfFileHeader->shStrNdx >= elf.sectionHeaders.size()) || (NEO::Elf::SHN_UNDEF == elf.elfFileHeader->shStrNdx)) {
        outErrReason.append("DeviceBinaryFormat::zebin : Invalid or missing shStrNdx in elf header\n");
        return DecodeError::invalidBinary;
    }

    auto sectionHeaderNamesData = elf.sectionHeaders[elf.elfFileHeader->shStrNdx].data;
    ConstStringRef sectionHeaderNamesString(reinterpret_cast<const char *>(sectionHeaderNamesData.begin()), sectionHeaderNamesData.size());

    for (auto &elfSectionHeader : elf.sectionHeaders) {
        ConstStringRef sectionName = ConstStringRef(sectionHeaderNamesString.begin() + elfSectionHeader.header->name);
        switch (elfSectionHeader.header->type) {
        default:
            outErrReason.append("DeviceBinaryFormat::zebin : Unhandled ELF section header type : " + std::to_string(elfSectionHeader.header->type) + "\n");
            return DecodeError::invalidBinary;
        case Elf::SHT_PROGBITS:
            if (sectionName.startsWith(Elf::SectionNames::textPrefix.data())) {
                out.textKernelSections.push_back(&elfSectionHeader);
            } else if (sectionName == Elf::SectionNames::text) {
                if (false == elfSectionHeader.data.empty()) {
                    out.textSections.push_back(&elfSectionHeader);
                }
            } else if (sectionName == Elf::SectionNames::dataConst) {
                out.constDataSections.push_back(&elfSectionHeader);
            } else if (sectionName == Elf::SectionNames::dataGlobalConst) {
                outWarning.append("Misspelled section name : " + sectionName.str() + ", should be : " + Elf::SectionNames::dataConst.str() + "\n");
                out.constDataSections.push_back(&elfSectionHeader);
            } else if (sectionName == Elf::SectionNames::dataGlobal) {
                out.globalDataSections.push_back(&elfSectionHeader);
            } else if (sectionName == Elf::SectionNames::dataConstString) {
                out.constDataStringSections.push_back(&elfSectionHeader);
            } else if (sectionName.startsWith(Elf::SectionNames::debugPrefix.data())) {
                // ignoring intentionally
            } else {
                outErrReason.append("DeviceBinaryFormat::zebin : Unhandled SHT_PROGBITS section : " + sectionName.str() + " currently supports only : " + Elf::SectionNames::text.str() + " (aliased to " + Elf::SectionNames::functions.str() + "), " + Elf::SectionNames::textPrefix.str() + "KERNEL_NAME, " + Elf::SectionNames::dataConst.str() + ", " + Elf::SectionNames::dataGlobal.str() + " and " + Elf::SectionNames::debugPrefix.str() + "* .\n");
                return DecodeError::invalidBinary;
            }
            break;
        case Elf::SHT_ZEBIN_ZEINFO:
            out.zeInfoSections.push_back(&elfSectionHeader);
            break;
        case NEO::Elf::SHT_SYMTAB:
            out.symtabSections.push_back(&elfSectionHeader);
            break;
        case Elf::SHT_ZEBIN_SPIRV:
            out.spirvSections.push_back(&elfSectionHeader);
            break;
        case NEO::Elf::SHT_NOTE:
            if (sectionName == Elf::SectionNames::noteIntelGT) {
                out.noteIntelGTSections.push_back(&elfSectionHeader);
            } else {
                outWarning.append("DeviceBinaryFormat::zebin : Unhandled SHT_NOTE section : " + sectionName.str() + " currently supports only : " + Elf::SectionNames::noteIntelGT.str() + ".\n");
            }
            break;
        case Elf::SHT_ZEBIN_MISC:
            if (sectionName == Elf::SectionNames::buildOptions) {
                out.buildOptionsSection.push_back(&elfSectionHeader);
            } else {
                outWarning.append("DeviceBinaryFormat::zebin : unhandled SHT_ZEBIN_MISC section : " + sectionName.str() + " currently supports only : " + Elf::SectionNames::buildOptions.str() + ".\n");
            }
            break;
        case NEO::Elf::SHT_STRTAB:
            // ignoring intentionally - section header names
            continue;
        case NEO::Elf::SHT_REL:
        case NEO::Elf::SHT_RELA:
            // ignoring intentionally - rel/rela sections handled by Elf decoder
            continue;
        case Elf::SHT_ZEBIN_GTPIN_INFO:
            if (sectionName.startsWith(Elf::SectionNames::gtpinInfo.data())) {
                out.gtpinInfoSections.push_back(&elfSectionHeader);
            } else {
                outWarning.append("DeviceBinaryFormat::zebin : Unhandled SHT_ZEBIN_GTPIN_INFO section : " + sectionName.str() + ", currently supports only : " + Elf::SectionNames::gtpinInfo.str() + "KERNEL_NAME\n");
            }
            break;
        case Elf::SHT_ZEBIN_VISA_ASM:
            // ignoring intentionally - visa asm
            continue;
        case NEO::Elf::SHT_NULL:
            // ignoring intentionally, inactive section, probably UNDEF
            continue;
        case NEO::Elf::SHT_NOBITS:
            if (sectionName == Elf::SectionNames::dataConstZeroInit) {
                out.constZeroInitDataSections.push_back(&elfSectionHeader);
            } else if (sectionName == Elf::SectionNames::dataGlobalZeroInit) {
                out.globalZeroInitDataSections.push_back(&elfSectionHeader);
            } else {
                outWarning.append("DeviceBinaryFormat::zebin : unhandled SHT_NOBITS section : " + sectionName.str() + " currently supports only : " + Elf::SectionNames::dataConstZeroInit.str() + " and " + Elf::SectionNames::dataGlobalZeroInit.str() + ".\n");
            }
            break;
        }
    }

    return DecodeError::success;
}

template <typename ContainerT>
bool validateZebinSectionsCountAtMost(const ContainerT &sectionsContainer, ConstStringRef sectionName, uint32_t max, std::string &outErrReason, std::string &outWarning) {
    if (sectionsContainer.size() <= max) {
        return true;
    }

    outErrReason.append("DeviceBinaryFormat::zebin : Expected at most " + std::to_string(max) + " of " + sectionName.str() + " section, got : " + std::to_string(sectionsContainer.size()) + "\n");
    return false;
}

template DecodeError validateZebinSectionsCount<Elf::EI_CLASS_32>(const ZebinSections<Elf::EI_CLASS_32> &sections, std::string &outErrReason, std::string &outWarning);
template DecodeError validateZebinSectionsCount<Elf::EI_CLASS_64>(const ZebinSections<Elf::EI_CLASS_64> &sections, std::string &outErrReason, std::string &outWarning);
template <Elf::ElfIdentifierClass numBits>
DecodeError validateZebinSectionsCount(const ZebinSections<numBits> &sections, std::string &outErrReason, std::string &outWarning) {
    bool valid = validateZebinSectionsCountAtMost(sections.zeInfoSections, Elf::SectionNames::zeInfo, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(sections.globalDataSections, Elf::SectionNames::dataGlobal, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(sections.globalZeroInitDataSections, Elf::SectionNames::dataGlobalZeroInit, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(sections.constDataSections, Elf::SectionNames::dataConst, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(sections.constZeroInitDataSections, Elf::SectionNames::dataConstZeroInit, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(sections.constDataStringSections, Elf::SectionNames::dataConstString, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(sections.symtabSections, Elf::SectionNames::symtab, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(sections.spirvSections, Elf::SectionNames::spv, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(sections.noteIntelGTSections, Elf::SectionNames::noteIntelGT, 1U, outErrReason, outWarning);
    valid &= validateZebinSectionsCountAtMost(sections.textSections, Elf::SectionNames::text, 1U, outErrReason, outWarning);
    return valid ? DecodeError::success : DecodeError::invalidBinary;
}

template <Elf::ElfIdentifierClass numBits>
ConstStringRef extractZeInfoMetadataString(const ArrayRef<const uint8_t> zebin, std::string &outErrReason, std::string &outWarning) {
    auto decodedElf = NEO::Elf::decodeElf<numBits>(zebin, outErrReason, outWarning);
    for (const auto &sectionHeader : decodedElf.sectionHeaders) {
        if (sectionHeader.header->type == Elf::SHT_ZEBIN_ZEINFO) {
            auto zeInfoData = sectionHeader.data;
            return ConstStringRef{reinterpret_cast<const char *>(zeInfoData.begin()), zeInfoData.size()};
        }
    }
    return ConstStringRef{};
}

ConstStringRef getZeInfoFromZebin(const ArrayRef<const uint8_t> zebin, std::string &outErrReason, std::string &outWarning) {
    return Elf::isElf<Elf::EI_CLASS_32>(zebin)
               ? extractZeInfoMetadataString<Elf::EI_CLASS_32>(zebin, outErrReason, outWarning)
               : extractZeInfoMetadataString<Elf::EI_CLASS_64>(zebin, outErrReason, outWarning);
}

template <Elf::ElfIdentifierClass numBits>
void handleTextSection(ProgramInfo &dst, NEO::Elf::Elf<numBits> &elf, ZebinSections<numBits> &zebinSections) {
    if (zebinSections.textSections.empty()) {
        return;
    }

    zebinSections.textKernelSections.push_back(zebinSections.textSections[0]);
    auto kernelInfo = std::make_unique<KernelInfo>();
    kernelInfo->kernelDescriptor.kernelMetadata.kernelName = NEO::Zebin::Elf::SectionNames::externalFunctions.str();
    dst.kernelInfos.push_back(kernelInfo.release());
}

template DecodeError decodeZebin<Elf::EI_CLASS_32>(ProgramInfo &dst, NEO::Elf::Elf<Elf::EI_CLASS_32> &elf, std::string &outErrReason, std::string &outWarning);
template DecodeError decodeZebin<Elf::EI_CLASS_64>(ProgramInfo &dst, NEO::Elf::Elf<Elf::EI_CLASS_64> &elf, std::string &outErrReason, std::string &outWarning);
template <Elf::ElfIdentifierClass numBits>
DecodeError decodeZebin(ProgramInfo &dst, NEO::Elf::Elf<numBits> &elf, std::string &outErrReason, std::string &outWarning) {
    ZebinSections<numBits> zebinSections;
    auto extractError = extractZebinSections(elf, zebinSections, outErrReason, outWarning);
    if (DecodeError::success != extractError) {
        return extractError;
    }

    extractError = validateZebinSectionsCount(zebinSections, outErrReason, outWarning);
    if (DecodeError::success != extractError) {
        return extractError;
    }

    if (false == zebinSections.globalDataSections.empty()) {
        dst.globalVariables.initData = zebinSections.globalDataSections[0]->data.begin();
        dst.globalVariables.size = zebinSections.globalDataSections[0]->data.size();
    }

    if (false == zebinSections.globalZeroInitDataSections.empty()) {
        dst.globalVariables.zeroInitSize = static_cast<size_t>(zebinSections.globalZeroInitDataSections[0]->header->size);
    }

    if (false == zebinSections.constDataSections.empty()) {
        dst.globalConstants.initData = zebinSections.constDataSections[0]->data.begin();
        dst.globalConstants.size = zebinSections.constDataSections[0]->data.size();
    }

    if (false == zebinSections.constZeroInitDataSections.empty()) {
        dst.globalConstants.zeroInitSize = static_cast<size_t>(zebinSections.constZeroInitDataSections[0]->header->size);
    }

    if (false == zebinSections.constDataStringSections.empty()) {
        dst.globalStrings.initData = zebinSections.constDataStringSections[0]->data.begin();
        dst.globalStrings.size = zebinSections.constDataStringSections[0]->data.size();
    }

    if (zebinSections.zeInfoSections.empty()) {
        outWarning.append("DeviceBinaryFormat::zebin : Expected at least one " + Elf::SectionNames::zeInfo.str() + " section, got 0\n");
        return DecodeError::success;
    }

    auto metadataSectionData = zebinSections.zeInfoSections[0]->data;
    ConstStringRef zeinfo(reinterpret_cast<const char *>(metadataSectionData.begin()), metadataSectionData.size());

    std::string logStr("\n=== ZEInfo logging begin ===\n");
    logStr.append(zeinfo.str());
    logStr.append("=== ZEInfo logging end ===\n");
    DBG_LOG(LogZEInfo, logStr.c_str());
    setKernelMiscInfoPosition(zeinfo, dst);
    if (std::string::npos != dst.kernelMiscInfoPos) {
        zeinfo = zeinfo.substr(static_cast<size_t>(0), dst.kernelMiscInfoPos);
    }

    auto decodeZeInfoError = ZeInfo::decodeZeInfo(dst, zeinfo, outErrReason, outWarning);
    if (DecodeError::success != decodeZeInfoError) {
        return decodeZeInfoError;
    }

    handleTextSection(dst, elf, zebinSections);

    for (auto &kernelInfo : dst.kernelInfos) {
        ConstStringRef kernelName(kernelInfo->kernelDescriptor.kernelMetadata.kernelName);
        auto kernelInstructions = getKernelHeap(kernelName, elf, zebinSections);
        if (kernelInstructions.empty()) {
            outErrReason.append("DeviceBinaryFormat::zebin : Could not find text section for kernel " + kernelName.str() + "\n");
            return DecodeError::invalidBinary;
        }

        auto gtpinInfoForKernel = getKernelGtpinInfo(kernelName, elf, zebinSections);
        if (false == gtpinInfoForKernel.empty()) {
            kernelInfo->igcInfoForGtpin = reinterpret_cast<const gtpin::igc_info_t *>(gtpinInfoForKernel.begin());
        }

        kernelInfo->heapInfo.pKernelHeap = kernelInstructions.begin();
        kernelInfo->heapInfo.kernelHeapSize = static_cast<uint32_t>(kernelInstructions.size());
        kernelInfo->heapInfo.kernelUnpaddedSize = static_cast<uint32_t>(kernelInstructions.size());

        auto &kernelSSH = kernelInfo->kernelDescriptor.generatedSsh;
        kernelInfo->heapInfo.pSsh = kernelSSH.data();
        kernelInfo->heapInfo.surfaceStateHeapSize = static_cast<uint32_t>(kernelSSH.size());

        auto &kernelDSH = kernelInfo->kernelDescriptor.generatedDsh;
        kernelInfo->heapInfo.pDsh = kernelDSH.data();
        kernelInfo->heapInfo.dynamicStateHeapSize = static_cast<uint32_t>(kernelDSH.size());
    }

    return DecodeError::success;
}

template ArrayRef<const uint8_t> getKernelHeap<Elf::EI_CLASS_32>(ConstStringRef &kernelName, Elf::Elf<Elf::EI_CLASS_32> &elf, const ZebinSections<Elf::EI_CLASS_32> &zebinSections);
template ArrayRef<const uint8_t> getKernelHeap<Elf::EI_CLASS_64>(ConstStringRef &kernelName, Elf::Elf<Elf::EI_CLASS_64> &elf, const ZebinSections<Elf::EI_CLASS_64> &zebinSections);
template <Elf::ElfIdentifierClass numBits>
ArrayRef<const uint8_t> getKernelHeap(ConstStringRef &kernelName, Elf::Elf<numBits> &elf, const ZebinSections<numBits> &zebinSections) {
    auto sectionHeaderNamesData = elf.sectionHeaders[elf.elfFileHeader->shStrNdx].data;
    ConstStringRef sectionHeaderNamesString(reinterpret_cast<const char *>(sectionHeaderNamesData.begin()), sectionHeaderNamesData.size());
    for (auto *textSection : zebinSections.textKernelSections) {
        ConstStringRef sectionName = ConstStringRef(sectionHeaderNamesString.begin() + textSection->header->name);
        if (getKernelNameFromSectionName(sectionName) == kernelName) {
            return textSection->data;
        }
    }
    return {};
}

ConstStringRef getKernelNameFromSectionName(ConstStringRef sectionName) {
    if (sectionName.startsWith(NEO::Zebin::Elf::SectionNames::textPrefix)) {
        return sectionName.substr(NEO::Zebin::Elf::SectionNames::textPrefix.length());
    } else {
        DEBUG_BREAK_IF(sectionName != NEO::Zebin::Elf::SectionNames::text);
        return Zebin::Elf::SectionNames::externalFunctions;
    }
}

template ArrayRef<const uint8_t> getKernelGtpinInfo<Elf::EI_CLASS_32>(ConstStringRef &kernelName, Elf::Elf<Elf::EI_CLASS_32> &elf, const ZebinSections<Elf::EI_CLASS_32> &zebinSections);
template ArrayRef<const uint8_t> getKernelGtpinInfo<Elf::EI_CLASS_64>(ConstStringRef &kernelName, Elf::Elf<Elf::EI_CLASS_64> &elf, const ZebinSections<Elf::EI_CLASS_64> &zebinSections);
template <Elf::ElfIdentifierClass numBits>
ArrayRef<const uint8_t> getKernelGtpinInfo(ConstStringRef &kernelName, Elf::Elf<numBits> &elf, const ZebinSections<numBits> &zebinSections) {
    auto sectionHeaderNamesData = elf.sectionHeaders[elf.elfFileHeader->shStrNdx].data;
    ConstStringRef sectionHeaderNamesString(reinterpret_cast<const char *>(sectionHeaderNamesData.begin()), sectionHeaderNamesData.size());
    for (auto *gtpinInfoSection : zebinSections.gtpinInfoSections) {
        ConstStringRef sectionName = ConstStringRef(sectionHeaderNamesString.begin() + gtpinInfoSection->header->name);
        auto suffix = sectionName.substr(static_cast<int>(Elf::SectionNames::gtpinInfo.length()));
        if (suffix == kernelName) {
            return gtpinInfoSection->data;
        }
    }
    return {};
}

} // namespace Zebin
} // namespace NEO
