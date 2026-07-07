/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_os_library.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/gtpin/leo_gtpin_notify.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct LeoGtpinInitTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();

        uint32_t (*openPinHandler)(void *) = [](void *arg) -> uint32_t {
            gtpinInitTimesCalled++;
            return 0;
        };

        MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, false);
        auto osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
        osLibrary->procMap["OpenGTPinOCL"] = reinterpret_cast<void *>(openPinHandler);
    }

    void TearDown() override {
        delete MockOsLibrary::loadLibraryNewObject;
        MockOsLibrary::loadLibraryNewObject = nullptr;
        Test<OclFixture>::TearDown();
    }

    static uint32_t gtpinInitTimesCalled;

    VariableBackup<uint32_t> gtpinCounterBackup{&gtpinInitTimesCalled, 0};
    std::unordered_map<std::string, std::string> mockableEnvs;
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup{&IoFunctions::mockableEnvValues, &mockableEnvs};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};
    VariableBackup<NEO::OsLibrary *> osLibraryBackup{&MockOsLibrary::loadLibraryNewObject, nullptr};
};
uint32_t LeoGtpinInitTest::gtpinInitTimesCalled = 0u;

TEST_F(LeoGtpinInitTest, givenInstrumentationEnabledWhenTryNotifyGtpinInitThenOpensOclGtpinFrontend) {
    mockableEnvs["ZET_ENABLE_PROGRAM_INSTRUMENTATION"] = "1";

    platform->tryNotifyGtpinInit();

    EXPECT_EQ(1u, gtpinInitTimesCalled);
}

TEST_F(LeoGtpinInitTest, givenInstrumentationDisabledWhenTryNotifyGtpinInitThenDoesNotOpenGtpin) {
    platform->tryNotifyGtpinInit();

    EXPECT_EQ(0u, gtpinInitTimesCalled);
}

TEST_F(LeoGtpinInitTest, givenInstrumentationEnabledWhenTryNotifyGtpinInitCalledMultipleTimesThenGtpinIsInitializedOnlyOnce) {
    mockableEnvs["ZET_ENABLE_PROGRAM_INSTRUMENTATION"] = "1";

    platform->tryNotifyGtpinInit();
    platform->tryNotifyGtpinInit();

    EXPECT_EQ(1u, gtpinInitTimesCalled);
}

TEST_F(LeoGtpinInitTest, givenPopulatedPlatformsWhenGtPinTryNotifyInitThenDelegatesToFirstPlatform) {
    mockableEnvs["ZET_ENABLE_PROGRAM_INSTRUMENTATION"] = "1";

    std::vector<std::unique_ptr<Platform>> platforms;
    platforms.push_back(std::make_unique<Platform>(driverHandle->toHandle()));
    VariableBackup<decltype(platformsImpl)> platformsBackup{&platformsImpl, &platforms};

    gtPinTryNotifyInit();

    EXPECT_EQ(1u, gtpinInitTimesCalled);
}

TEST_F(LeoGtpinInitTest, givenNullPlatformsWhenGtPinTryNotifyInitThenDoesNotInitializeGtpin) {
    mockableEnvs["ZET_ENABLE_PROGRAM_INSTRUMENTATION"] = "1";
    VariableBackup<decltype(platformsImpl)> platformsBackup{&platformsImpl, nullptr};

    gtPinTryNotifyInit();

    EXPECT_EQ(0u, gtpinInitTimesCalled);
}

TEST_F(LeoGtpinInitTest, givenEmptyPlatformsWhenGtPinTryNotifyInitThenDoesNotInitializeGtpin) {
    mockableEnvs["ZET_ENABLE_PROGRAM_INSTRUMENTATION"] = "1";
    std::vector<std::unique_ptr<Platform>> platforms;
    VariableBackup<decltype(platformsImpl)> platformsBackup{&platformsImpl, &platforms};

    gtPinTryNotifyInit();

    EXPECT_EQ(0u, gtpinInitTimesCalled);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
