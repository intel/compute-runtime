/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocloc_igc_facade_tests.h"

#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "mock/mock_ocloc_igc_facade.h"

#include <string>

namespace NEO {

TEST_F(OclocIgcFacadeTest, GivenMissingIgcLibraryWhenPreparingIgcThenFailureIsReported) {
    MockOclocIgcFacade mockIgcFacade{&mockArgHelper};
    mockIgcFacade.shouldFailLoadingOfIgcLib = true;

    ::testing::internal::CaptureStdout();
    const auto igcPreparationResult{mockIgcFacade.initialize(hwInfo)};
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::OUT_OF_HOST_MEMORY, igcPreparationResult);
    EXPECT_FALSE(mockIgcFacade.isInitialized());

    std::stringstream expectedErrorMessage;
    expectedErrorMessage << "Error! Loading of IGC library has failed! Filename: " << Os::igcDllName << "\n";

    EXPECT_EQ(expectedErrorMessage.str(), output);
}

TEST_F(OclocIgcFacadeTest, GivenFailingLoadingOfIgcSymbolsWhenPreparingIgcThenFailureIsReported) {
    MockOclocIgcFacade mockIgcFacade{&mockArgHelper};
    mockIgcFacade.shouldFailLoadingOfIgcCreateMainFunction = true;

    ::testing::internal::CaptureStdout();
    const auto igcPreparationResult{mockIgcFacade.initialize(hwInfo)};
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::OUT_OF_HOST_MEMORY, igcPreparationResult);
    EXPECT_FALSE(mockIgcFacade.isInitialized());

    const std::string expectedErrorMessage{"Error! Cannot load required functions from IGC library.\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OclocIgcFacadeTest, GivenFailingCreationOfIgcMainWhenPreparingIgcThenFailureIsReported) {
    MockOclocIgcFacade mockIgcFacade{&mockArgHelper};
    mockIgcFacade.shouldFailCreationOfIgcMain = true;

    ::testing::internal::CaptureStdout();
    const auto igcPreparationResult{mockIgcFacade.initialize(hwInfo)};
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::OUT_OF_HOST_MEMORY, igcPreparationResult);
    EXPECT_FALSE(mockIgcFacade.isInitialized());

    const std::string expectedErrorMessage{"Error! Cannot create IGC main component!\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OclocIgcFacadeTest, GivenIncompatibleIgcInterfacesWhenPreparingIgcThenFailureIsReported) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.EnableDebugBreak.set(false);

    MockOclocIgcFacade mockIgcFacade{&mockArgHelper};
    mockIgcFacade.isIgcInterfaceCompatibleReturnValue = false;
    mockIgcFacade.getIncompatibleInterfaceReturnValue = "SomeImportantInterface";

    ::testing::internal::CaptureStdout();
    const auto igcPreparationResult{mockIgcFacade.initialize(hwInfo)};
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::OUT_OF_HOST_MEMORY, igcPreparationResult);
    EXPECT_FALSE(mockIgcFacade.isInitialized());

    const std::string expectedErrorMessage{"Error! Incompatible interface in IGC: SomeImportantInterface\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OclocIgcFacadeTest, GivenMissingPatchtokenInterfaceWhenPreparingIgcThenFailureIsReported) {
    MockOclocIgcFacade mockIgcFacade{&mockArgHelper};
    mockIgcFacade.isPatchtokenInterfaceSupportedReturnValue = false;

    ::testing::internal::CaptureStdout();
    const auto igcPreparationResult{mockIgcFacade.initialize(hwInfo)};
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::OUT_OF_HOST_MEMORY, igcPreparationResult);
    EXPECT_FALSE(mockIgcFacade.isInitialized());

    const std::string expectedErrorMessage{"Error! Patchtoken interface is missing.\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OclocIgcFacadeTest, GivenFailingCreationOfIgcDeviceContextWhenPreparingIgcThenFailureIsReported) {
    MockOclocIgcFacade mockIgcFacade{&mockArgHelper};
    mockIgcFacade.shouldFailCreationOfIgcDeviceContext = true;

    ::testing::internal::CaptureStdout();
    const auto igcPreparationResult{mockIgcFacade.initialize(hwInfo)};
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::OUT_OF_HOST_MEMORY, igcPreparationResult);
    EXPECT_FALSE(mockIgcFacade.isInitialized());

    const std::string expectedErrorMessage{"Error! Cannot create IGC device context!\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OclocIgcFacadeTest, GivenInvalidIgcDeviceContextWhenPreparingIgcThenFailureIsReported) {
    constexpr std::array invalidReturnFlags = {
        &MockOclocIgcFacade::shouldReturnInvalidIgcPlatformHandle,
        &MockOclocIgcFacade::shouldReturnInvalidGTSystemInfoHandle,
        &MockOclocIgcFacade::shouldReturnInvalidIgcFeaturesAndWorkaroundsHandle};

    for (const auto &invalidReturnFlag : invalidReturnFlags) {
        MockOclocIgcFacade mockIgcFacade{&mockArgHelper};
        mockIgcFacade.*invalidReturnFlag = true;

        ::testing::internal::CaptureStdout();
        const auto igcPreparationResult{mockIgcFacade.initialize(hwInfo)};
        const auto output{::testing::internal::GetCapturedStdout()};

        EXPECT_EQ(OclocErrorCode::OUT_OF_HOST_MEMORY, igcPreparationResult);
        EXPECT_FALSE(mockIgcFacade.isInitialized());

        const std::string expectedErrorMessage{"Error! IGC device context has not been properly created!\n"};
        EXPECT_EQ(expectedErrorMessage, output);
    }
}

TEST_F(OclocIgcFacadeTest, GivenNoneErrorsSetWhenPreparingIgcThenSuccessIsReported) {
    MockOclocIgcFacade mockIgcFacade{&mockArgHelper};

    ::testing::internal::CaptureStdout();
    const auto igcPreparationResult{mockIgcFacade.initialize(hwInfo)};
    const auto output{::testing::internal::GetCapturedStdout()};

    EXPECT_EQ(OclocErrorCode::SUCCESS, igcPreparationResult);
    EXPECT_TRUE(output.empty()) << output;
    EXPECT_TRUE(mockIgcFacade.isInitialized());
}

TEST_F(OclocIgcFacadeTest, GivenInitializedIgcWhenGettingIncompatibleInterfaceThenEmptyStringIsReturned) {
    MockOclocIgcFacade mockIgcFacade{&mockArgHelper};

    ::testing::internal::CaptureStdout();
    const auto igcPreparationResult{mockIgcFacade.initialize(hwInfo)};
    const auto output{::testing::internal::GetCapturedStdout()};

    ASSERT_EQ(OclocErrorCode::SUCCESS, igcPreparationResult);

    const std::vector<CIF::InterfaceId_t> interfacesToIgnore{};
    const auto incompatibleInterface = mockIgcFacade.getIncompatibleInterface(interfacesToIgnore);

    EXPECT_TRUE(incompatibleInterface.empty()) << incompatibleInterface;
}

} // namespace NEO