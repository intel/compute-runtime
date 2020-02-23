/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clSVMFreeTests;

namespace ULT {

TEST_F(clSVMFreeTests, GivenNullPtrWhenFreeingSvmThenNoAction) {
    clSVMFree(
        nullptr, // cl_context context
        nullptr  // void *svm_pointer
    );
}
} // namespace ULT
