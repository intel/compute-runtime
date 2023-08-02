/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/pin/pin.h"
#include "shared/test/common/mocks/mock_os_library.h"

#include "gtest/gtest.h"

namespace ULT {

TEST(PinInitializationTest, GivenValidLibraryPinContextInitSucceeds) {
    uint32_t (*openPinHandler)(void *) = [](void *arg) -> uint32_t { return 0; };
    MockOsLibrary mockLibraryObject(reinterpret_cast<void *>(openPinHandler), false);
    MockOsLibrary::loadLibraryNewObject = &mockLibraryObject;
    NEO::PinContext::osLibraryLoadFunction = MockOsLibrary::load;
    std::string mockGTPinFunctionName{"aaa"};
    EXPECT_TRUE(NEO::PinContext::init(mockGTPinFunctionName));

    MockOsLibrary::loadLibraryNewObject = nullptr;
}

TEST(PinInitializationTest, GivenBadLibraryNamePinContextInitFAILS) {
    MockOsLibrary::loadLibraryNewObject = nullptr;
    NEO::PinContext::osLibraryLoadFunction = MockOsLibrary::load;
    std::string mockGTPinFunctionName{"aaa"};
    EXPECT_FALSE(NEO::PinContext::init(mockGTPinFunctionName));

    MockOsLibrary::loadLibraryNewObject = nullptr;
}

TEST(PinInitializationTest, GivenBadProcAddressPinContextInitFAILS) {
    MockOsLibrary mockLibraryObject(nullptr, false);
    MockOsLibrary::loadLibraryNewObject = &mockLibraryObject;
    NEO::PinContext::osLibraryLoadFunction = MockOsLibrary::load;
    std::string mockGTPinFunctionName{"aaa"};
    EXPECT_FALSE(NEO::PinContext::init(mockGTPinFunctionName));

    MockOsLibrary::loadLibraryNewObject = nullptr;
}

TEST(PinInitializationTest, GivenBadPinHandlerPinContextInitFAILS) {
    uint32_t (*openPinHandler)(void *) = [](void *arg) -> uint32_t { return 1; };
    MockOsLibrary mockLibraryObject(reinterpret_cast<void *>(openPinHandler), false);
    MockOsLibrary::loadLibraryNewObject = &mockLibraryObject;
    NEO::PinContext::osLibraryLoadFunction = MockOsLibrary::load;
    std::string mockGTPinFunctionName{"aaa"};
    EXPECT_FALSE(NEO::PinContext::init(mockGTPinFunctionName));

    MockOsLibrary::loadLibraryNewObject = nullptr;
}

} // namespace ULT
