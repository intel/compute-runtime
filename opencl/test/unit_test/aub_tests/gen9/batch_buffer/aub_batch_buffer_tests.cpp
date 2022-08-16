/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aub_batch_buffer_tests.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using AubBatchBufferTests = Test<NEO::ClDeviceFixture>;

static constexpr auto gpuBatchBufferAddr = 0x800400001000ull; // 48-bit GPU address

GEN9TEST_F(AubBatchBufferTests, givenSimpleRCSWithBatchBufferWhenItHasMSBSetInGpuAddressThenAUBShouldBeSetupSuccessfully) {
    setupAUBWithBatchBuffer<FamilyType>(pDevice, aub_stream::ENGINE_RCS, gpuBatchBufferAddr);
}
