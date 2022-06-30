/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(IoFunctionTest, WhenMockFunctionsSelectedThenCorrectFunctionPointerUsed) {
    IoFunctions::mockFopenCalled = 0;
    IoFunctions::mockVfptrinfCalled = 0;
    IoFunctions::mockFcloseCalled = 0;

    FILE *file = IoFunctions::fopenPtr("dummy file", "dummy mode");
    EXPECT_EQ(reinterpret_cast<FILE *>(0x40), file);
    int expectedPrintfRet = 0x10;
    int actualPrintfRet = IoFunctions::fprintf(file, "dummy format", "dummy string");
    EXPECT_EQ(expectedPrintfRet, actualPrintfRet);
    IoFunctions::fprintf(file, "another dummy format", "another dummy string");
    IoFunctions::fclosePtr(file);

    EXPECT_EQ(1u, IoFunctions::mockFopenCalled);
    EXPECT_EQ(2u, IoFunctions::mockVfptrinfCalled);
    EXPECT_EQ(1u, IoFunctions::mockFcloseCalled);
}
