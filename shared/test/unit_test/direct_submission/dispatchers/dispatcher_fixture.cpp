/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/direct_submission/dispatchers/dispatcher_fixture.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"

using namespace NEO;

void DispatcherFixture::setUp() {
    DeviceFixture::setUp();

    bufferAllocation = alignedMalloc(MemoryConstants::pageSize, MemoryConstants::pageSize);
    cmdBuffer.replaceBuffer(bufferAllocation, MemoryConstants::pageSize);
}

void DispatcherFixture::tearDown() {
    alignedFree(bufferAllocation);

    DeviceFixture::tearDown();
}
