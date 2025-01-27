/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_modules_zebin.h"

#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/mocks/mock_elf.h"

namespace ZebinTestData {
using ElfIdentifierClass = NEO::Elf::ElfIdentifierClass;

template struct ValidEmptyProgram<ElfIdentifierClass::EI_CLASS_32>;
template struct ValidEmptyProgram<ElfIdentifierClass::EI_CLASS_64>;

template ValidEmptyProgram<ElfIdentifierClass::EI_CLASS_32>::ValidEmptyProgram();
template ValidEmptyProgram<ElfIdentifierClass::EI_CLASS_64>::ValidEmptyProgram();
template <ElfIdentifierClass numBits>
ValidEmptyProgram<numBits>::ValidEmptyProgram() {
    NEO::Elf::ElfEncoder<numBits> enc;
    enc.getElfFileHeader().type = NEO::Zebin::Elf::ET_ZEBIN_EXE;
    enc.getElfFileHeader().machine = productFamily;
    auto zeInfo = std::string{"---\nversion : \'" + versionToString(NEO::Zebin::ZeInfo::zeInfoDecoderVersion) + "\'" + "\nkernels : \n  - name : " + kernelName + "\n    execution_env : \n      simd_size  : 32\n      grf_count : 128\n...\n"};
    enc.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, zeInfo);
    enc.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "valid_empty_kernel", zeInfo);
    storage = enc.encode();
    recalcPtr();
}

template <ElfIdentifierClass numBits>
void ValidEmptyProgram<numBits>::recalcPtr() {
    elfHeader = reinterpret_cast<NEO::Elf::ElfFileHeader<numBits> *>(storage.data());
}

template NEO::Elf::ElfSectionHeader<ElfIdentifierClass::EI_CLASS_32> &ValidEmptyProgram<ElfIdentifierClass::EI_CLASS_32>::appendSection(uint32_t sectionType, NEO::ConstStringRef sectionLabel, const ArrayRef<const uint8_t> sectionData);
template NEO::Elf::ElfSectionHeader<ElfIdentifierClass::EI_CLASS_64> &ValidEmptyProgram<ElfIdentifierClass::EI_CLASS_64>::appendSection(uint32_t sectionType, NEO::ConstStringRef sectionLabel, const ArrayRef<const uint8_t> sectionData);
template <ElfIdentifierClass numBits>
NEO::Elf::ElfSectionHeader<numBits> &ValidEmptyProgram<numBits>::appendSection(uint32_t sectionType, NEO::ConstStringRef sectionLabel, const ArrayRef<const uint8_t> sectionData) {
    std::string err, warn;
    auto decoded = NEO::Elf::decodeElf<numBits>(storage, err, warn);
    NEO::Elf::ElfEncoder<numBits> enc;
    enc.getElfFileHeader() = *decoded.elfFileHeader;
    int sectionIt = 0;
    auto sectionHeaderNamesData = decoded.sectionHeaders[decoded.elfFileHeader->shStrNdx].data;
    NEO::ConstStringRef sectionHeaderNamesString(reinterpret_cast<const char *>(sectionHeaderNamesData.begin()), sectionHeaderNamesData.size());
    for (const auto &section : decoded.sectionHeaders) {
        switch (section.header->type) {
        case NEO::Elf::SHN_UNDEF:
            break;
        case NEO::Elf::SHT_STRTAB:
            if (decoded.elfFileHeader->shStrNdx != sectionIt) {
                enc.appendSection(section.header->type, sectionHeaderNamesString.data() + section.header->name, section.data);
            }
            break;
        default:
            enc.appendSection(section.header->type, sectionHeaderNamesString.data() + section.header->name, section.data);
            break;
        }
        ++sectionIt;
    }
    enc.appendSection(sectionType, sectionLabel, sectionData);
    storage = enc.encode();
    recalcPtr();
    decoded = NEO::Elf::decodeElf<numBits>(storage, err, warn);
    sectionHeaderNamesData = decoded.sectionHeaders[decoded.elfFileHeader->shStrNdx].data;
    sectionHeaderNamesString = NEO::ConstStringRef(reinterpret_cast<const char *>(sectionHeaderNamesData.begin()), sectionHeaderNamesData.size());
    for (const auto &section : decoded.sectionHeaders) {
        if ((sectionType == section.header->type) && (sectionLabel == sectionHeaderNamesString.data() + section.header->name)) {
            return const_cast<NEO::Elf::ElfSectionHeader<numBits> &>(*section.header);
        }
    }
    UNREACHABLE();
}

template void ValidEmptyProgram<ElfIdentifierClass::EI_CLASS_32>::removeSection(uint32_t sectionType, NEO::ConstStringRef sectionLabel);
template void ValidEmptyProgram<ElfIdentifierClass::EI_CLASS_64>::removeSection(uint32_t sectionType, NEO::ConstStringRef sectionLabel);
template <ElfIdentifierClass numBits>
void ValidEmptyProgram<numBits>::removeSection(uint32_t sectionType, NEO::ConstStringRef sectionLabel) {
    std::string err, warn;
    auto decoded = NEO::Elf::decodeElf<numBits>(storage, err, warn);
    NEO::Elf::ElfEncoder<numBits> enc;
    enc.getElfFileHeader() = *decoded.elfFileHeader;
    int sectionIt = 0;
    auto sectionHeaderNamesData = decoded.sectionHeaders[decoded.elfFileHeader->shStrNdx].data;
    NEO::ConstStringRef sectionHeaderNamesString(reinterpret_cast<const char *>(sectionHeaderNamesData.begin()), sectionHeaderNamesData.size());
    for (const auto &section : decoded.sectionHeaders) {
        bool add = true;
        switch (section.header->type) {
        case NEO::Elf::SHN_UNDEF:
            add = false;
            break;
        case NEO::Elf::SHT_STRTAB:
            add = (decoded.elfFileHeader->shStrNdx != sectionIt);
            break;
        default:
            add = ((section.header->type != sectionType) || (sectionHeaderNamesString.data() + section.header->name != sectionLabel));
            break;
        }
        if (add) {
            enc.appendSection(section.header->type, sectionHeaderNamesString.data() + section.header->name, section.data);
        }
        ++sectionIt;
    }
    if (decoded.elfFileHeader->shNum <= 3) {
        enc.appendSection(NEO::Elf::SHT_STRTAB, "", {});
    }
    storage = enc.encode();
    recalcPtr();
}

ZebinWithExternalFunctionsInfo::ZebinWithExternalFunctionsInfo() {
    MockElfEncoder<> elfEncoder;
    auto &elfHeader = elfEncoder.getElfFileHeader();
    elfHeader.type = NEO::Zebin::Elf::ET_ZEBIN_EXE;
    elfHeader.flags = 0U;

    const uint8_t kData[32] = {0U};
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "kernel", kData);
    auto kernelSectionIdx = elfEncoder.getLastSectionHeaderIndex();

    const uint8_t funData[32] = {0U};
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + NEO::Zebin::Elf::SectionNames::externalFunctions.str(), funData);
    auto externalFunctionsIdx = elfEncoder.getLastSectionHeaderIndex();

    elfEncoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, zeInfo);

    NEO::Elf::ElfSymbolEntry<NEO::Elf::EI_CLASS_64> symbols[2];
    symbols[0].name = decltype(symbols[0].name)(elfEncoder.appendSectionName(fun0Name));
    symbols[0].info = NEO::Elf::SymbolTableType::STT_FUNC | NEO::Elf::SymbolTableBind::STB_GLOBAL << 4;
    symbols[0].shndx = decltype(symbols[0].shndx)(externalFunctionsIdx);
    symbols[0].size = 16;
    symbols[0].value = 0;

    symbols[1].name = decltype(symbols[1].name)(elfEncoder.appendSectionName(fun1Name));
    symbols[1].info = NEO::Elf::SymbolTableType::STT_FUNC | NEO::Elf::SymbolTableBind::STB_GLOBAL << 4;
    symbols[1].shndx = decltype(symbols[1].shndx)(externalFunctionsIdx);
    symbols[1].size = 16;
    symbols[1].value = 16;
    elfEncoder.appendSection(NEO::Elf::SHT_SYMTAB, NEO::Zebin::Elf::SectionNames::symtab,
                             ArrayRef<const uint8_t>(reinterpret_cast<const uint8_t *>(symbols), sizeof(symbols)));

    NEO::Elf::ElfRel<NEO::Elf::EI_CLASS_64> extFuncSegReloc = {}; // fun0 calls fun1
    extFuncSegReloc.offset = 0x8;
    extFuncSegReloc.info = (uint64_t(1) << 32) | NEO::Zebin::Elf::RelocTypeZebin::R_ZE_SYM_ADDR;
    auto &extFuncRelSection = elfEncoder.appendSection(NEO::Elf::SHT_REL, NEO::Elf::SpecialSectionNames::relPrefix.str() + NEO::Zebin::Elf::SectionNames::textPrefix.str() + NEO::Zebin::Elf::SectionNames::externalFunctions.str(),
                                                       {reinterpret_cast<uint8_t *>(&extFuncSegReloc), sizeof(extFuncSegReloc)});
    extFuncRelSection.info = externalFunctionsIdx;

    NEO::Elf::ElfRel<NEO::Elf::EI_CLASS_64>
        kernelSegReloc = {}; // kernel calls fun0
    kernelSegReloc.offset = 0x8;
    kernelSegReloc.info = (uint64_t(0) << 32) | NEO::Zebin::Elf::RelocTypeZebin::R_ZE_SYM_ADDR;
    auto &kernelRelSection = elfEncoder.appendSection(NEO::Elf::SHT_REL, NEO::Elf::SpecialSectionNames::relPrefix.str() + NEO::Zebin::Elf::SectionNames::textPrefix.str() + "kernel",
                                                      {reinterpret_cast<uint8_t *>(&kernelSegReloc), sizeof(kernelSegReloc)});
    kernelRelSection.info = kernelSectionIdx;

    storage = elfEncoder.encode();
    recalcPtr();

    nameToSegId["kernel"] = 0;
    nameToSegId["Intel_Symbol_Table_Void_Program"] = 1;
}

void ZebinWithExternalFunctionsInfo::recalcPtr() {
    elfHeader = reinterpret_cast<NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> *>(storage.data());
}

void ZebinWithExternalFunctionsInfo::setProductFamily(uint16_t productFamily) {
    elfHeader->machine = productFamily;
}

NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> ZebinWithExternalFunctionsInfo::getElf() {
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf({storage.data(), storage.size()}, errors, warnings);
    return elf;
}

ZebinWithL0TestCommonModule::ZebinWithL0TestCommonModule(const NEO::HardwareInfo &hwInfo, std::initializer_list<AppendElfAdditionalSection> additionalSections, bool forceRecompilation) {
    MockElfEncoder<> elfEncoder;
    auto &elfHeader = elfEncoder.getElfFileHeader();
    elfHeader.type = NEO::Zebin::Elf::ET_ZEBIN_EXE;
    if (forceRecompilation) {
        elfHeader.machine = NEO::Elf::EM_NONE;
    } else {
        auto compilerProductHelper = NEO::CompilerProductHelper::create(hwInfo.platform.eProductFamily);
        auto copyHwInfo = hwInfo;
        compilerProductHelper->adjustHwInfoForIgc(copyHwInfo);
        elfHeader.machine = copyHwInfo.platform.eProductFamily;
    }

    const uint8_t testKernelData[0xac0] = {0u};
    const uint8_t testKernelMemcpyBytesData[0x2c0] = {0u};

    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "test", testKernelData);
    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "memcpy_bytes_attr", testKernelMemcpyBytesData);
    elfEncoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, zeInfo);

    const uint8_t testAdditionalSectionsData[0x10] = {0u};
    for (const auto &s : additionalSections) {
        switch (s) {
        case AppendElfAdditionalSection::spirv:
            elfEncoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_SPIRV, NEO::Zebin::Elf::SectionNames::spv, testAdditionalSectionsData);
            break;
        case AppendElfAdditionalSection::global:
            elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::dataGlobal, testAdditionalSectionsData);
            break;
        case AppendElfAdditionalSection::constant:
            elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::dataConst, testAdditionalSectionsData);
            break;
        case AppendElfAdditionalSection::constantString:
            elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::dataConstString.str(), testAdditionalSectionsData);
            break;
        default:
            break;
        }
    }
    storage = elfEncoder.encode();
    recalcPtr();
}

void ZebinWithL0TestCommonModule::recalcPtr() {
    elfHeader = reinterpret_cast<NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> *>(storage.data());
}

template ZebinCopyBufferSimdModule<ElfIdentifierClass::EI_CLASS_32>::ZebinCopyBufferSimdModule(const NEO::HardwareInfo &hwInfo, uint8_t simdSize);
template ZebinCopyBufferSimdModule<ElfIdentifierClass::EI_CLASS_64>::ZebinCopyBufferSimdModule(const NEO::HardwareInfo &hwInfo, uint8_t simdSize);

template <ElfIdentifierClass numBits>
ZebinCopyBufferSimdModule<numBits>::ZebinCopyBufferSimdModule(const NEO::HardwareInfo &hwInfo, uint8_t simdSize) {
    zeInfoSize = static_cast<size_t>(snprintf(nullptr, 0, zeInfoCopyBufferSimdPlaceholder.c_str(), simdSize, simdSize, getLocalIdSize(hwInfo, simdSize)) + 1);
    zeInfoCopyBuffer.resize(zeInfoSize);
    snprintf(zeInfoCopyBuffer.data(), zeInfoSize, zeInfoCopyBufferSimdPlaceholder.c_str(), simdSize, simdSize, getLocalIdSize(hwInfo, simdSize));

    MockElfEncoder<numBits> elfEncoder;
    auto &elfHeader = elfEncoder.getElfFileHeader();
    elfHeader.type = NEO::Zebin::Elf::ET_ZEBIN_EXE;

    auto compilerProductHelper = NEO::CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    auto copyHwInfo = hwInfo;
    compilerProductHelper->adjustHwInfoForIgc(copyHwInfo);

    elfHeader.machine = copyHwInfo.platform.eProductFamily;
    auto &flags = reinterpret_cast<NEO::Zebin::Elf::ZebinTargetFlags &>(elfHeader.flags);
    flags.generatorId = 1u;

    const uint8_t testKernelData[0x2c0] = {0u};

    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "CopyBuffer", testKernelData);
    elfEncoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo, zeInfoCopyBuffer);

    storage = elfEncoder.encode();
    this->elfHeader = reinterpret_cast<NEO::Elf::ElfFileHeader<numBits> *>(storage.data());
}

size_t writeElfNote(ArrayRef<uint8_t> dst, ArrayRef<const uint8_t> desc, NEO::ConstStringRef name, uint32_t type) {
    auto noteSize = sizeof(NEO::Elf::ElfNoteSection) + alignUp(desc.size(), 4U) + alignUp(name.size() + 1U, 4U);
    UNRECOVERABLE_IF(dst.size() < noteSize)

    memset(dst.begin(), 0, noteSize);
    auto note = reinterpret_cast<NEO::Elf::ElfNoteSection *>(dst.begin());
    note->descSize = static_cast<uint32_t>(desc.size());
    note->nameSize = static_cast<uint32_t>(name.size() + 1U);
    note->type = type;
    auto noteName = ptrOffset(dst.begin(), sizeof(NEO::Elf::ElfNoteSection));
    std::memcpy(noteName, name.begin(), name.size());
    auto noteDesc = ptrOffset(noteName, note->nameSize);
    std::memcpy(noteDesc, desc.begin(), desc.size());
    return noteSize;
}

size_t writeIntelGTNote(ArrayRef<uint8_t> dst, NEO::Zebin::Elf::IntelGTSectionType sectionType, ArrayRef<const uint8_t> desc) {
    return writeElfNote(dst, desc, NEO::Zebin::Elf::intelGTNoteOwnerName, static_cast<uint32_t>(sectionType));
}

size_t writeIntelGTVersionNote(ArrayRef<uint8_t> dst, NEO::ConstStringRef version) {
    std::vector<uint8_t> desc(version.length() + 1U, 0U);
    memcpy_s(desc.data(), desc.size(), version.begin(), version.length());
    return writeIntelGTNote(dst, NEO::Zebin::Elf::zebinVersion, {desc.data(), desc.size()});
}

std::vector<uint8_t> createIntelGTNoteSection(NEO::ConstStringRef version, AOT::PRODUCT_CONFIG productConfig) {
    const size_t noteSectionSize = sizeof(NEO::Elf::ElfNoteSection) * 2 + 8U * 2 + alignUp(version.length() + 1, 4U) + sizeof(AOT::PRODUCT_CONFIG);
    std::vector<uint8_t> intelGTNotesSection(noteSectionSize, 0);
    size_t noteOffset = 0U;
    noteOffset += writeIntelGTVersionNote(ArrayRef<uint8_t>(ptrOffset(intelGTNotesSection.data(), noteOffset), intelGTNotesSection.size() - noteOffset), version);

    writeIntelGTNote(ArrayRef<uint8_t>(ptrOffset(intelGTNotesSection.data(), noteOffset), intelGTNotesSection.size() - noteOffset),
                     NEO::Zebin::Elf::IntelGTSectionType::productConfig,
                     ArrayRef<const uint8_t>::fromAny(&productConfig, 1U));
    return intelGTNotesSection;
}

std::vector<uint8_t> createIntelGTNoteSection(PRODUCT_FAMILY productFamily, GFXCORE_FAMILY coreFamily, NEO::Zebin::Elf::ZebinTargetFlags flags, NEO::ConstStringRef version) {
    const size_t noteSectionSize = sizeof(NEO::Elf::ElfNoteSection) * 4U + 4U * 8U + 3U * 4U + alignUp(version.length() + 1, 4U);
    std::vector<uint8_t> intelGTNotes(noteSectionSize, 0);
    size_t noteOffset = 0U;
    noteOffset += writeIntelGTNote(ArrayRef<uint8_t>(ptrOffset(intelGTNotes.data(), noteOffset), intelGTNotes.size() - noteOffset),
                                   NEO::Zebin::Elf::productFamily,
                                   ArrayRef<const uint8_t>::fromAny(&productFamily, 1U));

    noteOffset += writeIntelGTNote(ArrayRef<uint8_t>(ptrOffset(intelGTNotes.data(), noteOffset), intelGTNotes.size() - noteOffset),
                                   NEO::Zebin::Elf::gfxCore,
                                   ArrayRef<const uint8_t>::fromAny(&coreFamily, 1U));

    noteOffset += writeIntelGTNote(ArrayRef<uint8_t>(ptrOffset(intelGTNotes.data(), noteOffset), intelGTNotes.size() - noteOffset),
                                   NEO::Zebin::Elf::targetMetadata,
                                   ArrayRef<const uint8_t>::fromAny(&flags.packed, 1U));

    writeIntelGTVersionNote(ArrayRef<uint8_t>(ptrOffset(intelGTNotes.data(), noteOffset), intelGTNotes.size() - noteOffset),
                            version);
    return intelGTNotes;
}

}; // namespace ZebinTestData
