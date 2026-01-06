/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocloc_fcl_facade_tests.h"

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/os_inc_base.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"

#include "mock/mock_ocloc_fcl_facade.h"

#include <string>

namespace NEO {

TEST_F(OclocFclFacadeTest, GivenMissingFclLibraryWhenPreparingFclThenFailureIsReported) {
    MockOclocFclFacade mockFclFacade{&mockArgHelper};
    mockFclFacade.shouldFailLoadingOfFclLib = true;

    StreamCapture capture;
    capture.captureStdout();
    const auto fclPreparationResult{mockFclFacade.initialize(hwInfo)};
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_OUT_OF_HOST_MEMORY, fclPreparationResult);
    EXPECT_FALSE(mockFclFacade.isInitialized());

    std::stringstream expectedErrorMessage;
    expectedErrorMessage << "Error! Loading of FCL library has failed! Filename: " << Os::frontEndDllName << "\n";

    EXPECT_EQ(expectedErrorMessage.str(), output);
}

TEST_F(OclocFclFacadeTest, GivenFailingLoadingOfFclSymbolsWhenPreparingFclThenFailureIsReported) {
    MockOclocFclFacade mockFclFacade{&mockArgHelper};
    mockFclFacade.shouldFailLoadingOfFclCreateMainFunction = true;

    StreamCapture capture;
    capture.captureStdout();
    const auto fclPreparationResult{mockFclFacade.initialize(hwInfo)};
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_OUT_OF_HOST_MEMORY, fclPreparationResult);
    EXPECT_FALSE(mockFclFacade.isInitialized());

    const std::string expectedErrorMessage{"Error! Cannot load required functions from FCL library.\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OclocFclFacadeTest, GivenFailingCreationOfFclMainWhenPreparingFclThenFailureIsReported) {
    MockOclocFclFacade mockFclFacade{&mockArgHelper};
    mockFclFacade.shouldFailCreationOfFclMain = true;

    StreamCapture capture;
    capture.captureStdout();
    const auto fclPreparationResult{mockFclFacade.initialize(hwInfo)};
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_OUT_OF_HOST_MEMORY, fclPreparationResult);
    EXPECT_FALSE(mockFclFacade.isInitialized());

    const std::string expectedErrorMessage{"Error! Cannot create FCL main component!\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OclocFclFacadeTest, GivenIncompatibleFclInterfacesWhenPreparingFclThenFailureIsReported) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.EnableDebugBreak.set(false);

    MockOclocFclFacade mockFclFacade{&mockArgHelper};
    mockFclFacade.isFclInterfaceCompatibleReturnValue = false;
    mockFclFacade.getIncompatibleInterfaceReturnValue = "SomeImportantInterface";

    StreamCapture capture;
    capture.captureStdout();
    const auto fclPreparationResult{mockFclFacade.initialize(hwInfo)};
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_OUT_OF_HOST_MEMORY, fclPreparationResult);
    EXPECT_FALSE(mockFclFacade.isInitialized());

    const std::string expectedErrorMessage{"Error! Incompatible interface in FCL: SomeImportantInterface\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OclocFclFacadeTest, GivenFailingCreationOfFclDeviceContextWhenPreparingFclThenFailureIsReported) {
    MockOclocFclFacade mockFclFacade{&mockArgHelper};
    mockFclFacade.shouldFailCreationOfFclDeviceContext = true;

    StreamCapture capture;
    capture.captureStdout();
    const auto fclPreparationResult{mockFclFacade.initialize(hwInfo)};
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_OUT_OF_HOST_MEMORY, fclPreparationResult);
    EXPECT_FALSE(mockFclFacade.isInitialized());

    const std::string expectedErrorMessage{"Error! Cannot create FCL device context!\n"};
    EXPECT_EQ(expectedErrorMessage, output);
}

TEST_F(OclocFclFacadeTest, GivenNoneErrorsSetAndNotPopulateFclInterfaceWhenPreparingFclThenSuccessIsReported) {
    MockOclocFclFacade mockFclFacade{&mockArgHelper};
    mockFclFacade.shouldPopulateFclInterfaceReturnValue = false;

    StreamCapture capture;
    capture.captureStdout();
    const auto fclPreparationResult{mockFclFacade.initialize(hwInfo)};
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_SUCCESS, fclPreparationResult);
    EXPECT_TRUE(output.empty()) << output;
    EXPECT_TRUE(mockFclFacade.isInitialized());
    EXPECT_EQ(0, mockFclFacade.populateFclInterfaceCalledCount);
}

TEST_F(OclocFclFacadeTest, GivenPopulateFclInterfaceAndInvalidFclDeviceContextWhenPreparingFclThenFailureIsReported) {
    MockOclocFclFacade mockFclFacade{&mockArgHelper};
    mockFclFacade.shouldPopulateFclInterfaceReturnValue = true;
    mockFclFacade.shouldReturnInvalidFclPlatformHandle = true;

    StreamCapture capture;
    capture.captureStdout();
    const auto fclPreparationResult{mockFclFacade.initialize(hwInfo)};
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_OUT_OF_HOST_MEMORY, fclPreparationResult);
    EXPECT_FALSE(mockFclFacade.isInitialized());

    const std::string expectedErrorMessage{"Error! FCL device context has not been properly created!\n"};
    EXPECT_EQ(expectedErrorMessage, output);

    EXPECT_EQ(0, mockFclFacade.populateFclInterfaceCalledCount);
}

TEST_F(OclocFclFacadeTest, GivenPopulateFclInterfaceWhenPreparingFclThenSuccessIsReported) {
    MockOclocFclFacade mockFclFacade{&mockArgHelper};
    mockFclFacade.shouldPopulateFclInterfaceReturnValue = true;

    StreamCapture capture;
    capture.captureStdout();
    const auto fclPreparationResult{mockFclFacade.initialize(hwInfo)};
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_SUCCESS, fclPreparationResult);
    EXPECT_TRUE(output.empty()) << output;
    EXPECT_TRUE(mockFclFacade.isInitialized());

    EXPECT_EQ(1, mockFclFacade.populateFclInterfaceCalledCount);
}

TEST_F(OclocFclFacadeTest, GivenNoneErrorsSetWhenPreparingFclThenSuccessIsReported) {
    MockOclocFclFacade mockFclFacade{&mockArgHelper};

    StreamCapture capture;
    capture.captureStdout();
    const auto fclPreparationResult{mockFclFacade.initialize(hwInfo)};
    const auto output{capture.getCapturedStdout()};

    EXPECT_EQ(OCLOC_SUCCESS, fclPreparationResult);
    EXPECT_TRUE(output.empty()) << output;
    EXPECT_TRUE(mockFclFacade.isInitialized());

    const auto expectedPopulateCalledCount = mockFclFacade.shouldPopulateFclInterface() ? 1 : 0;
    EXPECT_EQ(expectedPopulateCalledCount, mockFclFacade.populateFclInterfaceCalledCount);
}

TEST_F(OclocFclFacadeTest, GivenInitializedFclWhenGettingIncompatibleInterfaceThenEmptyStringIsReturned) {
    MockOclocFclFacade mockFclFacade{&mockArgHelper};

    StreamCapture capture;
    capture.captureStdout();
    const auto fclPreparationResult{mockFclFacade.initialize(hwInfo)};
    const auto output{capture.getCapturedStdout()};

    ASSERT_EQ(OCLOC_SUCCESS, fclPreparationResult);

    const auto incompatibleInterface = mockFclFacade.getIncompatibleInterface();
    EXPECT_TRUE(incompatibleInterface.empty()) << incompatibleInterface;
}

} // namespace NEO