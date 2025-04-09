/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "iga_wrapper_tests.h"

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_os_library.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_iga_dll_guard.h"
#include "opencl/test/unit_test/offline_compiler/stdout_capturer.h"

#include <array>

namespace NEO {

TEST_F(IgaWrapperTest, GivenInvalidPathToIgaLibraryWhenDisassemblingGenIsaThenFalseIsReturnedAndErrorMessageIsPrinted) {
    MockOsLibrary::loadLibraryNewObject = nullptr;
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};

    MockIgaDllGuard mockIgaDllGuard{"some_invalid_path_to_library"};
    mockIgaDllGuard.enable();

    const std::string kernel{"INPUT TO DISASM"};
    std::string out;

    StdoutCapturer capturer{};
    const auto result = testedIgaWrapper.tryDisassembleGenISA(kernel.c_str(), static_cast<uint32_t>(kernel.size()), out);
    const auto output{capturer.acquireOutput()};

    EXPECT_FALSE(result);
    EXPECT_EQ("Warning: couldn't load iga - kernel binaries won't be disassembled.\n", output);

    EXPECT_EQ(0, igaDllMock->contextCreateCalledCount);
}

TEST_F(IgaWrapperTest, GivenContextCreationFailureWhenDisassemblingGenIsaThenFalseIsReturnedAndErrorMessageIsPrinted) {
    igaDllMock->contextCreateReturnValue = igaStatusFailure;
    igaDllMock->statusToStringReturnValue = "SOME_CONTEXT_ERROR";

    const std::string kernel{"INPUT TO DISASM"};
    std::string out;

    StdoutCapturer capturer{};
    const auto result = testedIgaWrapper.tryDisassembleGenISA(kernel.c_str(), static_cast<uint32_t>(kernel.size()), out);
    const auto output{capturer.acquireOutput()};

    EXPECT_FALSE(result);
    EXPECT_EQ("Error while creating IGA Context! Error msg: SOME_CONTEXT_ERROR", output);

    EXPECT_EQ(1, igaDllMock->contextCreateCalledCount);
    EXPECT_EQ(1, igaDllMock->statusToStringCalledCount);
}

TEST_F(IgaWrapperTest, GivenContextCreationSuccessAndDisassemblationFailureAndNoContextErrorsWhenDisassemblingGenIsaThenFalseIsReturnedAndErrorMessageIsPrinted) {
    const std::array mockSetupFlags = {
        &IgaDllMock::shouldContextGetErrorsReturnNullptrAndNonZeroSize,
        &IgaDllMock::shouldContextGetErrorsReturnZeroSizeAndNonNullptr};

    for (const auto &mockSetupFlag : mockSetupFlags) {
        igaDllMock = nullptr;
        igaDllMock = std::make_unique<IgaDllMock>();

        igaDllMock->contextCreateReturnValue = igaStatusSuccess;
        igaDllMock->disassembleReturnValue = igaStatusFailure;
        igaDllMock->statusToStringReturnValue = "SOME_DISASSEMBLE_ERROR";
        igaDllMock->contextGetErrorsReturnValue = igaStatusFailure;
        igaDllMock->contextReleaseReturnValue = igaStatusSuccess;

        *igaDllMock.*mockSetupFlag = true;

        const std::string kernel{"INPUT TO DISASM"};
        std::string out;

        StdoutCapturer capturer{};
        const auto result = testedIgaWrapper.tryDisassembleGenISA(kernel.c_str(), static_cast<uint32_t>(kernel.size()), out);
        const auto output{capturer.acquireOutput()};

        EXPECT_FALSE(result);
        EXPECT_EQ("Error while disassembling with IGA!\nStatus msg: SOME_DISASSEMBLE_ERROR\n", output);

        EXPECT_EQ(1, igaDllMock->contextCreateCalledCount);
        EXPECT_EQ(1, igaDllMock->disassembleCalledCount);
        EXPECT_EQ(1, igaDllMock->statusToStringCalledCount);
        EXPECT_EQ(1, igaDllMock->contextGetErrorsCalledCount);
        EXPECT_EQ(1, igaDllMock->contextReleaseCalledCount);
    }
}

TEST_F(IgaWrapperTest, GivenContextCreationSuccessAndDisassemblationFailureAndContextErrorsWhenDisassemblingGenIsaThenFalseIsReturnedAndErrorMessageIsPrinted) {
    igaDllMock->contextCreateReturnValue = igaStatusSuccess;
    igaDllMock->disassembleReturnValue = igaStatusFailure;
    igaDllMock->statusToStringReturnValue = "SOME_DISASSEMBLE_ERROR";
    igaDllMock->contextGetErrorsReturnValue = igaStatusSuccess;
    igaDllMock->contextGetErrorsOutputString = "SPECIFIC_ERROR";
    igaDllMock->contextReleaseReturnValue = igaStatusSuccess;

    const std::string kernel{"INPUT TO DISASM"};
    std::string out;

    StdoutCapturer capturer{};
    const auto result = testedIgaWrapper.tryDisassembleGenISA(kernel.c_str(), static_cast<uint32_t>(kernel.size()), out);
    const auto output{capturer.acquireOutput()};

    EXPECT_FALSE(result);
    EXPECT_EQ("Error while disassembling with IGA!\nStatus msg: SOME_DISASSEMBLE_ERROR\n"
              "Errors: SPECIFIC_ERROR\n",
              output);

    EXPECT_EQ(1, igaDllMock->contextCreateCalledCount);
    EXPECT_EQ(1, igaDllMock->disassembleCalledCount);
    EXPECT_EQ(1, igaDllMock->statusToStringCalledCount);
    EXPECT_EQ(1, igaDllMock->contextGetErrorsCalledCount);
    EXPECT_EQ(1, igaDllMock->contextReleaseCalledCount);
}

TEST_F(IgaWrapperTest, GivenContextCreationSuccessAndDisassemblationSuccessAndNoWarningsWhenDisassemblingGenIsaThenTrueIsReturnedAndNoMessageIsPrinted) {
    const std::array mockSetupFlags = {
        &IgaDllMock::shouldContextGetWarningsReturnNullptrAndNonZeroSize,
        &IgaDllMock::shouldContextGetWarningsReturnZeroSizeAndNonNullptr};

    for (const auto &mockSetupFlag : mockSetupFlags) {
        igaDllMock = nullptr;
        igaDllMock = std::make_unique<IgaDllMock>();

        igaDllMock->contextCreateReturnValue = igaStatusSuccess;
        igaDllMock->disassembleReturnValue = igaStatusSuccess;
        igaDllMock->contextGetWarningsReturnValue = igaStatusSuccess;
        igaDllMock->contextReleaseReturnValue = igaStatusSuccess;
        igaDllMock->disassembleOutputKernelText = "Some kernel text!";

        *igaDllMock.*mockSetupFlag = true;

        const std::string kernel{"INPUT TO DISASM"};
        std::string out;

        StdoutCapturer capturer{};
        const auto result = testedIgaWrapper.tryDisassembleGenISA(kernel.c_str(), static_cast<uint32_t>(kernel.size()), out);
        const auto output{capturer.acquireOutput()};

        EXPECT_TRUE(result);
        EXPECT_TRUE(output.empty()) << output;

        EXPECT_EQ(1, igaDllMock->contextCreateCalledCount);
        EXPECT_EQ(1, igaDllMock->disassembleCalledCount);
        EXPECT_EQ(0, igaDllMock->statusToStringCalledCount);
        EXPECT_EQ(0, igaDllMock->contextGetErrorsCalledCount);
        EXPECT_EQ(1, igaDllMock->contextGetWarningsCalledCount);
        EXPECT_EQ(1, igaDllMock->contextReleaseCalledCount);

        EXPECT_EQ("Some kernel text!", out);
    }
}

TEST_F(IgaWrapperTest, GivenContextCreationSuccessAndDisassemblationSuccessAndWarningsWhenDisassemblingGenIsaThenTrueIsReturnedAndWarningIsPrinted) {
    igaDllMock->contextCreateReturnValue = igaStatusSuccess;
    igaDllMock->disassembleReturnValue = igaStatusSuccess;
    igaDllMock->contextGetWarningsReturnValue = igaStatusSuccess;
    igaDllMock->contextGetWarningsOutputString = "SOME_WARNING_MESSAGE";
    igaDllMock->contextReleaseReturnValue = igaStatusSuccess;
    igaDllMock->disassembleOutputKernelText = "Some kernel text!";

    const std::string kernel{"INPUT TO DISASM"};
    std::string out;

    StdoutCapturer capturer{};
    const auto result = testedIgaWrapper.tryDisassembleGenISA(kernel.c_str(), static_cast<uint32_t>(kernel.size()), out);
    const auto output{capturer.acquireOutput()};

    EXPECT_TRUE(result);
    EXPECT_EQ("Warnings: SOME_WARNING_MESSAGE\n", output);

    EXPECT_EQ(1, igaDllMock->contextCreateCalledCount);
    EXPECT_EQ(1, igaDllMock->disassembleCalledCount);
    EXPECT_EQ(0, igaDllMock->statusToStringCalledCount);
    EXPECT_EQ(0, igaDllMock->contextGetErrorsCalledCount);
    EXPECT_EQ(1, igaDllMock->contextGetWarningsCalledCount);
    EXPECT_EQ(1, igaDllMock->contextReleaseCalledCount);

    EXPECT_EQ("Some kernel text!", out);
}

TEST_F(IgaWrapperTest, GivenInvalidPathToIgaLibraryWhenAssemblingGenIsaThenFalseIsReturnedAndErrorMessageIsPrinted) {
    MockOsLibrary::loadLibraryNewObject = nullptr;
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};

    MockIgaDllGuard mockIgaDllGuard{"some_invalid_path_to_library"};
    mockIgaDllGuard.enable();

    const std::string inAsm{"Input ASM"};
    std::string out;

    StdoutCapturer capturer{};
    const auto result = testedIgaWrapper.tryAssembleGenISA(inAsm, out);
    const auto output{capturer.acquireOutput()};

    EXPECT_FALSE(result);
    EXPECT_EQ("Warning: couldn't load iga - kernel binaries won't be assembled.\n", output);

    EXPECT_EQ(0, igaDllMock->contextCreateCalledCount);
}

TEST_F(IgaWrapperTest, GivenContextCreationFailureWhenAssemblingGenIsaThenFalseIsReturnedAndErrorMessageIsPrinted) {
    igaDllMock->contextCreateReturnValue = igaStatusFailure;
    igaDllMock->statusToStringReturnValue = "SOME_CONTEXT_ERROR";

    const std::string inAsm{"Input ASM"};
    std::string out;

    StdoutCapturer capturer{};
    const auto result = testedIgaWrapper.tryAssembleGenISA(inAsm, out);
    const auto output{capturer.acquireOutput()};

    EXPECT_FALSE(result);
    EXPECT_EQ("Error while creating IGA Context! Error msg: SOME_CONTEXT_ERROR", output);

    EXPECT_EQ(1, igaDllMock->contextCreateCalledCount);
    EXPECT_EQ(1, igaDllMock->statusToStringCalledCount);
}

TEST_F(IgaWrapperTest, GivenContextCreationSuccessAndAssemblationFailureAndContextErrorsWhenAssemblingGenIsaThenFalseIsReturnedAndErrorMessageIsPrinted) {
    igaDllMock->contextCreateReturnValue = igaStatusSuccess;
    igaDllMock->assembleReturnValue = igaStatusFailure;
    igaDllMock->statusToStringReturnValue = "SOME_ASSEMBLE_ERROR";
    igaDllMock->contextGetErrorsReturnValue = igaStatusSuccess;
    igaDllMock->contextGetErrorsOutputString = "SPECIFIC_ERROR";
    igaDllMock->contextReleaseReturnValue = igaStatusSuccess;

    const std::string inAsm{"Input ASM"};
    std::string out;

    StdoutCapturer capturer{};
    const auto result = testedIgaWrapper.tryAssembleGenISA(inAsm, out);
    const auto output{capturer.acquireOutput()};

    EXPECT_FALSE(result);
    EXPECT_EQ("Error while assembling with IGA!\nStatus msg: SOME_ASSEMBLE_ERROR\n"
              "Errors: SPECIFIC_ERROR\n",
              output);

    EXPECT_EQ(1, igaDllMock->contextCreateCalledCount);
    EXPECT_EQ(1, igaDllMock->assembleCalledCount);
    EXPECT_EQ(1, igaDllMock->statusToStringCalledCount);
    EXPECT_EQ(1, igaDllMock->contextGetErrorsCalledCount);
    EXPECT_EQ(1, igaDllMock->contextReleaseCalledCount);
}

TEST_F(IgaWrapperTest, GivenContextCreationSuccessAndAssemblationSuccessAndNoWarningsWhenAssemblingGenIsaThenTrueIsReturnedAndLogsAreNotPrinted) {
    igaDllMock->contextCreateReturnValue = igaStatusSuccess;
    igaDllMock->assembleReturnValue = igaStatusSuccess;
    igaDllMock->assembleOutput = "DISASSEMBLED OUTPUT";
    igaDllMock->contextGetErrorsReturnValue = igaStatusSuccess;
    igaDllMock->contextReleaseReturnValue = igaStatusSuccess;

    const std::string inAsm{"Input ASM"};
    std::string out;

    StdoutCapturer capturer{};
    const auto result = testedIgaWrapper.tryAssembleGenISA(inAsm, out);
    const auto output{capturer.acquireOutput()};

    EXPECT_TRUE(result);
    EXPECT_TRUE(output.empty()) << output;

    EXPECT_EQ(1, igaDllMock->contextCreateCalledCount);
    EXPECT_EQ(1, igaDllMock->assembleCalledCount);
    EXPECT_EQ(0, igaDllMock->statusToStringCalledCount);
    EXPECT_EQ(0, igaDllMock->contextGetErrorsCalledCount);
    EXPECT_EQ(1, igaDllMock->contextGetWarningsCalledCount);
    EXPECT_EQ(1, igaDllMock->contextReleaseCalledCount);

    EXPECT_STREQ("DISASSEMBLED OUTPUT", out.c_str());
}

TEST_F(IgaWrapperTest, GivenIgcWrapperWhenLoadingLibraryTwiceWithPathChangeInTheMiddleThenLazyLoadingOccursAndLibraryIsLoadedOnlyOnce) {
    struct TestedIgaWrapper : public IgaWrapper {
        using IgaWrapper::tryLoadIga;
    };

    TestedIgaWrapper igaWrapper{};
    EXPECT_TRUE(igaWrapper.tryLoadIga());

    MockIgaDllGuard mockIgaDllGuard{"some_invalid_path_to_library"};
    mockIgaDllGuard.enable();

    // Lazy loading should prevent loading from wrong path.
    EXPECT_TRUE(igaWrapper.tryLoadIga());
}

TEST_F(IgaWrapperTest, GivenIgcWrapperWhenCallingSetGfxCoreMultipleTimesThenFirstValidGfxCoreFamilyIsPreserved) {
    ASSERT_FALSE(testedIgaWrapper.isKnownPlatform());
    constexpr auto invalidGfxCoreFamily = IGFX_MAX_CORE;

    testedIgaWrapper.setGfxCore(invalidGfxCoreFamily);
    EXPECT_FALSE(testedIgaWrapper.isKnownPlatform());

    testedIgaWrapper.setGfxCore(IGFX_GEN12LP_CORE);
    EXPECT_TRUE(testedIgaWrapper.isKnownPlatform());

    // Expect that a valid family is preserved.
    testedIgaWrapper.setGfxCore(invalidGfxCoreFamily);
    EXPECT_TRUE(testedIgaWrapper.isKnownPlatform());
}

TEST_F(IgaWrapperTest, GivenIgcWrapperWhenCallingSetProductFamilyMultipleTimesThenFirstValidProductFamilyIsPreserved) {
    ASSERT_FALSE(testedIgaWrapper.isKnownPlatform());
    constexpr auto invalidProductFamily = IGFX_MAX_PRODUCT;

    testedIgaWrapper.setProductFamily(invalidProductFamily);
    EXPECT_FALSE(testedIgaWrapper.isKnownPlatform());

    testedIgaWrapper.setProductFamily(IGFX_TIGERLAKE_LP);
    EXPECT_TRUE(testedIgaWrapper.isKnownPlatform());

    // Expect that a valid family is preserved.
    testedIgaWrapper.setProductFamily(invalidProductFamily);
    EXPECT_TRUE(testedIgaWrapper.isKnownPlatform());
}

} // namespace NEO