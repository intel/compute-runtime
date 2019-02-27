/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "offline_compiler/offline_compiler.h"

#include "gtest/gtest.h"
#include <CL/cl.h>

#include <cstdint>

namespace OCLRT {

class OfflineCompilerTests : public ::testing::Test {
  public:
    OfflineCompilerTests() : pOfflineCompiler(nullptr),
                             retVal(CL_SUCCESS) {
        // ctor
    }

    OfflineCompiler *pOfflineCompiler;
    int retVal;
};

} // namespace OCLRT
