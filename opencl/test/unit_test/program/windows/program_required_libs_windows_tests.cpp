/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/program_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "gtest/gtest.h"

#include <string>

namespace NEO {

struct ProgramRequiredLibsWindowsTest : public ClDeviceFixture, public ::testing::Test {
    void SetUp() override {
        ClDeviceFixture::setUp();
        program = std::make_unique<MockProgram>(nullptr, false, toClDeviceVector(*pClDevice));
    }

    void TearDown() override {
        program.reset();
        ClDeviceFixture::tearDown();
    }

    std::unique_ptr<MockProgram> program;
};

TEST_F(ProgramRequiredLibsWindowsTest, givenProgramWithRequiredLibsOnWindowsWhenBuiltThenBuildLogReportsMissingDeps) {
    ProgramInfo programInfo;
    programInfo.requiredLibs.emplace_back("libSomeDep.zebin");

    auto retVal = program->resolveRequiredLibs(*pClDevice, programInfo);
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();
    std::string buildLog{program->getBuildLog(rootDeviceIndex)};
    EXPECT_NE(std::string::npos, buildLog.find("libSomeDep.zebin"));
}

TEST_F(ProgramRequiredLibsWindowsTest, givenMultipleRequiredLibsOnWindowsWhenBuiltThenFirstMissingLibReported) {
    ProgramInfo programInfo;
    programInfo.requiredLibs.emplace_back("libFirst.zebin");
    programInfo.requiredLibs.emplace_back("libSecond.zebin");

    auto retVal = program->resolveRequiredLibs(*pClDevice, programInfo);
    EXPECT_EQ(CL_BUILD_PROGRAM_FAILURE, retVal);

    auto rootDeviceIndex = pClDevice->getRootDeviceIndex();
    std::string buildLog{program->getBuildLog(rootDeviceIndex)};
    EXPECT_NE(std::string::npos, buildLog.find("libFirst.zebin"));
}

TEST_F(ProgramRequiredLibsWindowsTest, givenEmptyRequiredLibsOnWindowsWhenBuiltThenSuccess) {
    ProgramInfo programInfo;

    auto retVal = program->resolveRequiredLibs(*pClDevice, programInfo);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(program->buildInfos[pClDevice->getRootDeviceIndex()].requiredLibPrograms.empty());
}

} // namespace NEO
