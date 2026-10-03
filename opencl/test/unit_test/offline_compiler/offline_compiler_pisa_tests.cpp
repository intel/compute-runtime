/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_compilers.h"

#include "opencl/test/unit_test/offline_compiler/environment.h"
#include "opencl/test/unit_test/offline_compiler/mock/mock_argument_helper.h"
#include "opencl/test/unit_test/offline_compiler/mock/mock_offline_compiler.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string_view>
#include <vector>

extern Environment *gEnvironment;

namespace NEO {

TEST(OclocPisa, GivenPisaInputArgumentWhenParsingCommandLineThenSetsInputAndIntermediateCodeTypeToPisa) {
    const std::vector<std::string> argv = {
        "ocloc",
        "compile",
        "-file",
        "copybuffer.pisa",
        "-pisa_input",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};

    const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, result);

    EXPECT_EQ(NEO::pisaCodeType, mockOfflineCompiler.inputCodeType);
    EXPECT_EQ(NEO::pisaCodeType, mockOfflineCompiler.intermediateRepresentation);
}

TEST(OclocPisa, GivenEmptyPisaInputWhenBuildingThenInvalidFileIsReturned) {
    constexpr uint8_t emptyPisaStorage = 0;
    Source emptyPisa{&emptyPisaStorage, 0, "empty.pisa"};
    const std::vector<std::string> argv = {
        "ocloc",
        "compile",
        "-file",
        emptyPisa.name,
        "-pisa_input",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->inputs.push_back(emptyPisa);

    ASSERT_EQ(OCLOC_SUCCESS, mockOfflineCompiler.initialize(argv.size(), argv));

    StreamCapture capture;
    capture.captureStdout();

    const auto buildResult = mockOfflineCompiler.build();

    std::string output = capture.getCapturedStdout();
    EXPECT_STREQ("Error: Input file empty.pisa is empty.\n", output.c_str());

    EXPECT_EQ(OCLOC_INVALID_FILE, buildResult);
}

TEST(OclocPisa, GivenHeaderOnlyPisaInputWhenBuildingThenSuccessIsReturned) {
    constexpr std::string_view headerOnlyPisa = ".version 1.0;\n.target 100c;\n";
    Source pisaInput{reinterpret_cast<const uint8_t *>(headerOnlyPisa.data()), headerOnlyPisa.size(), "header_only.pisa"};
    const std::vector<std::string> argv = {
        "ocloc",
        "compile",
        "-file",
        pisaInput.name,
        "-pisa_input",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->inputs.push_back(pisaInput);

    ASSERT_EQ(OCLOC_SUCCESS, mockOfflineCompiler.initialize(argv.size(), argv));

    MockCompilerDebugVars igcDebugVars{gEnvironment->igcDebugVars};
    igcDebugVars.forceSuccessWithEmptyOutput = true;
    setIgcDebugVars(igcDebugVars);

    StreamCapture capture;
    capture.captureStdout();

    const auto buildResult = mockOfflineCompiler.build();

    std::string output = capture.getCapturedStdout();
    EXPECT_STREQ("Compilation from IR - skipping loading of FCL\n", output.c_str());

    setIgcDebugVars(gEnvironment->igcDebugVars);

    EXPECT_EQ(OCLOC_SUCCESS, buildResult);
    EXPECT_EQ(headerOnlyPisa.size(), mockOfflineCompiler.irBinarySize);
    EXPECT_EQ(headerOnlyPisa, std::string_view(mockOfflineCompiler.irBinary, mockOfflineCompiler.irBinarySize));
}

TEST(OclocPisa, GivenPisaInputAndSpecConstFileWhenBuildingThenSpecializationConstantsArePassedToIgc) {
    constexpr std::string_view headerOnlyPisa = ".version 1.0;\n.target 100c;\n";
    Source pisaInput{reinterpret_cast<const uint8_t *>(headerOnlyPisa.data()), headerOnlyPisa.size(), "header_only.pisa"};
    const std::vector<std::string> argv = {
        "ocloc",
        "compile",
        "-file",
        pisaInput.name,
        "-pisa_input",
        "-device",
        gEnvironment->devicePrefix.c_str(),
        "-spec_const",
        "constants.txt"};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->inputs.push_back(pisaInput);
    mockOfflineCompiler.uniqueHelper->filesMap["constants.txt"] = "0: 42\n1: 100";

    StreamCapture capture;
    capture.captureStdout();

    ASSERT_EQ(OCLOC_SUCCESS, mockOfflineCompiler.initialize(argv.size(), argv));

    std::vector<uint32_t> receivedIds;
    std::vector<uint64_t> receivedValues;
    MockCompilerDebugVars igcDebugVars{gEnvironment->igcDebugVars};
    igcDebugVars.forceSuccessWithEmptyOutput = true;
    igcDebugVars.receivedSpecConstantIdsOutput = &receivedIds;
    igcDebugVars.receivedSpecConstantValuesOutput = &receivedValues;
    setIgcDebugVars(igcDebugVars);

    const auto buildResult = mockOfflineCompiler.build();

    capture.getCapturedStdout();
    setIgcDebugVars(gEnvironment->igcDebugVars);

    EXPECT_EQ(OCLOC_SUCCESS, buildResult);
    ASSERT_EQ(2u, receivedIds.size());
    ASSERT_EQ(2u, receivedValues.size());
    EXPECT_EQ(0u, receivedIds[0]);
    EXPECT_EQ(1u, receivedIds[1]);
    EXPECT_EQ(42u, receivedValues[0]);
    EXPECT_EQ(100u, receivedValues[1]);
}

TEST(OclocPisa, GivenEmptyNonPisaInputWhenBuildingThenInvalidFileIsReturned) {
    constexpr uint8_t emptyInputStorage = 0;
    Source emptyInput{&emptyInputStorage, 0, "empty.cl"};
    const std::vector<std::string> argv = {
        "ocloc",
        "compile",
        "-file",
        emptyInput.name,
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};
    mockOfflineCompiler.uniqueHelper->inputs.push_back(emptyInput);

    ASSERT_EQ(OCLOC_SUCCESS, mockOfflineCompiler.initialize(argv.size(), argv));

    StreamCapture capture;
    capture.captureStdout();

    const auto buildResult = mockOfflineCompiler.build();

    std::string output = capture.getCapturedStdout();
    EXPECT_STREQ("Error: Input file empty.cl is empty.\n", output.c_str());

    EXPECT_EQ(OCLOC_INVALID_FILE, buildResult);
}

TEST(OclocPisa, GivenEmitPisaArgumentWhenParsingCommandLineThenSetsIntermediateRepresentationAndOnlyIr) {
    const std::vector<std::string> argv = {
        "ocloc",
        "compile",
        "-file",
        "copybuffer.cl",
        "-emit_pisa",
        "-device",
        gEnvironment->devicePrefix.c_str()};

    MockOfflineCompiler mockOfflineCompiler{};

    const auto result = mockOfflineCompiler.parseCommandLine(argv.size(), argv);
    EXPECT_EQ(OCLOC_SUCCESS, result);
    EXPECT_EQ(NEO::pisaCodeType, mockOfflineCompiler.intermediateRepresentation);
    EXPECT_TRUE(mockOfflineCompiler.onlyIr);
}

} // namespace NEO
