/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/api/cl_api_tests.h"

using namespace NEO;

namespace ULT {

TEST(clSetProgramSpecializationConstantTest, givenNullptrProgramWhenSetProgramSpecializationConstantThenErrorIsReturned) {
    auto retVal = clSetProgramSpecializationConstant(nullptr, 1, 1, nullptr);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
}

using clSetProgramSpecializationConstantTests = api_tests;

TEST_F(clSetProgramSpecializationConstantTests, givenNonSpirVProgramWhenSetProgramSpecializationConstantThenErrorIsReturned) {
    pProgram->isSpirV = false;
    int specValue = 1;

    auto retVal = clSetProgramSpecializationConstant(pProgram, 1, sizeof(int), &specValue);
    EXPECT_EQ(CL_INVALID_PROGRAM, retVal);
}

TEST_F(clSetProgramSpecializationConstantTests, givenProperProgramAndNullptrSpecValueWhenSetProgramSpecializationConstantThenErrorIsReturned) {
    pProgram->isSpirV = true;
    auto retVal = clSetProgramSpecializationConstant(pProgram, 1, 1, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

} // namespace ULT
