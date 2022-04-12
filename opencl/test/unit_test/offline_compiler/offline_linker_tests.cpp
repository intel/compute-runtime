/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "offline_linker_tests.h"

#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/test/common/mocks/mock_compilers.h"

#include "environment.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

extern Environment *gEnvironment;

namespace NEO {

using OperationMode = MockOfflineLinker::OperationMode;

void OfflineLinkerTest::SetUp() {
    MockCompilerDebugVars igcDebugVars{gEnvironment->igcDebugVars};
    igcDebugVars.binaryToReturn = binaryToReturn;
    igcDebugVars.binaryToReturnSize = sizeof(binaryToReturn);

    setIgcDebugVars(igcDebugVars);
}

void OfflineLinkerTest::TearDown() {
    setIgcDebugVars(gEnvironment->igcDebugVars);
}

std::string OfflineLinkerTest::getEmptySpirvFile() const {
    std::string spirv{"\x07\x23\x02\x03"};
    spirv.resize(64, '\0');

    return spirv;
}

std::string OfflineLinkerTest::getEmptyLlvmBcFile() const {
    std::string llvmbc{"BC\xc0\xde"};
    llvmbc.resize(64, '\0');

    return llvmbc;
}

MockOfflineLinker::InputFileContent OfflineLinkerTest::createFileContent(const std::string &content, IGC::CodeType::CodeType_t codeType) const {
    std::unique_ptr<char[]> bytes{new char[content.size()]};
    std::copy(content.begin(), content.end(), bytes.get());

    return {std::move(bytes), content.size(), codeType};
}

TEST_F(OfflineLinkerTest, GivenDefaultConstructedLinkerThenRequiredFieldsHaveDefaultValues) {
    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};

    EXPECT_EQ(OperationMode::SKIP_EXECUTION, mockOfflineLinker.operationMode);
    EXPECT_EQ("linker_output", mockOfflineLinker.outputFilename);
    EXPECT_EQ(IGC::CodeType::llvmBc, mockOfflineLinker.outputFormat);
}

TEST_F(OfflineLinkerTest, GivenLessThanTwoArgumentsWhenParsingThenInvalidCommandIsReturned) {
    const std::vector<std::string> argv = {
        "ocloc.exe"};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    const auto result = mockOfflineLinker.parseCommand(argv.size(), argv);
    EXPECT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, result);
}

TEST_F(OfflineLinkerTest, GivenInputFilesArgumentsWhenParsingThenListOfFilenamesIsPopulated) {
    const std::string firstFile{"sample_input_1.spv"};
    const std::string secondFile{"sample_input_2.spv"};

    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "-file",
        firstFile,
        "-file",
        secondFile};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    const auto result = mockOfflineLinker.parseCommand(argv.size(), argv);

    ASSERT_EQ(OclocErrorCode::SUCCESS, result);
    ASSERT_EQ(2u, mockOfflineLinker.inputFilenames.size());

    EXPECT_EQ(firstFile, mockOfflineLinker.inputFilenames[0]);
    EXPECT_EQ(secondFile, mockOfflineLinker.inputFilenames[1]);
}

TEST_F(OfflineLinkerTest, GivenOutputFilenameArgumentWhenParsingThenOutputFilenameIsSetAccordingly) {
    const std::string outputFilename{"my_custom_output_filename"};

    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "-out",
        outputFilename};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    const auto result = mockOfflineLinker.parseCommand(argv.size(), argv);

    ASSERT_EQ(OclocErrorCode::SUCCESS, result);
    EXPECT_EQ(outputFilename, mockOfflineLinker.outputFilename);
}

TEST_F(OfflineLinkerTest, GivenValidOutputFileFormatWhenParsingThenOutputFormatIsSetAccordingly) {
    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "-out_format",
        "LLVM_BC"};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    const auto result = mockOfflineLinker.parseCommand(argv.size(), argv);

    ASSERT_EQ(OclocErrorCode::SUCCESS, result);
    EXPECT_EQ(IGC::CodeType::llvmBc, mockOfflineLinker.outputFormat);
}

TEST_F(OfflineLinkerTest, GivenUnknownOutputFileFormatWhenParsingThenInvalidFormatIsSet) {
    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "-out_format",
        "StrangeFormat"};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    const auto result = mockOfflineLinker.parseCommand(argv.size(), argv);

    ASSERT_EQ(OclocErrorCode::SUCCESS, result);
    EXPECT_EQ(IGC::CodeType::invalid, mockOfflineLinker.outputFormat);
}

TEST_F(OfflineLinkerTest, GivenOptionsArgumentWhenParsingThenOptionsAreSet) {
    const std::string options{"-g"};

    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "-options",
        options};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    const auto result = mockOfflineLinker.parseCommand(argv.size(), argv);

    ASSERT_EQ(OclocErrorCode::SUCCESS, result);
    EXPECT_EQ(options, mockOfflineLinker.options);
}

TEST_F(OfflineLinkerTest, GivenInternalOptionsArgumentWhenParsingThenInternalOptionsAreSet) {
    const std::string internalOptions{"-ze-allow-zebin"};

    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "-internal_options",
        internalOptions};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    const auto result = mockOfflineLinker.parseCommand(argv.size(), argv);

    ASSERT_EQ(OclocErrorCode::SUCCESS, result);
    EXPECT_EQ(internalOptions, mockOfflineLinker.internalOptions);
}

TEST_F(OfflineLinkerTest, GivenHelpArgumentWhenParsingThenShowHelpOperationIsSet) {
    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "--help"};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    const auto result = mockOfflineLinker.parseCommand(argv.size(), argv);

    ASSERT_EQ(OclocErrorCode::SUCCESS, result);
    EXPECT_EQ(OperationMode::SHOW_HELP, mockOfflineLinker.operationMode);
}

TEST_F(OfflineLinkerTest, GivenUnknownArgumentWhenParsingThenErrorIsReported) {
    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "-some_new_unknown_command"};

    ::testing::internal::CaptureStdout();
    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    const auto result = mockOfflineLinker.initialize(argv.size(), argv);
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, result);

    const std::string expectedErrorMessage{"Invalid option (arg 2): -some_new_unknown_command\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OfflineLinkerTest, GivenFlagsWhichRequireMoreArgsWithoutThemWhenParsingThenErrorIsReported) {
    const std::array<std::string, 5> flagsToTest = {
        "-file", "-out", "-out_format", "-options", "-internal_options"};

    for (const auto &flag : flagsToTest) {
        const std::vector<std::string> argv = {
            "ocloc.exe",
            "link",
            flag};

        mockOclocIgcFacade = std::make_unique<MockOclocIgcFacade>(&mockArgHelper);
        MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};

        ::testing::internal::CaptureStdout();
        const auto result = mockOfflineLinker.parseCommand(argv.size(), argv);
        const auto output{::testing::internal::GetCapturedStdout()};

        EXPECT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, result);

        const std::string expectedErrorMessage{"Invalid option (arg 2): " + flag + "\n"};
        EXPECT_EQ(expectedErrorMessage, output);
    }
}

TEST_F(OfflineLinkerTest, GivenCommandWithoutInputFilesWhenVerificationIsPerformedThenErrorIsReturned) {
    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    mockOfflineLinker.inputFilenames = {};

    ::testing::internal::CaptureStdout();
    const auto verificationResult = mockOfflineLinker.verifyLinkerCommand();
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, verificationResult);

    const std::string expectedErrorMessage{"Error: Input name is missing! At least one input file is required!\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OfflineLinkerTest, GivenCommandWithEmptyFilenameWhenVerificationIsPerformedThenErrorIsReturned) {
    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "-file",
        ""};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};

    ::testing::internal::CaptureStdout();
    const auto verificationResult = mockOfflineLinker.initialize(argv.size(), argv);
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, verificationResult);

    const std::string expectedErrorMessage{"Error: Empty filename cannot be used!\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OfflineLinkerTest, GivenCommandWithNonexistentInputFileWhenVerificationIsPerformedThenErrorIsReturned) {
    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "-file",
        "some_file1.spv",
        "-file",
        "some_file2.spv"};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    const auto parsingResult = mockOfflineLinker.parseCommand(argv.size(), argv);
    ASSERT_EQ(OclocErrorCode::SUCCESS, parsingResult);

    ::testing::internal::CaptureStdout();
    const auto verificationResult = mockOfflineLinker.verifyLinkerCommand();
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::INVALID_FILE, verificationResult);

    const std::string expectedErrorMessage{"Error: Input file some_file1.spv missing.\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OfflineLinkerTest, GivenCommandWithInvalidOutputFormatWhenVerificationIsPerformedThenErrorIsReturned) {
    mockArgHelperFilesMap["some_file.spv"] = getEmptySpirvFile();

    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "-file",
        "some_file.spv",
        "-out_format",
        "SomeDummyUnknownFormat"};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    const auto parsingResult = mockOfflineLinker.parseCommand(argv.size(), argv);
    ASSERT_EQ(OclocErrorCode::SUCCESS, parsingResult);

    ::testing::internal::CaptureStdout();
    const auto verificationResult = mockOfflineLinker.verifyLinkerCommand();
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, verificationResult);

    const std::string expectedErrorMessage{"Error: Invalid output type!\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OfflineLinkerTest, GivenValidCommandWhenVerificationIsPerformedThenSuccessIsReturned) {
    mockArgHelperFilesMap["some_file1.spv"] = getEmptySpirvFile();
    mockArgHelperFilesMap["some_file2.spv"] = getEmptySpirvFile();

    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "-file",
        "some_file1.spv",
        "-file",
        "some_file2.spv"};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    const auto parsingResult = mockOfflineLinker.parseCommand(argv.size(), argv);
    ASSERT_EQ(OclocErrorCode::SUCCESS, parsingResult);

    ::testing::internal::CaptureStdout();
    const auto verificationResult = mockOfflineLinker.verifyLinkerCommand();
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::SUCCESS, verificationResult);
    EXPECT_TRUE(output.empty());
}

TEST_F(OfflineLinkerTest, GivenEmptyFileWhenLoadingInputFilesThenErrorIsReturned) {
    const std::string filename{"some_file.spv"};
    mockArgHelperFilesMap[filename] = "";
    mockArgHelper.shouldLoadDataFromFileReturnZeroSize = true;

    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "-file",
        filename};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};

    ::testing::internal::CaptureStdout();
    const auto initializationResult = mockOfflineLinker.initialize(argv.size(), argv);
    const auto output{::testing::internal::GetCapturedStdout()};

    ASSERT_EQ(OclocErrorCode::INVALID_FILE, initializationResult);

    const std::string expectedErrorMessage{"Error: Cannot read input file: some_file.spv\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OfflineLinkerTest, GivenValidFileWithUnknownFormatWhenLoadingInputFilesThenErrorIsReturned) {
    const std::string filename{"some_file.unknown"};

    // Spir-V or LLVM-BC magic constants are required. This should be treated as error.
    mockArgHelperFilesMap[filename] = "Some unknown format!";

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    mockOfflineLinker.inputFilenames.push_back(filename);

    ::testing::internal::CaptureStdout();
    const auto readingResult = mockOfflineLinker.loadInputFilesContent();
    const auto output{::testing::internal::GetCapturedStdout()};

    ASSERT_EQ(OclocErrorCode::INVALID_PROGRAM, readingResult);

    const std::string expectedErrorMessage{"Error: Unsupported format of input file: some_file.unknown\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OfflineLinkerTest, GivenValidFilesWithValidFormatsWhenLoadingInputFilesThenFilesAreLoadedAndSuccessIsReturned) {
    const std::string firstFilename{"some_file1.spv"};
    const std::string secondFilename{"some_file2.llvmbc"};

    mockArgHelperFilesMap[firstFilename] = getEmptySpirvFile();
    mockArgHelperFilesMap[secondFilename] = getEmptyLlvmBcFile();

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    mockOfflineLinker.inputFilenames.push_back(firstFilename);
    mockOfflineLinker.inputFilenames.push_back(secondFilename);

    ::testing::internal::CaptureStdout();
    const auto readingResult = mockOfflineLinker.loadInputFilesContent();
    const auto output{::testing::internal::GetCapturedStdout()};

    ASSERT_EQ(OclocErrorCode::SUCCESS, readingResult);
    EXPECT_TRUE(output.empty());

    const auto &firstExpectedContent = mockArgHelperFilesMap[firstFilename];
    const auto &firstActualContent = mockOfflineLinker.inputFilesContent[0];

    ASSERT_EQ(firstExpectedContent.size() + 1, firstActualContent.size);
    const auto isFirstPairEqual = std::equal(firstExpectedContent.begin(), firstExpectedContent.end(), firstActualContent.bytes.get());
    EXPECT_TRUE(isFirstPairEqual);

    const auto &secondExpectedContent = mockArgHelperFilesMap[secondFilename];
    const auto &secondActualContent = mockOfflineLinker.inputFilesContent[1];

    ASSERT_EQ(secondExpectedContent.size() + 1, secondActualContent.size);
    const auto isSecondPairEqual = std::equal(secondExpectedContent.begin(), secondExpectedContent.end(), secondActualContent.bytes.get());
    EXPECT_TRUE(isSecondPairEqual);
}

TEST_F(OfflineLinkerTest, GivenValidFilesWhenInitializationIsSuccessfulThenLinkModeOfOperationIsSet) {
    const std::string firstFilename{"some_file1.spv"};
    const std::string secondFilename{"some_file2.llvmbc"};

    mockArgHelperFilesMap[firstFilename] = getEmptySpirvFile();
    mockArgHelperFilesMap[secondFilename] = getEmptyLlvmBcFile();

    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "-file",
        firstFilename,
        "-file",
        secondFilename};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};

    ::testing::internal::CaptureStdout();
    const auto initializationResult = mockOfflineLinker.initialize(argv.size(), argv);
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::SUCCESS, initializationResult);
    EXPECT_TRUE(output.empty());

    EXPECT_EQ(OperationMode::LINK_FILES, mockOfflineLinker.operationMode);
}

TEST_F(OfflineLinkerTest, GivenSPIRVandLLVMBCFilesWhenElfOutputIsRequestedThenElfWithSPIRVAndLLVMSectionsIsCreated) {
    auto spirvFileContent = createFileContent(getEmptySpirvFile(), IGC::CodeType::spirV);
    auto llvmbcFileContent = createFileContent(getEmptyLlvmBcFile(), IGC::CodeType::llvmBc);

    mockArgHelper.interceptOutput = true;

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    mockOfflineLinker.inputFilesContent.emplace_back(std::move(spirvFileContent.bytes), spirvFileContent.size, spirvFileContent.codeType);
    mockOfflineLinker.inputFilesContent.emplace_back(std::move(llvmbcFileContent.bytes), llvmbcFileContent.size, llvmbcFileContent.codeType);
    mockOfflineLinker.outputFormat = IGC::CodeType::elf;
    mockOfflineLinker.operationMode = OperationMode::LINK_FILES;

    const auto linkingResult{mockOfflineLinker.execute()};
    ASSERT_EQ(OclocErrorCode::SUCCESS, linkingResult);

    ASSERT_EQ(1u, mockArgHelper.interceptedFiles.count("linker_output"));
    const auto &rawOutput{mockArgHelper.interceptedFiles.at("linker_output")};
    const auto encodedElf{ArrayRef<const uint8_t>::fromAny(rawOutput.data(), rawOutput.size())};

    std::string errorReason{};
    std::string warning{};
    const auto elf{Elf::decodeElf(encodedElf, errorReason, warning)};

    ASSERT_TRUE(errorReason.empty());
    EXPECT_TRUE(warning.empty());

    // SPIR-V bitcode section.
    EXPECT_EQ(Elf::SHT_OPENCL_SPIRV, elf.sectionHeaders[1].header->type);

    const auto &expectedFirstSection = mockOfflineLinker.inputFilesContent[0];
    const auto &actualFirstSection = elf.sectionHeaders[1];
    ASSERT_EQ(expectedFirstSection.size, actualFirstSection.header->size);

    const auto isFirstSectionContentEqual = std::memcmp(actualFirstSection.data.begin(), expectedFirstSection.bytes.get(), expectedFirstSection.size) == 0;
    EXPECT_TRUE(isFirstSectionContentEqual);

    // LLVM bitcode section.
    EXPECT_EQ(Elf::SHT_OPENCL_LLVM_BINARY, elf.sectionHeaders[2].header->type);

    const auto &expectedSecondSection = mockOfflineLinker.inputFilesContent[1];
    const auto &actualSecondSection = elf.sectionHeaders[2];
    ASSERT_EQ(expectedSecondSection.size, actualSecondSection.header->size);

    const auto isSecondSectionContentEqual = std::memcmp(actualSecondSection.data.begin(), expectedSecondSection.bytes.get(), expectedSecondSection.size) == 0;
    EXPECT_TRUE(isSecondSectionContentEqual);
}

TEST_F(OfflineLinkerTest, GivenValidInputFileContentsWhenLlvmBcOutputIsRequestedThenSuccessIsReturnedAndFileIsWritten) {
    auto spirvFileContent = createFileContent(getEmptySpirvFile(), IGC::CodeType::spirV);
    auto llvmbcFileContent = createFileContent(getEmptyLlvmBcFile(), IGC::CodeType::llvmBc);

    mockArgHelper.interceptOutput = true;

    HardwareInfo hwInfo{};
    const auto igcInitializationResult{mockOclocIgcFacade->initialize(hwInfo)};
    ASSERT_EQ(OclocErrorCode::SUCCESS, igcInitializationResult);

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    mockOfflineLinker.inputFilesContent.emplace_back(std::move(spirvFileContent.bytes), spirvFileContent.size, spirvFileContent.codeType);
    mockOfflineLinker.inputFilesContent.emplace_back(std::move(llvmbcFileContent.bytes), llvmbcFileContent.size, llvmbcFileContent.codeType);
    mockOfflineLinker.outputFormat = IGC::CodeType::llvmBc;
    mockOfflineLinker.operationMode = OperationMode::LINK_FILES;

    const auto linkingResult{mockOfflineLinker.execute()};
    ASSERT_EQ(OclocErrorCode::SUCCESS, linkingResult);

    ASSERT_EQ(1u, mockArgHelper.interceptedFiles.count("linker_output"));
    const auto &actualOutput{mockArgHelper.interceptedFiles.at("linker_output")};
    const auto &expectedOutput{binaryToReturn};

    ASSERT_EQ(sizeof(expectedOutput), actualOutput.size());

    const auto isActualOutputSameAsExpected{std::equal(std::begin(expectedOutput), std::end(expectedOutput), std::begin(actualOutput))};
    EXPECT_TRUE(isActualOutputSameAsExpected);
}

TEST_F(OfflineLinkerTest, GivenValidInputFileContentsAndFailingIGCWhenLlvmBcOutputIsRequestedThenErrorIsReturned) {
    MockCompilerDebugVars igcDebugVars{gEnvironment->igcDebugVars};
    igcDebugVars.forceBuildFailure = true;
    setIgcDebugVars(igcDebugVars);

    auto spirvFileContent = createFileContent(getEmptySpirvFile(), IGC::CodeType::spirV);
    auto llvmbcFileContent = createFileContent(getEmptyLlvmBcFile(), IGC::CodeType::llvmBc);

    mockArgHelper.interceptOutput = true;

    HardwareInfo hwInfo{};
    const auto igcInitializationResult{mockOclocIgcFacade->initialize(hwInfo)};
    ASSERT_EQ(OclocErrorCode::SUCCESS, igcInitializationResult);

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    mockOfflineLinker.inputFilesContent.emplace_back(std::move(spirvFileContent.bytes), spirvFileContent.size, spirvFileContent.codeType);
    mockOfflineLinker.inputFilesContent.emplace_back(std::move(llvmbcFileContent.bytes), llvmbcFileContent.size, llvmbcFileContent.codeType);
    mockOfflineLinker.outputFormat = IGC::CodeType::llvmBc;
    mockOfflineLinker.operationMode = OperationMode::LINK_FILES;

    ::testing::internal::CaptureStdout();
    const auto linkingResult{mockOfflineLinker.execute()};
    const auto output{::testing::internal::GetCapturedStdout()};

    ASSERT_EQ(OclocErrorCode::BUILD_PROGRAM_FAILURE, linkingResult);
    EXPECT_EQ(0u, mockArgHelper.interceptedFiles.count("linker_output"));

    const std::string expectedErrorMessage{"Error: Translation has failed! IGC returned empty output.\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OfflineLinkerTest, GivenValidInputFileContentsAndInvalidTranslationOutputWhenLlvmBcOutputIsRequestedThenErrorIsReturned) {
    MockCompilerDebugVars igcDebugVars{gEnvironment->igcDebugVars};
    igcDebugVars.shouldReturnInvalidTranslationOutput = true;
    setIgcDebugVars(igcDebugVars);

    auto spirvFileContent = createFileContent(getEmptySpirvFile(), IGC::CodeType::spirV);
    auto llvmbcFileContent = createFileContent(getEmptyLlvmBcFile(), IGC::CodeType::llvmBc);

    mockArgHelper.interceptOutput = true;

    HardwareInfo hwInfo{};
    const auto igcInitializationResult{mockOclocIgcFacade->initialize(hwInfo)};
    ASSERT_EQ(OclocErrorCode::SUCCESS, igcInitializationResult);

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    mockOfflineLinker.inputFilesContent.emplace_back(std::move(spirvFileContent.bytes), spirvFileContent.size, spirvFileContent.codeType);
    mockOfflineLinker.inputFilesContent.emplace_back(std::move(llvmbcFileContent.bytes), llvmbcFileContent.size, llvmbcFileContent.codeType);
    mockOfflineLinker.outputFormat = IGC::CodeType::llvmBc;
    mockOfflineLinker.operationMode = OperationMode::LINK_FILES;

    ::testing::internal::CaptureStdout();
    const auto linkingResult{mockOfflineLinker.execute()};
    const auto output{::testing::internal::GetCapturedStdout()};

    ASSERT_EQ(OclocErrorCode::OUT_OF_HOST_MEMORY, linkingResult);
    EXPECT_EQ(0u, mockArgHelper.interceptedFiles.count("linker_output"));

    const std::string expectedErrorMessage{"Error: Translation has failed! IGC output is nullptr!\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OfflineLinkerTest, GivenUninitializedLinkerWhenExecuteIsInvokedThenErrorIsIssued) {
    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};

    ::testing::internal::CaptureStdout();
    const auto executionResult{mockOfflineLinker.execute()};
    const auto output{::testing::internal::GetCapturedStdout()};

    ASSERT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, executionResult);
    ASSERT_FALSE(output.empty());
}

TEST_F(OfflineLinkerTest, GivenHelpRequestWhenExecuteIsInvokedThenHelpIsPrinted) {
    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    mockOfflineLinker.operationMode = OperationMode::SHOW_HELP;

    ::testing::internal::CaptureStdout();
    const auto executionResult{mockOfflineLinker.execute()};
    const auto output{::testing::internal::GetCapturedStdout()};

    ASSERT_EQ(OclocErrorCode::SUCCESS, executionResult);
    ASSERT_FALSE(output.empty());
}

TEST_F(OfflineLinkerTest, GivenInvalidOperationModeWhenExecuteIsInvokedThenErrorIsIssued) {
    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    mockOfflineLinker.operationMode = static_cast<OperationMode>(7);

    ::testing::internal::CaptureStdout();
    const auto executionResult{mockOfflineLinker.execute()};
    const auto output{::testing::internal::GetCapturedStdout()};

    ASSERT_EQ(OclocErrorCode::INVALID_COMMAND_LINE, executionResult);
    ASSERT_FALSE(output.empty());
}

TEST_F(OfflineLinkerTest, GivenUninitializedHwInfoWhenInitIsCalledThenHwInfoIsInitialized) {
    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    ASSERT_EQ(IGFX_UNKNOWN, mockOfflineLinker.hwInfo.platform.eProductFamily);

    const auto hwInfoInitializationResult{mockOfflineLinker.initHardwareInfo()};
    ASSERT_EQ(OclocErrorCode::SUCCESS, hwInfoInitializationResult);
    EXPECT_NE(IGFX_UNKNOWN, mockOfflineLinker.hwInfo.platform.eProductFamily);
}

TEST_F(OfflineLinkerTest, GivenEmptyHwInfoTableWhenInitializationIsPerformedThenItFailsOnHwInit) {
    const std::string firstFilename{"some_file1.spv"};
    const std::string secondFilename{"some_file2.llvmbc"};

    mockArgHelperFilesMap[firstFilename] = getEmptySpirvFile();
    mockArgHelperFilesMap[secondFilename] = getEmptyLlvmBcFile();

    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "-file",
        firstFilename,
        "-file",
        secondFilename};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    mockOfflineLinker.shouldReturnEmptyHardwareInfoTable = true;

    ::testing::internal::CaptureStdout();
    const auto initializationResult = mockOfflineLinker.initialize(argv.size(), argv);
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::INVALID_DEVICE, initializationResult);

    const std::string expectedErrorMessage{"Error! Cannot retrieve any valid hardware information!\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OfflineLinkerTest, GivenMissingIgcLibraryWhenInitializationIsPerformedThenItFailsOnIgcPreparation) {
    const std::string firstFilename{"some_file1.spv"};
    const std::string secondFilename{"some_file2.llvmbc"};

    mockArgHelperFilesMap[firstFilename] = getEmptySpirvFile();
    mockArgHelperFilesMap[secondFilename] = getEmptyLlvmBcFile();

    const std::vector<std::string> argv = {
        "ocloc.exe",
        "link",
        "-file",
        firstFilename,
        "-file",
        secondFilename};

    mockOclocIgcFacade->shouldFailLoadingOfIgcLib = true;
    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};

    ::testing::internal::CaptureStdout();
    const auto initializationResult = mockOfflineLinker.initialize(argv.size(), argv);
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::OUT_OF_HOST_MEMORY, initializationResult);

    std::stringstream expectedErrorMessage;
    expectedErrorMessage << "Error! Loading of IGC library has failed! Filename: " << Os::igcDllName << "\n";

    EXPECT_EQ(expectedErrorMessage.str(), output);
}

TEST_F(OfflineLinkerTest, GivenOfflineLinkerWhenStoringValidBuildLogThenItIsSaved) {
    const std::string someValidLog{"Warning: This is a build log!"};

    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    mockOfflineLinker.tryToStoreBuildLog(someValidLog.data(), someValidLog.size());

    EXPECT_EQ(someValidLog, mockOfflineLinker.getBuildLog());
}

TEST_F(OfflineLinkerTest, GivenOfflineLinkerWhenStoringInvalidBuildLogThenItIsIgnored) {
    MockOfflineLinker mockOfflineLinker{&mockArgHelper, std::move(mockOclocIgcFacade)};
    mockOfflineLinker.tryToStoreBuildLog(nullptr, 0);

    const auto buildLog{mockOfflineLinker.getBuildLog()};
    EXPECT_TRUE(buildLog.empty());

    // Invalid size has been passed.
    const char *log{"Info: This is a log!"};
    mockOfflineLinker.tryToStoreBuildLog(log, 0);

    const auto buildLog2{mockOfflineLinker.getBuildLog()};
    EXPECT_TRUE(buildLog2.empty());
}

} // namespace NEO