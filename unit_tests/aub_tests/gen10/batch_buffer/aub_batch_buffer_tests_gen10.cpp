/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aub_batch_buffer_tests_gen10.h"

#include "unit_tests/fixtures/device_fixture.h"

using Gen10AubBatchBufferTests = Test<NEO::DeviceFixture>;

static constexpr auto gpuBatchBufferAddr = 0x800400001000; // 48-bit GPU address

GEN10TEST_F(Gen10AubBatchBufferTests, givenSimpleRCSWithBatchBufferWhenItHasMSBSetInGpuAddressThenAUBShouldBeSetupSuccessfully) {
    setupAUBWithBatchBuffer<FamilyType>(pDevice, NEO::EngineType::ENGINE_RCS, gpuBatchBufferAddr);
}
