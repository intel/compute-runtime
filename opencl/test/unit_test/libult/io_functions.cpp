/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_io_functions.h"

namespace NEO {
namespace IoFunctions {
fopenFuncPtr fopenPtr = &mockFopen;
vfprintfFuncPtr vfprintfPtr = &mockVfptrinf;
fcloseFuncPtr fclosePtr = &mockFclose;
getenvFuncPtr getenvPtr = &mockGetenv;

uint32_t mockFopenCalled = 0;
uint32_t mockVfptrinfCalled = 0;
uint32_t mockFcloseCalled = 0;
bool returnMockEnvValue = false;
std::string mockEnvValue = "1";

} // namespace IoFunctions
} // namespace NEO
