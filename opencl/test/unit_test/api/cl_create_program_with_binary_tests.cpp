/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/intermediate_representations.h"

#include "opencl/test/unit_test/api/cl_api_tests.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

using namespace NEO;

using ClCreateProgramWithILTests = ApiTests;

namespace ULT {
TEST_F(ClCreateProgramWithILTests, GivenNonSpirvIlWhenCreatingProgramWithIlThenItIsTreatedAsPisaAndProgramIsCreated) {
    const uint32_t notSpirv[16] = {0xDEADBEEF};

    cl_int err = CL_SUCCESS;
    cl_program prog = clCreateProgramWithIL(pContext, notSpirv, sizeof(notSpirv), &err);
    EXPECT_EQ(CL_SUCCESS, err);
    ASSERT_NE(nullptr, prog);

    auto program = static_cast<MockProgram *>(prog);
    EXPECT_EQ(NEO::pisaCodeType, program->getIntermediateRepresentation());
    EXPECT_TRUE(program->getIsGeneratedByIgc());

    clReleaseProgram(prog);
}

TEST_F(ClCreateProgramWithILTests, GivenNonSpirvIlAndNoErrorPointerWhenCreatingProgramWithIlThenItIsTreatedAsPisaAndProgramIsCreated) {
    const uint32_t notSpirv[16] = {0xDEADBEEF};

    cl_program prog = clCreateProgramWithIL(pContext, notSpirv, sizeof(notSpirv), nullptr);
    ASSERT_NE(nullptr, prog);

    auto program = static_cast<MockProgram *>(prog);
    EXPECT_EQ(NEO::pisaCodeType, program->getIntermediateRepresentation());
    EXPECT_TRUE(program->getIsGeneratedByIgc());

    clReleaseProgram(prog);
}
} // namespace ULT
