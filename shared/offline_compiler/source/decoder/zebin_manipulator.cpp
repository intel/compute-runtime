/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "zebin_manipulator.h"

#include "shared/offline_compiler/source/decoder/iga_wrapper.h"
#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/source/device_binary_format/device_binary_formats.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/zebin_elf.h"
#include "shared/source/device_binary_format/zebin_decoder.h"

#include <algorithm>

namespace NEO::ZebinManipulator {

ErrorCode parseIntelGTNotesSectionForDevice(const std::vector<Elf::IntelGTNote> &intelGTNotes, IgaWrapper *iga) {
    size_t productFamilyNoteId = std::numeric_limits<size_t>::max();
    size_t gfxCoreNoteId = std::numeric_limits<size_t>::max();
    for (size_t i = 0; i < intelGTNotes.size(); i++) {
        if (intelGTNotes[i].type == Elf::IntelGTSectionType::ProductFamily) {
            productFamilyNoteId = i;
        } else if (intelGTNotes[i].type == Elf::IntelGTSectionType::GfxCore) {
            gfxCoreNoteId = i;
        }
    }

    if (productFamilyNoteId != std::numeric_limits<size_t>::max()) {
        UNRECOVERABLE_IF(sizeof(PRODUCT_FAMILY) != intelGTNotes[productFamilyNoteId].data.size());
        auto productFamily = *reinterpret_cast<const PRODUCT_FAMILY *>(intelGTNotes[productFamilyNoteId].data.begin());
        iga->setProductFamily(productFamily);
        return OclocErrorCode::SUCCESS;
    } else if (gfxCoreNoteId != std::numeric_limits<size_t>::max()) {
        UNRECOVERABLE_IF(sizeof(GFXCORE_FAMILY) != intelGTNotes[gfxCoreNoteId].data.size());
        auto gfxCore = *reinterpret_cast<const GFXCORE_FAMILY *>(intelGTNotes[gfxCoreNoteId].data.begin());
        iga->setGfxCore(gfxCore);
        return OclocErrorCode::SUCCESS;
    }

    return OclocErrorCode::INVALID_DEVICE;
}

ErrorCode validateInput(const std::vector<std::string> &args, IgaWrapper *iga, OclocArgHelper *argHelper, Arguments &outArguments) {
    for (size_t argIndex = 2; argIndex < args.size(); ++argIndex) {
        const auto &currArg = args[argIndex];
        const bool hasMoreArgs = (argIndex + 1 < args.size());
        if ("-file" == currArg && hasMoreArgs) {
            outArguments.binaryFile = args[++argIndex];
        } else if ("-device" == currArg && hasMoreArgs) {
            iga->setProductFamily(getProductFamilyFromDeviceName(args[++argIndex]));
        } else if ("-dump" == currArg && hasMoreArgs) {
            outArguments.pathToDump = args[++argIndex];
            addSlash(outArguments.pathToDump);
        } else if ("--help" == currArg) {
            outArguments.showHelp = true;
            return OclocErrorCode::SUCCESS;
        } else if ("-q" == currArg) {
            argHelper->getPrinterRef() = MessagePrinter(true);
            iga->setMessagePrinter(argHelper->getPrinterRef());
        } else if ("-skip-asm-translation" == currArg) {
            outArguments.skipIGAdisassembly = true;
        } else {
            argHelper->printf("Unknown argument %s\n", currArg.c_str());
            return OclocErrorCode::INVALID_COMMAND_LINE;
        }
    }

    if (outArguments.binaryFile.empty()) {
        argHelper->printf("Error: Missing -file argument\n");
        return OclocErrorCode::INVALID_COMMAND_LINE;
    }

    if (outArguments.pathToDump.empty()) {
        argHelper->printf("Warning: Path to dump -dump not specified. Using \"./dump/\" as dump folder.\n");
        outArguments.pathToDump = "dump/";
    }

    return OclocErrorCode::SUCCESS;
}

bool is64BitZebin(OclocArgHelper *argHelper, const std::string &sectionsInfoFilepath) {
    std::vector<std::string> lines;
    argHelper->readFileToVectorOfStrings(sectionsInfoFilepath, lines);
    if (lines.empty()) {
        return false;
    }

    auto ss = std::stringstream(lines[0]);
    std::vector<std::string> elfTypeStringSplit;
    while (ss.good()) {
        auto &element = elfTypeStringSplit.emplace_back();
        std::getline(ss, element, ' ');
    }
    return elfTypeStringSplit.size() == 2 && std::stoi(elfTypeStringSplit[1]) == 64;
}

BinaryFormats getBinaryFormatForAssemble(OclocArgHelper *argHelper, const std::vector<std::string> &args) {
    auto it = std::find(args.begin(), args.end(), "-dump");
    std::string dump = (it != args.end() && (it + 1) != args.end()) ? *(it + 1) : "dump/";
    auto sectionsInfoFilepath = dump + ZebinManipulator::sectionsInfoFilename.str();
    const bool usesZebin = argHelper->fileExists(sectionsInfoFilepath);
    if (usesZebin) {
        return ZebinManipulator::is64BitZebin(argHelper, sectionsInfoFilepath) ? BinaryFormats::Zebin64b : BinaryFormats::Zebin32b;
    }
    return BinaryFormats::PatchTokens;
}

BinaryFormats getBinaryFormatForDisassemble(OclocArgHelper *argHelper, const std::vector<std::string> &args) {
    auto it = std::find(args.begin(), args.end(), "-file");
    if (it != args.end() && (it + 1) != args.end()) {
        auto file = argHelper->readBinaryFile(*(it + 1));
        auto fileRef = ArrayRef<const uint8_t>::fromAny(file.data(), file.size());
        if (NEO::isDeviceBinaryFormat<DeviceBinaryFormat::Zebin>(fileRef)) {
            auto numBits = Elf::getElfNumBits(fileRef);
            return numBits == Elf::EI_CLASS_64 ? BinaryFormats::Zebin64b : BinaryFormats::Zebin32b;
        }
    }
    return BinaryFormats::PatchTokens;
}

template ZebinDecoder<Elf::EI_CLASS_32>::ZebinDecoder(OclocArgHelper *argHelper);
template ZebinDecoder<Elf::EI_CLASS_64>::ZebinDecoder(OclocArgHelper *argHelper);
template <Elf::ELF_IDENTIFIER_CLASS numBits>
ZebinDecoder<numBits>::ZebinDecoder(OclocArgHelper *argHelper) : argHelper(argHelper),
                                                                 iga(new IgaWrapper) {
    iga->setMessagePrinter(argHelper->getPrinterRef());
}

template ZebinDecoder<Elf::EI_CLASS_32>::~ZebinDecoder();
template ZebinDecoder<Elf::EI_CLASS_64>::~ZebinDecoder();
template <Elf::ELF_IDENTIFIER_CLASS numBits>
ZebinDecoder<numBits>::~ZebinDecoder() {}

template ErrorCode ZebinDecoder<Elf::EI_CLASS_32>::decode();
template ErrorCode ZebinDecoder<Elf::EI_CLASS_64>::decode();
template <Elf::ELF_IDENTIFIER_CLASS numBits>
ErrorCode ZebinDecoder<numBits>::decode() {
    auto zebinBinary = argHelper->readBinaryFile(this->arguments.binaryFile);
    ArrayRef<const uint8_t> zebinBinaryRef = {reinterpret_cast<const uint8_t *>(zebinBinary.data()),
                                              zebinBinary.size()};

    ElfT elf;
    ErrorCode retVal = decodeZebin(ArrayRef<const uint8_t>::fromAny(zebinBinary.data(), zebinBinary.size()), elf);
    if (retVal != OclocErrorCode::SUCCESS) {
        argHelper->printf("Error while decoding zebin.\n");
        return retVal;
    }

    if (false == arguments.skipIGAdisassembly) {
        auto intelGTNotes = getIntelGTNotes(elf);
        if (intelGTNotes.empty()) {
            argHelper->printf("Error missing or invalid Intel GT Notes section.\n");
            return OclocErrorCode::INVALID_FILE;
        }

        retVal = parseIntelGTNotesSectionForDevice(intelGTNotes, iga.get());
        if (retVal != OclocErrorCode::SUCCESS) {
            argHelper->printf("Error while parsing Intel GT Notes section for device.\n");
            return retVal;
        }
    }

    auto sectionsInfo = dumpElfSections(elf);
    dumpSectionInfo(sectionsInfo);

    return OclocErrorCode::SUCCESS;
}

template ErrorCode ZebinDecoder<Elf::EI_CLASS_32>::validateInput(const std::vector<std::string> &args);
template ErrorCode ZebinDecoder<Elf::EI_CLASS_64>::validateInput(const std::vector<std::string> &args);
template <Elf::ELF_IDENTIFIER_CLASS numBits>
ErrorCode ZebinDecoder<numBits>::validateInput(const std::vector<std::string> &args) {
    return ZebinManipulator::validateInput(args, iga.get(), argHelper, arguments);
}

template void ZebinDecoder<Elf::EI_CLASS_32>::printHelp();
template void ZebinDecoder<Elf::EI_CLASS_64>::printHelp();
template <Elf::ELF_IDENTIFIER_CLASS numBits>
void ZebinDecoder<numBits>::printHelp() {
    argHelper->printf(R"===(Disassembles Zebin.
Output of such operation is a set of files that can be later used to reassemble back.
Symbols and relocations are translated into human readable format. Kernels are translated
into assembly. File named "sections.txt" is created which describes zebin sections.

Usage: ocloc disasm -file <file> [-dump <dump_dir>] [-device <device_type>] [-skip-asm-translation]
  -file <file>               Input file to be disassembled.

  -dump <dump_dir>           Optional. Path for files representing decoded binary. Default is './dump'.

  -device <device_type>      Optional. Target device of input binary. 

  -skip-asm-translation      Optional. Skips parsing intelGTNotes for device and skips kernel 
                             translation to assembly.

  --help                     Print this usage message.
)===");
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
void ZebinDecoder<numBits>::dump(ConstStringRef name, ArrayRef<const uint8_t> data) {
    auto outPath = arguments.pathToDump + name.str();
    argHelper->saveOutput(outPath, data.begin(), data.size());
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
void ZebinDecoder<numBits>::dumpKernelData(ConstStringRef name, ArrayRef<const uint8_t> data) {
    std::string disassembledKernel;
    if (false == arguments.skipIGAdisassembly &&
        iga->tryDisassembleGenISA(data.begin(), static_cast<uint32_t>(data.size()), disassembledKernel)) {
        dump(name.str() + ".asm", {reinterpret_cast<const uint8_t *>(disassembledKernel.data()), disassembledKernel.length()});
    } else {
        dump(name, data);
    }
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
void ZebinDecoder<numBits>::dumpSymtab(ElfT &elf, ArrayRef<const uint8_t> symtabData) {
    ArrayRef<const ElfSymT> symbols(reinterpret_cast<const ElfSymT *>(symtabData.begin()),
                                    symtabData.size() / sizeof(ElfSymT));

    std::stringstream symbolsFile;
    symbolsFile << "Id, Name, Section, Value, Type, Visibility, Binding\n";
    int symbolID = 0;
    for (auto &symbol : symbols) {
        auto symbolName = elf.getSymbolName(symbol.name);
        if (symbolName.empty()) {
            symbolName = "UNDEF";
        }
        auto sectionName = elf.getSectionName(symbol.shndx);
        if (sectionName.empty()) {
            sectionName = "UNDEF";
        }

        symbolsFile << std::to_string(symbolID++) << ", "
                    << symbolName << ", "
                    << sectionName << ", "
                    << std::to_string(symbol.value) << ", "
                    << std::to_string(symbol.getType()) << ", "
                    << std::to_string(symbol.getVisibility()) << ", "
                    << std::to_string(symbol.getBinding()) << "\n";
    }
    auto symbolsFileStr = symbolsFile.str();
    dump(Elf::SectionsNamesZebin::symtab, ArrayRef<const uint8_t>::fromAny(symbolsFileStr.data(), symbolsFileStr.size()));
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
std::vector<SectionInfo> ZebinDecoder<numBits>::dumpElfSections(ElfT &elf) {
    std::vector<SectionInfo> sectionInfos;
    for (size_t secId = 1U; secId < elf.sectionHeaders.size(); secId++) {
        auto &[header, data] = elf.sectionHeaders[secId];
        auto sectionName = elf.getSectionName(static_cast<uint32_t>(secId));
        if (header->type == Elf::SHT_PROGBITS &&
            ConstStringRef(sectionName).startsWith(Elf::SectionsNamesZebin::textPrefix)) {
            dumpKernelData(sectionName, data);
        } else if (header->type == Elf::SHT_SYMTAB) {
            dumpSymtab(elf, data);
        } else if (header->type == Elf::SHT_REL) {
            dumpRel(sectionName, data);
        } else if (header->type == Elf::SHT_RELA) {
            dumpRela(sectionName, data);
        } else if (header->type == Elf::SHT_STRTAB) {
            continue;
        } else {
            dump(sectionName, data);
        }
        sectionInfos.push_back({sectionName, header->type});
    }
    return sectionInfos;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
void ZebinDecoder<numBits>::dumpSectionInfo(const std::vector<SectionInfo> &sectionInfos) {
    std::stringstream sectionsInfoStr;
    sectionsInfoStr << "ElfType " << (numBits == Elf::EI_CLASS_64 ? "64b" : "32b") << std::endl;
    sectionsInfoStr << "Section name, Section type" << std::endl;
    for (const auto &sectionInfo : sectionInfos) {
        sectionsInfoStr << sectionInfo.name << ", " << std::to_string(sectionInfo.type) << std::endl;
    }
    auto sectionInfoStr = sectionsInfoStr.str();
    dump(sectionsInfoFilename, ArrayRef<const uint8_t>::fromAny(sectionInfoStr.data(), sectionInfoStr.size()));
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
ErrorCode ZebinDecoder<numBits>::decodeZebin(ArrayRef<const uint8_t> zebin, ElfT &outElf) {
    std::string errors, warnings;
    outElf = Elf::decodeElf<numBits>(zebin, errors, warnings);

    if (false == errors.empty()) {
        argHelper->printf("decodeElf error: %s\n", errors.c_str());
        return OclocErrorCode::INVALID_FILE;
    }

    return OclocErrorCode::SUCCESS;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
std::vector<Elf::IntelGTNote> ZebinDecoder<numBits>::getIntelGTNotes(ElfT &elf) {
    std::vector<Elf::IntelGTNote> intelGTNotes;
    std::string errors, warnings;
    NEO::getIntelGTNotes(elf, intelGTNotes, errors, warnings);
    if (false == errors.empty()) {
        argHelper->printf("Error when reading intelGTNotes: %s\n", errors.c_str());
    }
    return intelGTNotes;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
void ZebinDecoder<numBits>::dumpRel(ConstStringRef name, ArrayRef<const uint8_t> data) {
    ArrayRef<const ElfRelT> relocs = {reinterpret_cast<const ElfRelT *>(data.begin()),
                                      data.size() / sizeof(ElfRelT)};
    std::stringstream relocsFile;
    relocsFile << "Offset, Type, SymbolIdx\n";
    for (auto &reloc : relocs) {
        relocsFile << std::to_string(reloc.offset) << ", "
                   << std::to_string(reloc.getRelocationType()) << ", "
                   << std::to_string(reloc.getSymbolTableIndex()) << "\n";
    }
    auto relocsFileStr = relocsFile.str();
    dump(name, ArrayRef<const uint8_t>::fromAny(relocsFileStr.data(), relocsFileStr.length()));
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
void ZebinDecoder<numBits>::dumpRela(ConstStringRef name, ArrayRef<const uint8_t> data) {
    ArrayRef<const ElfRelaT> relocs = {reinterpret_cast<const ElfRelaT *>(data.begin()),
                                       data.size() / sizeof(ElfRelaT)};
    std::stringstream relocsFile;
    relocsFile << "Offset, Type, SymbolIdx, Addend\n";
    for (auto &reloc : relocs) {
        relocsFile << std::to_string(reloc.offset) << ", "
                   << std::to_string(reloc.getRelocationType()) << ", "
                   << std::to_string(reloc.getSymbolTableIndex()) << ", "
                   << std::to_string(reloc.addend) << "\n";
    }
    auto relocsFileStr = relocsFile.str();
    dump(name, ArrayRef<const uint8_t>::fromAny(relocsFileStr.data(), relocsFileStr.length()));
}

template ZebinEncoder<Elf::EI_CLASS_32>::ZebinEncoder(OclocArgHelper *argHelper);
template ZebinEncoder<Elf::EI_CLASS_64>::ZebinEncoder(OclocArgHelper *argHelper);
template <Elf::ELF_IDENTIFIER_CLASS numBits>
ZebinEncoder<numBits>::ZebinEncoder(OclocArgHelper *argHelper) : argHelper(argHelper), iga(new IgaWrapper) {
    iga->setMessagePrinter(argHelper->getPrinterRef());
}

template ZebinEncoder<Elf::EI_CLASS_32>::~ZebinEncoder();
template ZebinEncoder<Elf::EI_CLASS_64>::~ZebinEncoder();
template <Elf::ELF_IDENTIFIER_CLASS numBits>
ZebinEncoder<numBits>::~ZebinEncoder() {}

template ErrorCode ZebinEncoder<Elf::EI_CLASS_32>::encode();
template ErrorCode ZebinEncoder<Elf::EI_CLASS_64>::encode();
template <Elf::ELF_IDENTIFIER_CLASS numBits>
ErrorCode ZebinEncoder<numBits>::encode() {
    ErrorCode retVal = OclocErrorCode::SUCCESS;

    std::vector<SectionInfo> sectionInfos;
    retVal = loadSectionsInfo(sectionInfos);
    if (retVal != OclocErrorCode::SUCCESS) {
        argHelper->printf("Error while loading sections file.\n");
        return retVal;
    }

    retVal = checkIfAllFilesExist(sectionInfos);
    if (retVal != OclocErrorCode::SUCCESS) {
        argHelper->printf("Error: Missing one or more section files.\n");
        return retVal;
    }

    auto intelGTNotesSectionData = getIntelGTNotesSection(sectionInfos);
    auto intelGTNotes = getIntelGTNotes(intelGTNotesSectionData);
    retVal = parseIntelGTNotesSectionForDevice(intelGTNotes, iga.get());
    if (retVal != OclocErrorCode::SUCCESS) {
        argHelper->printf("Error while parsing Intel GT Notes section for device.\n");
        return retVal;
    }

    ElfEncoderT elfEncoder;
    elfEncoder.getElfFileHeader().machine = Elf::ELF_MACHINE::EM_INTELGT;
    elfEncoder.getElfFileHeader().type = Elf::ELF_TYPE_ZEBIN::ET_ZEBIN_EXE;

    retVal = appendSections(elfEncoder, sectionInfos);
    if (retVal != OclocErrorCode::SUCCESS) {
        argHelper->printf("Error while appending elf sections.\n");
        return retVal;
    }

    auto zebin = elfEncoder.encode();
    argHelper->saveOutput(getFilePath(arguments.binaryFile), zebin.data(), zebin.size());

    return OclocErrorCode::SUCCESS;
}

template ErrorCode ZebinEncoder<Elf::EI_CLASS_32>::validateInput(const std::vector<std::string> &args);
template ErrorCode ZebinEncoder<Elf::EI_CLASS_64>::validateInput(const std::vector<std::string> &args);
template <Elf::ELF_IDENTIFIER_CLASS numBits>
ErrorCode ZebinEncoder<numBits>::validateInput(const std::vector<std::string> &args) {
    return ZebinManipulator::validateInput(args, iga.get(), argHelper, arguments);
}

template void ZebinEncoder<Elf::EI_CLASS_32>::printHelp();
template void ZebinEncoder<Elf::EI_CLASS_64>::printHelp();
template <Elf::ELF_IDENTIFIER_CLASS numBits>
void ZebinEncoder<numBits>::printHelp() {
    argHelper->printf(R"===(Assembles Zebin from input files.
It's expected that input files were previously generated by 'ocloc disasm'
command or are compatible with 'ocloc disasm' output (especially in terms of
file naming scheme).

Usage: ocloc asm -file <file> [-dump <dump_dir>] [-device <device_type>] [-skip-asm-translation]
  -file <file>               Name of the newly assembled zebin.

  -dump <dump_dir>           Optional. Path to the input directory containing disassembled binary.
                             Default is './dump'.

  -device <device_type>      Optional. Target device of input binary. 

  --help                     Print this usage message.
)===");
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
std::vector<char> ZebinEncoder<numBits>::getIntelGTNotesSection(const std::vector<SectionInfo> &sectionInfos) {
    bool containsIntelGTNoteSection = false;
    for (auto &sectionInfo : sectionInfos) {
        if (sectionInfo.type == Elf::SHT_NOTE &&
            sectionInfo.name == Elf::SectionsNamesZebin::noteIntelGT) {
            containsIntelGTNoteSection = true;
            break;
        }
    }
    if (false == containsIntelGTNoteSection) {
        return {};
    }

    return argHelper->readBinaryFile(getFilePath(Elf::SectionsNamesZebin::noteIntelGT.data()));
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
std::vector<Elf::IntelGTNote> ZebinEncoder<numBits>::getIntelGTNotes(const std::vector<char> &intelGtNotesSection) {
    std::vector<Elf::IntelGTNote> intelGTNotes;
    std::string errors, warnings;
    auto refIntelGTNotesSection = ArrayRef<const uint8_t>::fromAny(intelGtNotesSection.data(), intelGtNotesSection.size());
    auto decodeError = decodeIntelGTNoteSection<numBits>(refIntelGTNotesSection, intelGTNotes, errors, warnings);
    argHelper->printf(warnings.c_str());
    if (decodeError != NEO::DecodeError::Success) {
        argHelper->printf(errors.c_str());
    }
    return intelGTNotes;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
ErrorCode ZebinEncoder<numBits>::loadSectionsInfo(std::vector<SectionInfo> &sectionInfos) {
    std::vector<std::string> sectionsInfoLines;
    argHelper->readFileToVectorOfStrings(getFilePath(sectionsInfoFilename.data()), sectionsInfoLines);
    if (sectionsInfoLines.size() <= 2) {
        return OclocErrorCode::INVALID_FILE;
    }

    sectionInfos.resize(sectionsInfoLines.size() - 2);
    for (size_t i = 2; i < sectionsInfoLines.size(); i++) {
        auto elements = parseLine(sectionsInfoLines[i]);
        UNRECOVERABLE_IF(elements.size() != 2);
        auto &sectionInfo = sectionInfos[i - 2];
        sectionInfo.name = elements[0];
        sectionInfo.type = static_cast<uint32_t>(std::stoull(elements[1]));
    }
    return OclocErrorCode::SUCCESS;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
ErrorCode NEO::ZebinManipulator::ZebinEncoder<numBits>::checkIfAllFilesExist(const std::vector<SectionInfo> &sectionInfos) {
    for (auto &sectionInfo : sectionInfos) {
        bool fileExists = argHelper->fileExists(getFilePath(sectionInfo.name));
        if (ConstStringRef(sectionInfo.name).startsWith(Elf::SectionsNamesZebin::textPrefix)) {
            fileExists |= argHelper->fileExists(getFilePath(sectionInfo.name + ".asm"));
        }

        if (false == fileExists) {
            argHelper->printf("Error: Could not find the file \"%s\"\n", sectionInfo.name.c_str());
            return OclocErrorCode::INVALID_FILE;
        }
    }
    return OclocErrorCode::SUCCESS;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
ErrorCode ZebinEncoder<numBits>::appendSections(ElfEncoderT &encoder, const std::vector<SectionInfo> &sectionInfos) {
    SecNameToIdMapT secNameToId;
    size_t symtabIdx = std::numeric_limits<size_t>::max();
    for (size_t i = 0; i < sectionInfos.size(); i++) {
        secNameToId[sectionInfos[i].name] = i + 1;
        if (sectionInfos[i].name == Elf::SectionsNamesZebin::symtab) {
            symtabIdx = i + 1;
        }
    }

    ErrorCode retVal = OclocErrorCode::SUCCESS;
    for (const auto &section : sectionInfos) {
        if (section.type == Elf::SHT_SYMTAB) {
            retVal |= appendSymtab(encoder, section, sectionInfos.size() + 1, secNameToId);
        } else if (section.type == Elf::SHT_REL) {
            retVal |= appendRel(encoder, section, secNameToId[section.name.substr(Elf::SpecialSectionNames::relPrefix.length())], symtabIdx);
        } else if (section.type == Elf::SHT_RELA) {
            retVal |= appendRela(encoder, section, secNameToId[section.name.substr(Elf::SpecialSectionNames::relaPrefix.length())], symtabIdx);
        } else if (section.type == Elf::SHT_PROGBITS && ConstStringRef(section.name).startsWith(Elf::SectionsNamesZebin::textPrefix)) {
            retVal |= appendKernel(encoder, section);
        } else {
            retVal |= appendOther(encoder, section);
        }
    }
    return retVal;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
ErrorCode ZebinEncoder<numBits>::appendRel(ElfEncoderT &encoder, const SectionInfo &section, size_t targetSecId, size_t symtabSecId) {
    std::vector<std::string> relocationLines;
    argHelper->readFileToVectorOfStrings(getFilePath(section.name), relocationLines);
    if (relocationLines.empty()) {
        argHelper->printf("Error: Empty relocations file: %s\n", section.name.c_str());
        return OclocErrorCode::INVALID_FILE;
    }
    auto relocs = parseRel(relocationLines);
    auto &sec = encoder.appendSection(Elf::SHT_REL, section.name, ArrayRef<const uint8_t>::fromAny(relocs.data(), relocs.size()));
    sec.info = static_cast<uint32_t>(targetSecId);
    sec.link = static_cast<uint32_t>(symtabSecId);
    return OclocErrorCode::SUCCESS;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
ErrorCode ZebinEncoder<numBits>::appendRela(ElfEncoderT &encoder, const SectionInfo &section, size_t targetSecId, size_t symtabSecId) {
    std::vector<std::string> relocationLines;
    argHelper->readFileToVectorOfStrings(getFilePath(section.name), relocationLines);
    if (relocationLines.empty()) {
        argHelper->printf("Error: Empty relocations file: %s\n", section.name.c_str());
        return OclocErrorCode::INVALID_FILE;
    }
    auto relocs = parseRela(relocationLines);
    auto &sec = encoder.appendSection(Elf::SHT_RELA, section.name, ArrayRef<const uint8_t>::fromAny(relocs.data(), relocs.size()));
    sec.info = static_cast<uint32_t>(targetSecId);
    sec.link = static_cast<uint32_t>(symtabSecId);
    return OclocErrorCode::SUCCESS;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
std::string ZebinEncoder<numBits>::getFilePath(const std::string &filename) {
    return arguments.pathToDump + filename;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
std::string ZebinEncoder<numBits>::parseKernelAssembly(ArrayRef<const char> kernelAssembly) {
    std::string kernelAssemblyString(kernelAssembly.begin(), kernelAssembly.end());
    std::string outBinary;
    if (iga->tryAssembleGenISA(kernelAssemblyString, outBinary)) {
        return outBinary;
    }
    return {};
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
ErrorCode ZebinEncoder<numBits>::appendKernel(ElfEncoderT &encoder, const SectionInfo &section) {
    if (argHelper->fileExists(getFilePath(section.name + ".asm"))) {
        auto data = argHelper->readBinaryFile(getFilePath(section.name + ".asm"));
        auto kernelBinary = parseKernelAssembly(ArrayRef<const char>::fromAny(data.data(), data.size()));
        ArrayRef<const uint8_t> refKernelBinary = {reinterpret_cast<const uint8_t *>(kernelBinary.data()), kernelBinary.size()};
        encoder.appendSection(section.type, section.name, refKernelBinary);
    } else {
        auto data = argHelper->readBinaryFile(getFilePath(section.name));
        encoder.appendSection(Elf::SHT_PROGBITS, section.name, ArrayRef<const uint8_t>::fromAny(data.data(), data.size()));
    }
    return OclocErrorCode::SUCCESS;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
ErrorCode ZebinEncoder<numBits>::appendSymtab(ElfEncoderT &encoder, const SectionInfo &section, size_t strtabSecId, SecNameToIdMapT secNameToId) {
    std::vector<std::string> symTabLines;
    argHelper->readFileToVectorOfStrings(getFilePath(section.name), symTabLines);
    if (symTabLines.empty()) {
        argHelper->printf("Error: Empty symtab file: %s\n", section.name.c_str());
        return OclocErrorCode::INVALID_FILE;
    }

    size_t numLocalSymbols = 0;
    auto symbols = parseSymbols(symTabLines, encoder, numLocalSymbols, secNameToId);

    auto &symtabSection = encoder.appendSection(section.type, section.name, ArrayRef<const uint8_t>::fromAny(symbols.data(), symbols.size()));
    symtabSection.info = static_cast<uint32_t>(numLocalSymbols);
    symtabSection.link = static_cast<uint32_t>(strtabSecId);
    return OclocErrorCode::SUCCESS;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
ErrorCode NEO::ZebinManipulator::ZebinEncoder<numBits>::appendOther(ElfEncoderT &encoder, const SectionInfo &section) {
    auto sectionData = argHelper->readBinaryFile(getFilePath(section.name));
    encoder.appendSection(section.type, section.name, ArrayRef<const uint8_t>::fromAny(sectionData.data(), sectionData.size()));
    return OclocErrorCode::SUCCESS;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
std::vector<std::string> ZebinEncoder<numBits>::parseLine(const std::string &line) {
    std::vector<std::string> out;
    auto ss = std::stringstream(line);
    while (ss.good()) {
        auto &element = out.emplace_back();
        std::getline(ss, element, ',');
    }
    return out;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
std::vector<typename ZebinEncoder<numBits>::ElfRelT> ZebinEncoder<numBits>::parseRel(const std::vector<std::string> &relocationsFile) {
    std::vector<ElfRelT> relocs;
    relocs.resize(relocationsFile.size() - 1);

    for (size_t lineId = 1U; lineId < relocationsFile.size(); lineId++) {
        auto elements = parseLine(relocationsFile[lineId]);
        UNRECOVERABLE_IF(elements.size() != 3);

        auto &reloc = relocs[lineId - 1];
        reloc.offset = static_cast<typename ElfRelT::Offset>(std::stoull(elements[0]));
        reloc.setRelocationType(static_cast<typename ElfRelT::Info>(std::stoull(elements[1])));
        reloc.setSymbolTableIndex(static_cast<typename ElfRelT::Info>(std::stoull(elements[2])));
    }

    return relocs;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
std::vector<typename ZebinEncoder<numBits>::ElfRelaT> ZebinEncoder<numBits>::parseRela(const std::vector<std::string> &relocationsFile) {
    std::vector<ElfRelaT> relocs;
    relocs.resize(relocationsFile.size() - 1);

    for (size_t lineId = 1U; lineId < relocationsFile.size(); lineId++) {
        auto elements = parseLine(relocationsFile[lineId]);
        UNRECOVERABLE_IF(elements.size() != 4);

        auto &reloc = relocs[lineId - 1];
        reloc.offset = static_cast<typename ElfRelaT::Offset>(std::stoull(elements[0]));
        reloc.setRelocationType(static_cast<typename ElfRelaT::Info>(std::stoull(elements[1])));
        reloc.setSymbolTableIndex(static_cast<typename ElfRelaT::Info>(std::stoull(elements[2])));
        reloc.addend = static_cast<typename ElfRelaT::Addend>(std::stoll(elements[3]));
    }

    return relocs;
}

template <Elf::ELF_IDENTIFIER_CLASS numBits>
std::vector<typename ZebinEncoder<numBits>::ElfSymT> ZebinEncoder<numBits>::parseSymbols(const std::vector<std::string> &symbolsFile, ElfEncoderT &encoder, size_t &outNumLocalSymbols, SecNameToIdMapT secNameToId) {
    std::vector<ElfSymT> symbols;
    symbols.resize(symbolsFile.size() - 1);
    outNumLocalSymbols = 0U;

    for (size_t lineId = 1U; lineId < symbolsFile.size(); lineId++) {
        auto &line = symbolsFile[lineId];
        auto elements = parseLine(line);
        UNRECOVERABLE_IF(elements.size() != 7);

        auto symbolId = std::stoull(elements[0]);
        auto symbolName = elements[1].substr(1);
        auto sectionName = elements[2].substr(1);
        auto symbolValue = std::stoull(elements[3]);
        auto symbolType = std::stoi(elements[4]);
        auto symbolVisibility = std::stoi(elements[5]);
        auto symbolBinding = std::stoi(elements[6]);

        UNRECOVERABLE_IF(symbolId >= symbols.size());
        auto &symbol = symbols[static_cast<size_t>(symbolId)];
        symbol.name = static_cast<typename ElfSymT::Name>((symbolName == "UNDEF") ? 0 : encoder.appendSectionName(symbolName));
        symbol.shndx = static_cast<typename ElfSymT::Shndx>((sectionName == "UNDEF") ? 0 : static_cast<uint16_t>(secNameToId[sectionName]));
        symbol.value = static_cast<typename ElfSymT::Value>(symbolValue);
        symbol.setType(static_cast<typename ElfSymT::Info>(symbolType));
        symbol.setVisibility(static_cast<typename ElfSymT::Other>(symbolVisibility));
        symbol.setBinding(static_cast<typename ElfSymT::Info>(symbolBinding));

        if (symbol.getBinding() == Elf::STB_LOCAL) {
            outNumLocalSymbols = lineId;
        }
    }

    return symbols;
}

} // namespace NEO::ZebinManipulator
