/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/const_stringref.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class OclocArgHelper;
struct IgaWrapper;
namespace NEO {

namespace Elf {
struct IntelGTNote;

template <ELF_IDENTIFIER_CLASS numBits>
struct Elf;

template <ELF_IDENTIFIER_CLASS numBits>
struct ElfEncoder;
} // namespace Elf

namespace ZebinManipulator {

struct SectionInfo {
    std::string name;
    uint32_t type;
};

struct Arguments {
    std::string pathToDump = "";
    std::string binaryFile = "";
    bool showHelp = false;
    bool skipIGAdisassembly = false;
};

enum BinaryFormats {
    PatchTokens,
    Zebin32b,
    Zebin64b
};

using ErrorCode = int;

ErrorCode parseIntelGTNotesSectionForDevice(const std::vector<Elf::IntelGTNote> &intelGTNotes, IgaWrapper *iga);
ErrorCode validateInput(const std::vector<std::string> &args, IgaWrapper *iga, OclocArgHelper *argHelper, Arguments &outArguments);

BinaryFormats getBinaryFormatForAssemble(OclocArgHelper *argHelper, const std::vector<std::string> &args);
BinaryFormats getBinaryFormatForDisassemble(OclocArgHelper *argHelper, const std::vector<std::string> &args);
bool is64BitZebin(OclocArgHelper *argHelper, const std::string &sectionsInfoFilepath);

constexpr ConstStringRef sectionsInfoFilename = "sections.txt";

template <Elf::ELF_IDENTIFIER_CLASS numBits>
class ZebinDecoder {
  public:
    using ElfT = Elf::Elf<numBits>;
    using ElfRelT = Elf::ElfRel<numBits>;
    using ElfRelaT = Elf::ElfRela<numBits>;
    using ElfSymT = Elf::ElfSymbolEntry<numBits>;

    ZebinDecoder(OclocArgHelper *argHelper);
    ~ZebinDecoder();
    ErrorCode decode();
    ErrorCode validateInput(const std::vector<std::string> &args);
    void printHelp();

  protected:
    MOCKABLE_VIRTUAL ErrorCode decodeZebin(ArrayRef<const uint8_t> zebin, ElfT &outElf);
    MOCKABLE_VIRTUAL void dump(ConstStringRef name, ArrayRef<const uint8_t> data);
    MOCKABLE_VIRTUAL void dumpKernelData(ConstStringRef name, ArrayRef<const uint8_t> data);
    MOCKABLE_VIRTUAL void dumpRel(ConstStringRef name, ArrayRef<const uint8_t> data);
    MOCKABLE_VIRTUAL void dumpRela(ConstStringRef name, ArrayRef<const uint8_t> data);
    MOCKABLE_VIRTUAL void dumpSymtab(ElfT &elf, ArrayRef<const uint8_t> symtabData);
    MOCKABLE_VIRTUAL std::vector<SectionInfo> dumpElfSections(ElfT &elf);
    MOCKABLE_VIRTUAL void dumpSectionInfo(const std::vector<SectionInfo> &sectionInfos);
    MOCKABLE_VIRTUAL std::vector<Elf::IntelGTNote> getIntelGTNotes(ElfT &elf);

  public:
    bool &showHelp = arguments.showHelp;

  protected:
    Arguments arguments;
    OclocArgHelper *argHelper;
    std::unique_ptr<IgaWrapper> iga;
};

template <Elf::ELF_IDENTIFIER_CLASS numBits>
class ZebinEncoder {
  public:
    using ElfEncoderT = Elf::ElfEncoder<numBits>;
    using SecNameToIdMapT = std::unordered_map<std::string, size_t>;
    using ElfSecHdrT = Elf::ElfSectionHeader<numBits>;
    using ElfRelT = Elf::ElfRel<numBits>;
    using ElfRelaT = Elf::ElfRela<numBits>;
    using ElfSymT = Elf::ElfSymbolEntry<numBits>;

    ZebinEncoder(OclocArgHelper *argHelper);
    ~ZebinEncoder();
    ErrorCode encode();
    ErrorCode validateInput(const std::vector<std::string> &args);
    void printHelp();

  protected:
    MOCKABLE_VIRTUAL ErrorCode loadSectionsInfo(std::vector<SectionInfo> &sectionInfos);
    MOCKABLE_VIRTUAL ErrorCode checkIfAllFilesExist(const std::vector<SectionInfo> &sectionInfos);
    MOCKABLE_VIRTUAL std::vector<char> getIntelGTNotesSection(const std::vector<SectionInfo> &sectionInfos);
    MOCKABLE_VIRTUAL std::vector<Elf::IntelGTNote> getIntelGTNotes(const std::vector<char> &intelGtNotesSection);

    MOCKABLE_VIRTUAL ErrorCode appendSections(ElfEncoderT &encoder, const std::vector<SectionInfo> &sectionInfos);
    MOCKABLE_VIRTUAL ErrorCode appendRel(ElfEncoderT &encoder, const SectionInfo &section, size_t targetSecId, size_t symtabSecId);
    MOCKABLE_VIRTUAL ErrorCode appendRela(ElfEncoderT &encoder, const SectionInfo &section, size_t targetSecId, size_t symtabSecId);
    MOCKABLE_VIRTUAL ErrorCode appendKernel(ElfEncoderT &encoder, const SectionInfo &section);
    MOCKABLE_VIRTUAL ErrorCode appendSymtab(ElfEncoderT &encoder, const SectionInfo &section, size_t strtabId, SecNameToIdMapT secNameToId);
    MOCKABLE_VIRTUAL ErrorCode appendOther(ElfEncoderT &encoder, const SectionInfo &section);

    MOCKABLE_VIRTUAL std::string getFilePath(const std::string &filename);
    MOCKABLE_VIRTUAL std::string parseKernelAssembly(ArrayRef<const char> kernelAssembly);
    MOCKABLE_VIRTUAL std::vector<std::string> parseLine(const std::string &line);
    MOCKABLE_VIRTUAL std::vector<ElfRelT> parseRel(const std::vector<std::string> &relocationsFile);
    MOCKABLE_VIRTUAL std::vector<ElfRelaT> parseRela(const std::vector<std::string> &relocationsFile);
    MOCKABLE_VIRTUAL std::vector<ElfSymT> parseSymbols(const std::vector<std::string> &symbolsFile, ElfEncoderT &encoder, size_t &outNumLocalSymbols, SecNameToIdMapT secNameToId);

  public:
    bool &showHelp = arguments.showHelp;

  protected:
    Arguments arguments;
    OclocArgHelper *argHelper;
    std::unique_ptr<IgaWrapper> iga;
};

}; // namespace ZebinManipulator
} // namespace NEO