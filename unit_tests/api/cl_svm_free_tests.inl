/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"

using namespace OCLRT;

typedef api_tests clSVMFreeTests;

namespace ULT {

TEST_F(clSVMFreeTests, invalidParams) {
    clSVMFree(
        nullptr, // cl_context context
        nullptr  // void *svm_pointer
    );
}
} // namespace ULT
