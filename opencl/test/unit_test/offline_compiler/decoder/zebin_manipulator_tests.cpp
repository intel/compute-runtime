/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/decoder/zebin_manipulator.h"
#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_elf.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/offline_compiler/decoder/mock/mock_iga_wrapper.h"
#include "opencl/test/unit_test/offline_compiler/decoder/mock/mock_zebin_decoder.h"
#include "opencl/test/unit_test/offline_compiler/decoder/mock/mock_zebin_encoder.h"
#include "opencl/test/unit_test/offline_compiler/mock/mock_argument_helper.h"

#include <platforms.h>

template <NEO::Elf::ElfIdentifierClass numBits>
struct MockZebin {
    MockZebin() {
        NEO::Elf::ElfEncoder<numBits> encoder;
        encoder.getElfFileHeader().machine = NEO::Elf::ElfMachine::EM_INTELGT;
        encoder.getElfFileHeader().type = NEO::Zebin::Elf::ElfTypeZebin::ET_ZEBIN_EXE;

        uint8_t kernelData[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38};
        encoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::textPrefix.str() + "exit_kernel", {kernelData, 8});

        uint8_t dataGlobal[] = {0x00, 0x01, 0x02, 0x03, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
        encoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::dataGlobal, {dataGlobal, 12});

        uint8_t debugInfo[] = {0x10, 0x11, 0x12, 0x13, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
        encoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Zebin::Elf::SectionNames::debugInfo, {debugInfo, 12});

        uint8_t spvData[] = {0x20, 0x21, 0x22, 0x23};
        encoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_SPIRV, NEO::Zebin::Elf::SectionNames::spv, {spvData, 4});

        NEO::ConstStringRef buildOptions = "";
        encoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_MISC, NEO::Zebin::Elf::SectionNames::buildOptions,
                              {reinterpret_cast<const uint8_t *>(buildOptions.data()), buildOptions.size()});

        std::string zeInfo = R"===('
---
version:         '1.15'
kernels:
  - name:            exit_kernel
    execution_env:
      grf_count:       128
      simd_size:       16
...
)===";
        encoder.appendSection(NEO::Zebin::Elf::SHT_ZEBIN_ZEINFO, NEO::Zebin::Elf::SectionNames::zeInfo,
                              {reinterpret_cast<const uint8_t *>(zeInfo.data()), zeInfo.size()});

        NEO::Zebin::Elf::ZebinTargetFlags flags;
        flags.packed = 0U;
        auto intelGTNotes = ZebinTestData::createIntelGTNoteSection(IGFX_DG2, IGFX_XE_HPG_CORE, flags, versionToString({1, 15}));
        encoder.appendSection(NEO::Elf::SHT_NOTE, NEO::Zebin::Elf::SectionNames::noteIntelGT,
                              {intelGTNotes.data(), intelGTNotes.size()});

        NEO::Elf::ElfRel<numBits> dataRelocation;
        dataRelocation.offset = 0x4;
        dataRelocation.setRelocationType(NEO::Zebin::Elf::RelocTypeZebin::R_ZE_SYM_ADDR);
        dataRelocation.setSymbolTableIndex(2);
        auto &dataRelSec = encoder.appendSection(NEO::Elf::SHT_REL, NEO::Elf::SpecialSectionNames::relPrefix.str() + NEO::Zebin::Elf::SectionNames::dataGlobal.str(),
                                                 {reinterpret_cast<const uint8_t *>(&dataRelocation), sizeof(dataRelocation)});
        dataRelSec.link = 10;
        dataRelSec.info = 2;

        NEO::Elf::ElfRela<numBits> debugRelocation;
        debugRelocation.offset = 0x4;
        debugRelocation.setRelocationType(NEO::Zebin::Elf::RelocTypeZebin::R_ZE_SYM_ADDR);
        debugRelocation.setSymbolTableIndex(1);
        auto &debugDataRelaSec = encoder.appendSection(NEO::Elf::SHT_RELA, NEO::Elf::SpecialSectionNames::relaPrefix.str() + NEO::Zebin::Elf::SectionNames::debugInfo.str(),
                                                       {reinterpret_cast<const uint8_t *>(&debugRelocation), sizeof(debugRelocation)});
        debugDataRelaSec.link = 10;
        debugDataRelaSec.info = 3;

        NEO::Elf::ElfSymbolEntry<numBits> symbols[3] = {};
        symbols[1].shndx = 1;
        symbols[1].value = 0x0;
        symbols[1].name = encoder.appendSectionName("exit_kernel");
        symbols[1].setBinding(NEO::Elf::SymbolTableBind::STB_LOCAL);
        symbols[1].setType(NEO::Elf::SymbolTableType::STT_FUNC);

        symbols[2].shndx = 2;
        symbols[2].value = 0x0;
        symbols[2].name = encoder.appendSectionName("var_x");
        symbols[2].setBinding(NEO::Elf::SymbolTableBind::STB_GLOBAL);
        symbols[2].setType(NEO::Elf::SymbolTableType::STT_OBJECT);

        auto &symtabSec = encoder.appendSection(NEO::Elf::SHT_SYMTAB, NEO::Zebin::Elf::SectionNames::symtab,
                                                {reinterpret_cast<const uint8_t *>(symbols), sizeof(symbols)});
        symtabSec.link = 11;
        symtabSec.info = 2;
        storage = encoder.encode();
    }
    std::vector<uint8_t> storage;
};

TEST(ZebinManipulatorTests, GivenValidZebinWhenItIsDisassembledAndAssembledBackThenItIsSameBinary) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });
    MockZebin<NEO::Elf::EI_CLASS_32> mockZebin32;
    MockZebin<NEO::Elf::EI_CLASS_64> mockZebin64;
    for (const auto zebin : {&mockZebin32.storage, &mockZebin64.storage}) {
        // Disassemble
        uint32_t disasmNumOutputs;
        uint8_t **disasmDataOutputs;
        uint64_t *disasmLenOutputs;
        char **disasmNameOutputs;
        std::array disasmArgs = {
            "ocloc",
            "disasm",
            "-file",
            "zebin.bin",
            "-skip-asm-translation"};

        uint32_t numSources = 1;
        const uint8_t *dataSources[1] = {zebin->data()};
        const uint64_t lenSources[1] = {zebin->size()};
        const char *nameSources[1] = {"zebin.bin"};

        int retVal = oclocInvoke(static_cast<uint32_t>(disasmArgs.size()), disasmArgs.data(),
                                 numSources, dataSources, lenSources, nameSources,
                                 0, nullptr, nullptr, nullptr,
                                 &disasmNumOutputs, &disasmDataOutputs, &disasmLenOutputs, &disasmNameOutputs);
        EXPECT_EQ(OCLOC_SUCCESS, retVal);

        // Assemble
        std::array asmArgs = {
            "ocloc",
            "asm",
            "-file",
            "zebin.bin"};

        uint32_t asmNumOutputs;
        uint8_t **asmDataOutputs;
        uint64_t *asmLenOutputs;
        char **asmNameOutputs;

        retVal = oclocInvoke(static_cast<uint32_t>(asmArgs.size()), asmArgs.data(),
                             disasmNumOutputs, const_cast<const uint8_t **>(disasmDataOutputs), disasmLenOutputs, const_cast<const char **>(disasmNameOutputs),
                             0, nullptr, nullptr, nullptr,
                             &asmNumOutputs, &asmDataOutputs, &asmLenOutputs, &asmNameOutputs);
        EXPECT_EQ(OCLOC_SUCCESS, retVal);

        // Check
        ArrayRef<uint8_t> rebuiltZebin(asmDataOutputs[0], static_cast<size_t>(asmLenOutputs[0]));
        EXPECT_EQ(zebin->size(), rebuiltZebin.size());
        if (zebin->size() == rebuiltZebin.size()) {
            EXPECT_EQ(0, std::memcmp(zebin->data(), rebuiltZebin.begin(), zebin->size()));
        }

        oclocFreeOutput(&disasmNumOutputs, &disasmDataOutputs, &disasmLenOutputs, &disasmNameOutputs);
        oclocFreeOutput(&asmNumOutputs, &asmDataOutputs, &asmLenOutputs, &asmNameOutputs);
    }
};

struct ZebinManipulatorValidateArgumentsFixture {
    ZebinManipulatorValidateArgumentsFixture() : argHelper(filesMap) {}
    void setUp() {}
    void tearDown() {}
    MockOclocArgHelper::FilesMap filesMap;
    MockOclocArgHelper argHelper;
    MockIgaWrapper iga;
    NEO::Zebin::Manipulator::Arguments arguments;
};

using ZebinManipulatorValidateInputTests = Test<ZebinManipulatorValidateArgumentsFixture>;
TEST_F(ZebinManipulatorValidateInputTests, GivenValidInputWhenValidatingInputThenPopulateArgumentsCorrectly) {
    std::vector<std::string> args = {"ocloc",
                                     "asm/disasm",
                                     "-file",
                                     "zebin.bin",
                                     "-device",
                                     "dg2",
                                     "-dump",
                                     "./dump/",
                                     "-q",
                                     "-skip-asm-translation"};

    auto retVal = NEO::Zebin::Manipulator::validateInput(args, &iga, &argHelper, arguments);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);

    EXPECT_EQ("zebin.bin", arguments.binaryFile);
    EXPECT_EQ("./dump/", arguments.pathToDump);
    EXPECT_TRUE(iga.setProductFamilyWasCalled);
    EXPECT_TRUE(argHelper.getPrinterRef().isSuppressed());
    EXPECT_TRUE(arguments.skipIGAdisassembly);
}

TEST_F(ZebinManipulatorValidateInputTests, GivenHelpArgumentWhenValidatingInputThenShowHelpIsSet) {
    std::vector<std::string> args = {"ocloc",
                                     "asm/disasm",
                                     "--help"};
    auto retVal = NEO::Zebin::Manipulator::validateInput(args, &iga, &argHelper, arguments);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    EXPECT_TRUE(arguments.showHelp);
}

TEST_F(ZebinManipulatorValidateInputTests, GivenInvalidInputWhenValidatingInputThenReturnsError) {
    std::vector<std::string> args = {"ocloc",
                                     "asm/disasm",
                                     "-unknown_arg"};

    StreamCapture capture;
    capture.captureStdout();
    auto retVal = NEO::Zebin::Manipulator::validateInput(args, &iga, &argHelper, arguments);
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, retVal);
    EXPECT_EQ("Unknown argument -unknown_arg\n", output);
}

TEST_F(ZebinManipulatorValidateInputTests, GivenMissingFileWhenValidatingInputThenReturnsError) {
    std::vector<std::string> args = {"ocloc",
                                     "asm/disasm",
                                     "-dump",
                                     "./dump/"};

    StreamCapture capture;
    capture.captureStdout();
    auto retVal = NEO::Zebin::Manipulator::validateInput(args, &iga, &argHelper, arguments);
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, retVal);
    EXPECT_EQ("Error: Missing -file argument\n", output);
}

TEST_F(ZebinManipulatorValidateInputTests, GivenMissingSecondPartOfTheArgumentWhenValidatingInputThenReturnsError) {
    std::vector<std::string> args = {"ocloc",
                                     "asm/disasm",
                                     "-arg"};
    for (const auto halfArg : {"-file", "-device", "-dump"}) {
        args[2] = halfArg;

        StreamCapture capture;
        capture.captureStdout();
        auto retVal = NEO::Zebin::Manipulator::validateInput(args, &iga, &argHelper, arguments);
        const auto output{capture.getCapturedStdout()};

        EXPECT_EQ(OCLOC_INVALID_COMMAND_LINE, retVal);
        const auto expectedOutput = "Unknown argument " + std::string(halfArg) + "\n";
        EXPECT_EQ(expectedOutput, output);
    }
}

TEST_F(ZebinManipulatorValidateInputTests, GivenValidArgsButDumpNotSpecifiedWhenValidatingInputThenReturnSuccessAndSetDumpToDefaultAndPrintWarning) {
    std::vector<std::string> args = {"ocloc",
                                     "asm/disasm",
                                     "-file",
                                     "binary.bin"};

    StreamCapture capture;
    capture.captureStdout();
    auto retVal = NEO::Zebin::Manipulator::validateInput(args, &iga, &argHelper, arguments);
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    EXPECT_EQ("Warning: Path to dump -dump not specified. Using \"./dump/\" as dump folder.\n", output);
}

TEST(ZebinManipulatorTests, GivenIntelGTNotesWithProductFamilyWhenParsingIntelGTNoteSectionsForDeviceThenIgaProductFamilyIsSet) {
    PRODUCT_FAMILY productFamily = PRODUCT_FAMILY::IGFX_DG2;
    std::vector<NEO::Zebin::Elf::IntelGTNote> intelGTnotes;
    intelGTnotes.resize(1);
    intelGTnotes[0].type = NEO::Zebin::Elf::IntelGTSectionType::productFamily;
    intelGTnotes[0].data = ArrayRef<const uint8_t>::fromAny(&productFamily, 1);

    auto iga = std::make_unique<MockIgaWrapper>();
    auto retVal = NEO::Zebin::Manipulator::parseIntelGTNotesSectionForDevice(intelGTnotes, iga.get(), nullptr);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    EXPECT_TRUE(iga->setProductFamilyWasCalled);
}

TEST(ZebinManipulatorTests, GivenIntelGTNotesWithGfxCoreFamilyWhenParsingIntelGTNoteSectionsForDeviceThenIgaGfxCoreIsSet) {
    GFXCORE_FAMILY gfxCore = GFXCORE_FAMILY::IGFX_XE_HPG_CORE;
    std::vector<NEO::Zebin::Elf::IntelGTNote> intelGTnotes;
    intelGTnotes.resize(1);
    intelGTnotes[0].type = NEO::Zebin::Elf::IntelGTSectionType::gfxCore;
    intelGTnotes[0].data = ArrayRef<const uint8_t>::fromAny(&gfxCore, 1);

    auto iga = std::make_unique<MockIgaWrapper>();
    auto retVal = NEO::Zebin::Manipulator::parseIntelGTNotesSectionForDevice(intelGTnotes, iga.get(), nullptr);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    EXPECT_TRUE(iga->setGfxCoreWasCalled);
}

TEST(ZebinManipulatorTests, GivenIntelGTNotesWithValidProductConfigWhenParsingIntelGTNoteSectionsForDeviceThenIgaProductFamilyIsSetAndSuccessIsReturned) {
    MockOclocArgHelper::FilesMap files;
    files.insert({"binary.bin", "000000000000000"});
    MockOclocArgHelper argHelper(files);

    const auto &aotInfo = argHelper.productConfigHelper->getDeviceAotInfo().back();
    auto productConfig = aotInfo.aotConfig;
    std::vector<NEO::Zebin::Elf::IntelGTNote> intelGTnotes;
    intelGTnotes.resize(1);
    intelGTnotes[0].type = NEO::Zebin::Elf::IntelGTSectionType::productConfig;
    intelGTnotes[0].data = ArrayRef<const uint8_t>::fromAny(&productConfig, 1u);

    auto iga = std::make_unique<MockIgaWrapper>();
    auto retVal = NEO::Zebin::Manipulator::parseIntelGTNotesSectionForDevice(intelGTnotes, iga.get(), &argHelper);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    EXPECT_TRUE(iga->setProductFamilyWasCalled);
}

TEST(ZebinManipulatorTests, GivenIntelGTNotesWithInvalidProductConfigWhenParsingIntelGTNoteSectionsForDeviceThenReturnError) {
    MockOclocArgHelper::FilesMap files;
    files.insert({"binary.bin", "000000000000000"});
    MockOclocArgHelper argHelper(files);

    AOT::PRODUCT_CONFIG productConfig = AOT::PRODUCT_CONFIG::UNKNOWN_ISA;
    std::vector<NEO::Zebin::Elf::IntelGTNote> intelGTnotes;
    intelGTnotes.resize(1);
    intelGTnotes[0].type = NEO::Zebin::Elf::IntelGTSectionType::productConfig;
    intelGTnotes[0].data = ArrayRef<const uint8_t>::fromAny(&productConfig, 1u);

    auto iga = std::make_unique<MockIgaWrapper>();
    auto retVal = NEO::Zebin::Manipulator::parseIntelGTNotesSectionForDevice(intelGTnotes, iga.get(), &argHelper);
    EXPECT_EQ(OCLOC_INVALID_DEVICE, retVal);
}

TEST(ZebinManipulatorTests, GivenIntelGTNotesWithoutProductFamilyOrGfxCoreFamilyEntryWhenParsingIntelGTNoteSectionsForDeviceThenReturnError) {
    std::vector<NEO::Zebin::Elf::IntelGTNote> intelGTnotes;

    auto iga = std::make_unique<MockIgaWrapper>();
    auto retVal = NEO::Zebin::Manipulator::parseIntelGTNotesSectionForDevice(intelGTnotes, iga.get(), nullptr);
    EXPECT_EQ(OCLOC_INVALID_DEVICE, retVal);
}

TEST(ZebinManipulatorTests, GivenNonZebinBinaryWhenGetBinaryFormatForDisassembleThenReturnPatchTokensFormat) {
    MockOclocArgHelper::FilesMap files;
    files.insert({"binary.bin", "000000000000000"});
    MockOclocArgHelper argHelper(files);

    auto format = NEO::Zebin::Manipulator::getBinaryFormatForDisassemble(&argHelper, {"ocloc", "disasm", "-file", "binary.bin"});
    EXPECT_EQ(NEO::Zebin::Manipulator::BinaryFormats::PatchTokens, format);
}

TEST(ZebinManipulatorTests, GivenEmptySectionsInfoWhenCheckingIfIs64BitZebinThenReturnFalse) {
    MockOclocArgHelper::FilesMap files;
    files.insert({"sections.txt", ""});
    MockOclocArgHelper argHelper(files);
    auto retVal = NEO::Zebin::Manipulator::is64BitZebin(&argHelper, "sections.txt");
    EXPECT_FALSE(retVal);
}

TEST(ZebinManipulatorTests, GivenInvalidSectionsInfoWhenCheckingIfIs64BitZebinThenReturnFalse) {
    MockOclocArgHelper::FilesMap files;
    files.insert({"sections.txt", "ElfType"});
    MockOclocArgHelper argHelper(files);
    auto retVal = NEO::Zebin::Manipulator::is64BitZebin(&argHelper, "sections.txt");
    EXPECT_FALSE(retVal);
}

template <NEO::Elf::ElfIdentifierClass numBits>
struct ZebinDecoderFixture {
    ZebinDecoderFixture() : argHelper(filesMap), decoder(&argHelper) {
        argHelper.messagePrinter.setSuppressMessages(true);
    };
    void setUp() {}
    void tearDown() {}
    std::string getOutput() { return argHelper.messagePrinter.getLog().str(); }
    MockOclocArgHelper::FilesMap filesMap;
    MockOclocArgHelper argHelper;
    MockZebinDecoder<numBits> decoder;
};

using ZebinDecoderTests = Test<ZebinDecoderFixture<NEO::Elf::EI_CLASS_64>>;
TEST_F(ZebinDecoderTests, GivenErrorWhenDecodingZebinWhenDecodeThenErrorIsReturned) {
    decoder.returnValueDecodeZebin = OCLOC_INVALID_FILE;
    const auto retVal = decoder.decode();
    EXPECT_EQ(decoder.returnValueDecodeZebin, retVal);
    EXPECT_EQ("Error while decoding zebin.\n", getOutput());
}

TEST_F(ZebinDecoderTests, GivenNoIntelGTNotesWhenDecodeThenErrorIsReturned) {
    decoder.returnValueGetIntelGTNotes = {};
    const auto retVal = decoder.decode();
    EXPECT_EQ(OCLOC_INVALID_FILE, retVal);
    EXPECT_EQ("Error missing or invalid Intel GT Notes section.\n", getOutput());
}

TEST_F(ZebinDecoderTests, GivenInvalidIntelGTNotesWhenDecodeThenErrorIsReturned) {
    decoder.returnValueGetIntelGTNotes = {};
    decoder.returnValueGetIntelGTNotes.resize(1);
    const std::string zebinVersion = "1.0";
    decoder.returnValueGetIntelGTNotes[0].type = NEO::Zebin::Elf::IntelGTSectionType::zebinVersion;
    decoder.returnValueGetIntelGTNotes[0].data = ArrayRef<const uint8_t>::fromAny(zebinVersion.data(), zebinVersion.length());

    const auto retVal = decoder.decode();
    EXPECT_EQ(OCLOC_INVALID_DEVICE, retVal);
    EXPECT_EQ("Error while parsing Intel GT Notes section for device.\n", getOutput());
}

TEST_F(ZebinDecoderTests, GivenElfWithInvalidIntelGTNotesWhenGetIntelGTNotesThenEmptyVectorIsReturned) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoder;
    NEO::Zebin::Elf::ZebinTargetFlags flags;
    flags.packed = 0U;
    const uint8_t intelGTNotes[8] = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
    elfEncoder.appendSection(NEO::Elf::SHT_NOTE, NEO::Zebin::Elf::SectionNames::noteIntelGT,
                             {intelGTNotes, 8U});
    auto elfBinary = elfEncoder.encode();
    ASSERT_FALSE(elfBinary.empty());
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(elfBinary, errors, warnings);
    ASSERT_TRUE(errors.empty()) << errors;
    ASSERT_TRUE(warnings.empty()) << warnings;

    decoder.callBaseGetIntelGTNotes = true;
    auto retVal = decoder.getIntelGTNotes(elf);
    EXPECT_TRUE(retVal.empty());
    EXPECT_FALSE(getOutput().empty());
}

TEST_F(ZebinDecoderTests, GivenElfWithValidIntelGTNotesWhenGetIntelGTNotesThenIntelGTNotesAreReturned) {
    NEO::Elf::ElfEncoder<NEO::Elf::EI_CLASS_64> elfEncoder;
    NEO::Zebin::Elf::ZebinTargetFlags flags;
    flags.packed = 0U;
    auto intelGTNotes = ZebinTestData::createIntelGTNoteSection(IGFX_DG2, IGFX_XE_HPG_CORE, flags, versionToString({1, 15}));
    elfEncoder.appendSection(NEO::Elf::SHT_NOTE, NEO::Zebin::Elf::SectionNames::noteIntelGT,
                             {intelGTNotes.data(), intelGTNotes.size()});
    auto elfBinary = elfEncoder.encode();
    ASSERT_FALSE(elfBinary.empty());
    std::string errors, warnings;
    auto elf = NEO::Elf::decodeElf(elfBinary, errors, warnings);
    ASSERT_TRUE(errors.empty()) << errors;
    ASSERT_TRUE(warnings.empty()) << warnings;

    decoder.callBaseGetIntelGTNotes = true;
    auto retVal = decoder.getIntelGTNotes(elf);
    EXPECT_FALSE(retVal.empty());
    EXPECT_TRUE(getOutput().empty());
}

TEST_F(ZebinDecoderTests, GivenSkipIGADisassemblyWhenDecodeThenDoNotParseIntelGTNotes) {
    decoder.arguments.skipIGAdisassembly = true;

    const auto retVal = decoder.decode();
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    EXPECT_FALSE(decoder.callBaseDecodeZebin);
}

TEST_F(ZebinDecoderTests, GivenNoFailsWhenDecodeThenSuccessIsReturned) {
    decoder.returnValueGetIntelGTNotes = {};
    decoder.returnValueGetIntelGTNotes.resize(1);
    const PRODUCT_FAMILY productFamily = PRODUCT_FAMILY::IGFX_DG2;
    decoder.returnValueGetIntelGTNotes[0].type = NEO::Zebin::Elf::IntelGTSectionType::productFamily;
    decoder.returnValueGetIntelGTNotes[0].data = ArrayRef<const uint8_t>::fromAny(&productFamily, 1U);

    const auto retVal = decoder.decode();
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
}

TEST_F(ZebinDecoderTests, GivenInvalidZebinWhenDecodeZebinThenErrorIsReturned) {
    decoder.callBaseDecodeZebin = true;
    uint8_t invalidFile[16] = {0};
    NEO::Elf::Elf<NEO::Elf::EI_CLASS_64> elf;
    const auto retVal = decoder.decodeZebin(ArrayRef<const uint8_t>::fromAny(invalidFile, 16), elf);
    EXPECT_EQ(OCLOC_INVALID_FILE, retVal);
    EXPECT_EQ("decodeElf error: Invalid or missing ELF header\n", getOutput());
}

TEST_F(ZebinDecoderTests, GivenSkipIgaDisassemblyWhenDumpKernelDataThenDumpAsBinary) {
    argHelper.interceptOutput = true;
    decoder.arguments.skipIGAdisassembly = true;
    const uint8_t kernel[16] = {0};
    decoder.dumpKernelData(".text.kernel", {kernel, 16});
    EXPECT_FALSE(decoder.mockIga->disasmWasCalled);
    auto dump = argHelper.interceptedFiles.find(".text.kernel");
    ASSERT_NE(dump, argHelper.interceptedFiles.end());
    EXPECT_EQ(16U, dump->second.size());
}

TEST_F(ZebinDecoderTests, GivenFailOnTryDissasembleGenISAWhenDumpKernelDataThenDumpAsBinary) {
    argHelper.interceptOutput = true;
    decoder.mockIga->asmToReturn = "";
    const uint8_t kernel[16] = {0};
    decoder.dumpKernelData(".text.kernel", {kernel, 16});
    EXPECT_TRUE(decoder.mockIga->disasmWasCalled);
    auto dump = argHelper.interceptedFiles.find(".text.kernel");
    ASSERT_NE(dump, argHelper.interceptedFiles.end());
    EXPECT_EQ(16U, dump->second.size());
}

TEST_F(ZebinDecoderTests, GivenSuccessfulDissasembleGenIsaWhenDumpKernelDataThenDumpAsAssembly) {
    argHelper.interceptOutput = true;
    std::string asmToReturn = "assembly";
    decoder.mockIga->asmToReturn = asmToReturn;
    const uint8_t kernel[16] = {0};
    decoder.dumpKernelData(".text.kernel", {kernel, 16});
    EXPECT_TRUE(decoder.mockIga->disasmWasCalled);
    auto dump = argHelper.interceptedFiles.find(".text.kernel.asm");
    ASSERT_NE(dump, argHelper.interceptedFiles.end());
    EXPECT_EQ(asmToReturn, dump->second);
}

TEST_F(ZebinDecoderTests, WhenPrintHelpIsCalledThenHelpIsPrinted) {
    decoder.printHelp();
    EXPECT_FALSE(getOutput().empty());
}

template <NEO::Elf::ElfIdentifierClass numBits>
struct ZebinEncoderFixture {
    ZebinEncoderFixture() : argHelper(filesMap), encoder(&argHelper) {
        argHelper.messagePrinter.setSuppressMessages(true);
    };
    void setUp() {}
    void tearDown() {}
    std::string getOutput() { return argHelper.messagePrinter.getLog().str(); }
    MockOclocArgHelper::FilesMap filesMap;
    MockOclocArgHelper argHelper;
    MockZebinEncoder<numBits> encoder;
};

using ZebinEncoderTests = Test<ZebinEncoderFixture<NEO::Elf::EI_CLASS_64>>;
TEST_F(ZebinEncoderTests, GivenErrorOnLoadSectionsInfoWhenEncodeThenErrorIsReturned) {
    encoder.retValLoadSectionsInfo = OCLOC_INVALID_FILE;
    auto retVal = encoder.encode();
    EXPECT_EQ(encoder.retValLoadSectionsInfo, retVal);
    EXPECT_EQ("Error while loading sections file.\n", getOutput());
}

TEST_F(ZebinEncoderTests, GivenErrorOnCheckIfAllFilesExistWhenEncodeThenErrorIsReturned) {
    encoder.retValCheckIfAllFilesExist = OCLOC_INVALID_FILE;
    auto retVal = encoder.encode();
    EXPECT_EQ(encoder.retValCheckIfAllFilesExist, retVal);
    EXPECT_EQ("Error: Missing one or more section files.\n", getOutput());
}

TEST_F(ZebinEncoderTests, GivenMissingIntelGTNotesWhenEncodeThenErrorIsReturned) {
    encoder.retValGetIntelGTNotes = {};
    auto retVal = encoder.encode();
    EXPECT_EQ(OCLOC_INVALID_DEVICE, retVal);
    EXPECT_EQ("Error while parsing Intel GT Notes section for device.\n", getOutput());
}

TEST_F(ZebinEncoderTests, GivenErrorOnAppendingSectionsWhenEncodeThenErrorIsReturned) {
    encoder.retValAppendSections = OCLOC_INVALID_FILE;
    auto retVal = encoder.encode();
    EXPECT_EQ(OCLOC_INVALID_FILE, retVal);
    EXPECT_EQ("Error while appending elf sections.\n", getOutput());
}

TEST_F(ZebinEncoderTests, GivenNoIntelGTNotesWhenGetIntelGTNotesSectionThenEmptyVectorIsReturned) {
    std::vector<NEO::Zebin::Manipulator::SectionInfo> sectionInfos = {{".note.notIntelGT", NEO::Elf::SHT_NOTE}};
    encoder.callBaseGetIntelGTNotesSection = true;
    auto retVal = encoder.getIntelGTNotesSection(sectionInfos);
    EXPECT_TRUE(retVal.empty());
}

TEST_F(ZebinEncoderTests, GivenInvalidSectionsInfoFileWhenLoadSectionsInfoThenErrorIsReturned) {
    filesMap.insert({NEO::Zebin::Manipulator::sectionsInfoFilename.str(), ""});
    encoder.callBaseLoadSectionsInfo = true;
    std::vector<NEO::Zebin::Manipulator::SectionInfo> sectionInfos;
    auto retVal = encoder.loadSectionsInfo(sectionInfos);
    EXPECT_EQ(OCLOC_INVALID_FILE, retVal);
    EXPECT_TRUE(sectionInfos.empty());
}

TEST_F(ZebinEncoderTests, GivenInvalidIntelGTNotesSectionWhenGetIntelGTNotesThenEmptyVectorIsReturned) {
    encoder.callBaseGetIntelGTNotes = true;
    std::vector<char> invalidNotes = {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7};
    auto retVal = encoder.getIntelGTNotes(invalidNotes);
    EXPECT_TRUE(retVal.empty());
}

TEST_F(ZebinEncoderTests, GivenInvalidSymtabFileWhenAppendSymtabThenErrorIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });

    MockElfEncoder elfEncoder;
    NEO::Zebin::Manipulator::SectionInfo sectionInfo;
    sectionInfo.name = ".symtab";
    sectionInfo.type = NEO::Elf::SHT_SYMTAB;
    std::unordered_map<std::string, size_t> secNameToId;
    auto retVal = encoder.appendSymtab(elfEncoder, sectionInfo, 0U, secNameToId);
    EXPECT_EQ(OCLOC_INVALID_FILE, retVal);
    EXPECT_EQ("Error: Empty symtab file: .symtab\n", getOutput());
}

TEST_F(ZebinEncoderTests, GivenInvalidRelFileWhenAppendRelThenErrorIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });

    MockElfEncoder elfEncoder;
    NEO::Zebin::Manipulator::SectionInfo sectionInfo;
    sectionInfo.name = ".rel.text";
    sectionInfo.type = NEO::Elf::SHT_REL;
    std::unordered_map<std::string, size_t> secNameToId;
    auto retVal = encoder.appendRel(elfEncoder, sectionInfo, 1U, 2U);
    EXPECT_EQ(OCLOC_INVALID_FILE, retVal);
    EXPECT_EQ("Error: Empty relocations file: .rel.text\n", getOutput());
}

TEST_F(ZebinEncoderTests, GivenInvalidRelaFileWhenAppendRelaThenErrorIsReturned) {
    VariableBackup<decltype(NEO::IoFunctions::fopenPtr)> mockFopen(&NEO::IoFunctions::fopenPtr, [](const char *filename, const char *mode) -> FILE * { return NULL; });

    MockElfEncoder elfEncoder;
    NEO::Zebin::Manipulator::SectionInfo sectionInfo;
    sectionInfo.name = ".rela.text";
    sectionInfo.type = NEO::Elf::SHT_RELA;
    std::unordered_map<std::string, size_t> secNameToId;
    auto retVal = encoder.appendRela(elfEncoder, sectionInfo, 1U, 2U);
    EXPECT_EQ(OCLOC_INVALID_FILE, retVal);
    EXPECT_EQ("Error: Empty relocations file: .rela.text\n", getOutput());
}

TEST_F(ZebinEncoderTests, GivenAsmFileWhenAppendKernelThenTranslateItToBinaryFile) {
    MockElfEncoder elfEncoder;
    NEO::Zebin::Manipulator::SectionInfo sectionInfo;
    sectionInfo.name = ".text.kernel";
    sectionInfo.type = NEO::Elf::SHT_PROGBITS;
    filesMap.insert({sectionInfo.name + ".asm", "assembly"});

    auto retVal = encoder.appendKernel(elfEncoder, sectionInfo);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    EXPECT_TRUE(encoder.parseKernelAssemblyCalled);
}

TEST_F(ZebinEncoderTests, GivenAsmFileMissingWhenAppendKernelThenUseBinaryFile) {
    MockElfEncoder elfEncoder;
    NEO::Zebin::Manipulator::SectionInfo sectionInfo;
    sectionInfo.name = ".text.kernel";
    sectionInfo.type = NEO::Elf::SHT_PROGBITS;
    filesMap.insert({sectionInfo.name, "kernelData"});
    auto retVal = encoder.appendKernel(elfEncoder, sectionInfo);
    EXPECT_EQ(OCLOC_SUCCESS, retVal);
    EXPECT_FALSE(encoder.parseKernelAssemblyCalled);
}

TEST_F(ZebinEncoderTests, GivenSuccessfulAssemblyGenISAWhenParseKernelAssemblyThenAssemblyIsReturned) {
    encoder.mockIga->binaryToReturn = "1234";
    encoder.callBaseParseKernelAssembly = true;
    auto retVal = encoder.parseKernelAssembly({});
    EXPECT_EQ(encoder.mockIga->binaryToReturn, retVal);
}

TEST_F(ZebinEncoderTests, GivenUnsuccessfulAssemblyGenISAWhenParseKernelAssemblyThenEmptyStringIsReturned) {
    encoder.mockIga->binaryToReturn = "";
    encoder.callBaseParseKernelAssembly = true;
    auto retVal = encoder.parseKernelAssembly({});
    EXPECT_TRUE(retVal.empty());
}

TEST_F(ZebinEncoderTests, GivenMissingFileWhenCheckIfAllFilesExistThenErrorIsReturned) {
    encoder.callBaseCheckIfAllFilesExist = true;
    std::vector<NEO::Zebin::Manipulator::SectionInfo> sectionInfos;
    sectionInfos.resize(1);
    sectionInfos[0].name = ".text.kernel";
    sectionInfos[0].type = NEO::Elf::SHT_PROGBITS;
    auto retVal = encoder.checkIfAllFilesExist(sectionInfos);
    EXPECT_EQ(OCLOC_INVALID_FILE, retVal);
}

TEST_F(ZebinEncoderTests, WhenPrintHelpIsCalledThenHelpIsPrinted) {
    encoder.printHelp();
    EXPECT_FALSE(getOutput().empty());
}
