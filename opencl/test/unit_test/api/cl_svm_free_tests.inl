/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace NEO;

using ClSVMFreeTests = ApiTests;

namespace ULT {

TEST_F(ClSVMFreeTests, GivenNullPtrWhenFreeingSvmThenNoAction) {
    clSVMFree(
        nullptr, // cl_context context
        nullptr  // void *svm_pointer
    );
}

} // namespace ULT
