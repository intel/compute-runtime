/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_os_library.h"

#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

#include <string>
#include <unordered_map>

using namespace NEO;

namespace ULT {

uint32_t gtpinInitTimesCalled = 0u;

TEST(GTPinInitNotifyTests, givenAvailablePlatformsAndNoEnvironmentVariableSetWhenTryingToNotifyGtpinInitializationThenGtpinDoesNotGetNotified) {
    platformsImpl->clear();
    constructPlatform();
    ASSERT_FALSE(platformsImpl->empty());

    VariableBackup<uint32_t> gtpinCounterBackup(&gtpinInitTimesCalled, 0u);
    uint32_t (*openPinHandler)(void *) = [](void *arg) -> uint32_t { gtpinInitTimesCalled++; return 0; };
    MockOsLibrary::loadLibraryNewObject = new MockOsLibrary(reinterpret_cast<void *>(openPinHandler), false);
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};

    gtPinTryNotifyInit();
    EXPECT_EQ(0u, gtpinInitTimesCalled);
    platformsImpl->clear();
    delete MockOsLibrary::loadLibraryNewObject;
}

TEST(GTPinInitNotifyTests, givenNoPlatformsAvailableAndEnvironmentVariableSetWhenTryingToNotifyGtpinInitializationThenGtpinDoesNotGetNotified) {
    platformsImpl->clear();
    ASSERT_TRUE(platformsImpl->empty());

    VariableBackup<uint32_t> gtpinCounterBackup(&gtpinInitTimesCalled, 0u);
    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_INSTRUMENTATION", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    uint32_t (*openPinHandler)(void *) = [](void *arg) -> uint32_t { gtpinInitTimesCalled++; return 0; };
    MockOsLibrary::loadLibraryNewObject = new MockOsLibrary(reinterpret_cast<void *>(openPinHandler), false);
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};

    gtPinTryNotifyInit();
    EXPECT_EQ(0u, gtpinInitTimesCalled);
    platformsImpl->clear();
    delete MockOsLibrary::loadLibraryNewObject;
}

TEST(GTPinInitNotifyTests, givenAvailablePlatformsAndEnvironmentVariableSetWhenTryingToNotifyGtpinInitializationThenGtpinGetsNotified) {
    platformsImpl->clear();
    constructPlatform();
    ASSERT_FALSE(platformsImpl->empty());

    VariableBackup<uint32_t> gtpinCounterBackup(&gtpinInitTimesCalled, 0u);
    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_INSTRUMENTATION", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    uint32_t (*openPinHandler)(void *) = [](void *arg) -> uint32_t { gtpinInitTimesCalled++; return 0; };
    MockOsLibrary::loadLibraryNewObject = new MockOsLibrary(reinterpret_cast<void *>(openPinHandler), false);
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};

    gtPinTryNotifyInit();
    EXPECT_EQ(1u, gtpinInitTimesCalled);
    platformsImpl->clear();
}

} // namespace ULT
