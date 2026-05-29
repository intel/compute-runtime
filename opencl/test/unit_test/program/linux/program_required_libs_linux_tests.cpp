/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/program_info.h"
#include "shared/source/utilities/io_functions.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

#include <cstdio>
#include <optional>
#include <string>

namespace NEO {

struct ProgramRequiredLibsLinuxTest : public ClDeviceFixture, public ::testing::Test {
    void SetUp() override {
        ClDeviceFixture::setUp();

        ZebinTestData::ValidEmptyProgram<> libZebin;
        libZebinBytes.assign(libZebin.storage.begin(), libZebin.storage.end());

        mockFreadBufferBackup.emplace(&IoFunctions::mockFreadBuffer, libZebinBytes.data());
        mockFreadReturnBackup.emplace(&IoFunctions::mockFreadReturn, libZebinBytes.size());
        mockFtellReturnBackup.emplace(&IoFunctions::mockFtellReturn, static_cast<long int>(libZebinBytes.size()));
    }

    void TearDown() override {
        ClDeviceFixture::tearDown();
    }

    void forceFopenFailure() {
        mockFopenReturnedBackup.emplace(&IoFunctions::mockFopenReturned, nullptr);
    }

    static constexpr const char *mockedSearchPath = "/mocked/search/path";

    std::vector<char> libZebinBytes;

    std::optional<VariableBackup<decltype(IoFunctions::mockFreadBuffer)>> mockFreadBufferBackup;
    std::optional<VariableBackup<decltype(IoFunctions::mockFreadReturn)>> mockFreadReturnBackup;
    std::optional<VariableBackup<decltype(IoFunctions::mockFtellReturn)>> mockFtellReturnBackup;
    std::optional<VariableBackup<decltype(IoFunctions::mockFopenReturned)>> mockFopenReturnedBackup;
};

TEST_F(ProgramRequiredLibsLinuxTest, givenLibAvailableWhenLoadRequiredLibBinaryCalledThenBytesReturned) {
    auto bytes = pClDevice->loadRequiredLibBinary(mockedSearchPath, "libTestLib.zebin");
    ASSERT_FALSE(bytes.empty());
    EXPECT_EQ(libZebinBytes.size(), bytes.size());
    EXPECT_EQ(0, memcmp(bytes.data(), libZebinBytes.data(), bytes.size()));
}

TEST_F(ProgramRequiredLibsLinuxTest, givenFileNotAvailableWhenLoadRequiredLibBinaryCalledThenEmptyReturned) {
    forceFopenFailure();
    auto bytes = pClDevice->loadRequiredLibBinary(mockedSearchPath, "libMissing.zebin");
    EXPECT_TRUE(bytes.empty());
}

TEST_F(ProgramRequiredLibsLinuxTest, givenLibAvailableWhenGetRequiredLibProgramCalledThenLibLoadedAndCached) {
    DebugManagerStateRestore restorer;
    debugManager.flags.RequiredLibsBinarySearchPath.set(mockedSearchPath);

    const std::string libName = "libTestLib.zebin";

    auto *libProgram = pClDevice->getRequiredLibProgram(libName);
    ASSERT_NE(nullptr, libProgram);

    auto lock = pClDevice->requiredLibsRegistry.lock();
    EXPECT_TRUE(pClDevice->requiredLibsRegistry->contains(libName));
    EXPECT_EQ(libProgram, pClDevice->requiredLibsRegistry->at(libName).get());
}

TEST_F(ProgramRequiredLibsLinuxTest, givenLibAvailableWhenGetCalledTwiceThenSecondCallReturnsCached) {
    DebugManagerStateRestore restorer;
    debugManager.flags.RequiredLibsBinarySearchPath.set(mockedSearchPath);

    const std::string libName = "libCached.zebin";

    auto *first = pClDevice->getRequiredLibProgram(libName);
    auto *second = pClDevice->getRequiredLibProgram(libName);
    ASSERT_NE(nullptr, first);
    EXPECT_EQ(first, second);
}

TEST_F(ProgramRequiredLibsLinuxTest, givenLibNotAvailableWhenGetRequiredLibProgramCalledThenReturnsNull) {
    DebugManagerStateRestore restorer;
    debugManager.flags.RequiredLibsBinarySearchPath.set(mockedSearchPath);
    forceFopenFailure();

    auto *libProgram = pClDevice->getRequiredLibProgram("libNonExistent.zebin");
    EXPECT_EQ(nullptr, libProgram);

    auto lock = pClDevice->requiredLibsRegistry.lock();
    EXPECT_FALSE(pClDevice->requiredLibsRegistry->contains("libNonExistent.zebin"));
}

TEST_F(ProgramRequiredLibsLinuxTest, givenLibAvailableWhenResolveRequiredLibsCalledThenLibLoadedIntoRegistry) {
    DebugManagerStateRestore restorer;
    debugManager.flags.RequiredLibsBinarySearchPath.set(mockedSearchPath);

    const std::string libName = "libResolve.zebin";

    auto program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*pClDevice));

    ProgramInfo programInfo;
    programInfo.requiredLibs.emplace_back(libName);

    auto retVal = program->resolveRequiredLibs(*pClDevice, programInfo);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();
    auto &reqLibs = program->buildInfos[rootDeviceIndex].requiredLibPrograms;
    ASSERT_EQ(1U, reqLibs.size());
    EXPECT_NE(nullptr, reqLibs[0]);
}

TEST_F(ProgramRequiredLibsLinuxTest, givenLibNotAvailableWhenResolveRequiredLibsCalledThenFailureAndBuildLogPopulated) {
    DebugManagerStateRestore restorer;
    debugManager.flags.RequiredLibsBinarySearchPath.set(mockedSearchPath);
    forceFopenFailure();

    auto program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*pClDevice));

    ProgramInfo programInfo;
    programInfo.requiredLibs.emplace_back("libMissing.zebin");

    auto retVal = program->resolveRequiredLibs(*pClDevice, programInfo);
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();
    std::string buildLog{program->getBuildLog(rootDeviceIndex)};
    EXPECT_NE(std::string::npos, buildLog.find("libMissing.zebin"));
}

TEST_F(ProgramRequiredLibsLinuxTest, givenMultipleLibsAvailableWhenResolveCalledThenAllLibsLoaded) {
    DebugManagerStateRestore restorer;
    debugManager.flags.RequiredLibsBinarySearchPath.set(mockedSearchPath);

    auto program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*pClDevice));

    ProgramInfo programInfo;
    programInfo.requiredLibs.emplace_back("libA.zebin");
    programInfo.requiredLibs.emplace_back("libB.zebin");

    auto retVal = program->resolveRequiredLibs(*pClDevice, programInfo);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();
    auto &reqLibs = program->buildInfos[rootDeviceIndex].requiredLibPrograms;
    EXPECT_EQ(2U, reqLibs.size());

    auto lock = pClDevice->requiredLibsRegistry.lock();
    EXPECT_TRUE(pClDevice->requiredLibsRegistry->contains("libA.zebin"));
    EXPECT_TRUE(pClDevice->requiredLibsRegistry->contains("libB.zebin"));
}

} // namespace NEO
