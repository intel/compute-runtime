/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/helpers/options.h"

#include "buffer_operations_fixture.h"

namespace NEO {
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
} // namespace NEO
