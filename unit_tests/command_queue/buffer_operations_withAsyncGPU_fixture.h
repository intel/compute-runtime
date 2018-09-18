/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "buffer_operations_fixture.h"
#include "runtime/helpers/options.h"

namespace OCLRT {
struct AsyncGPUoperations : public EnqueueWriteBufferTypeTest {
    void SetUp() override {
        storeInitHWTag = initialHardwareTag;
        initialHardwareTag = 0;
        EnqueueWriteBufferTypeTest::SetUp();
    }

    void TearDown() override {
        initialHardwareTag = storeInitHWTag;
        EnqueueWriteBufferTypeTest::TearDown();
    }

  protected:
    int storeInitHWTag;
};
} // namespace OCLRT
