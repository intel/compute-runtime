/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/buffer_fixture.h"

#include "unit_tests/mocks/mock_context.h"

using OCLRT::Context;

// clang-format off
static char bufferMemory[] = {
    0x00, 0x10, 0x20, 0x30,
    0x01, 0x11, 0x21, 0x31,
    0x02, 0x12, 0x22, 0x32,
    0x03, 0x13, 0x23, 0x33,
};
// clang-format on

void *BufferDefaults::hostPtr = bufferMemory;
const size_t BufferDefaults::sizeInBytes = sizeof(bufferMemory);
Context *BufferDefaults::context = nullptr;
