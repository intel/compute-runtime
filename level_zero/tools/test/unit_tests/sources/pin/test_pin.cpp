/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_os_library.h"

#include "level_zero/tools/source/pin/pin.h"

#include "gtest/gtest.h"

namespace ult {

TEST(PinInitializationTest, GivenValidLibraryPinContextInitSucceeds) {
    uint32_t (*openPinHandler)(void *) = [](void *arg) -> uint32_t { return 0; };
    auto newPtr = new MockOsLibrary(reinterpret_cast<void *>(openPinHandler), false);
    MockOsLibrary::loadLibraryNewObject = newPtr;
    L0::PinContext::osLibraryLoadFunction = MockOsLibrary::load;
    EXPECT_EQ(L0::PinContext::init(), ZE_RESULT_SUCCESS);
    L0::PinContext::osLibraryLoadFunction = NEO::OsLibrary::load;
    MockOsLibrary::loadLibraryNewObject = nullptr;
    delete newPtr;
}

TEST(PinInitializationTest, GivenBadLibraryNamePinContextInitFAILS) {
    MockOsLibrary::loadLibraryNewObject = nullptr;
    L0::PinContext::osLibraryLoadFunction = MockOsLibrary::load;
    EXPECT_EQ(L0::PinContext::init(), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
    L0::PinContext::osLibraryLoadFunction = NEO::OsLibrary::load;
    MockOsLibrary::loadLibraryNewObject = nullptr;
}

TEST(PinInitializationTest, GivenBadProcAddressPinContextInitFAILS) {
    auto newPtr = new MockOsLibrary(nullptr, false);
    MockOsLibrary::loadLibraryNewObject = newPtr;
    L0::PinContext::osLibraryLoadFunction = MockOsLibrary::load;
    EXPECT_EQ(L0::PinContext::init(), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
    L0::PinContext::osLibraryLoadFunction = NEO::OsLibrary::load;
    MockOsLibrary::loadLibraryNewObject = nullptr;
    delete newPtr;
}

TEST(PinInitializationTest, GivenBadPinHandlerPinContextInitFAILS) {
    uint32_t (*openPinHandler)(void *) = [](void *arg) -> uint32_t { return 1; };
    auto newPtr = new MockOsLibrary(reinterpret_cast<void *>(openPinHandler), false);
    MockOsLibrary::loadLibraryNewObject = newPtr;
    L0::PinContext::osLibraryLoadFunction = MockOsLibrary::load;
    EXPECT_EQ(L0::PinContext::init(), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
    L0::PinContext::osLibraryLoadFunction = NEO::OsLibrary::load;
    MockOsLibrary::loadLibraryNewObject = nullptr;
    delete newPtr;
}

} // namespace ult
