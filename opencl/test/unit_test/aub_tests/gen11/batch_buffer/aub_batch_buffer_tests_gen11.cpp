/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aub_batch_buffer_tests_gen11.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using Gen11AubBatchBufferTests = Test<NEO::ClDeviceFixture>;

static constexpr auto gpuBatchBufferAddr = 0x800400001000; // 48-bit GPU address

GEN11TEST_F(Gen11AubBatchBufferTests, givenSimpleRCSWithBatchBufferWhenItHasMSBSetInGpuAddressThenAUBShouldBeSetupSuccessfully) {
    setupAUBWithBatchBuffer<FamilyType>(pDevice, aub_stream::ENGINE_RCS, gpuBatchBufferAddr);
}
