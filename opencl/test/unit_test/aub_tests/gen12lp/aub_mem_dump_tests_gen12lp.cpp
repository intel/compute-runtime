/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub_tests/command_stream/aub_mem_dump_tests.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

namespace NEO {

using Gen12LPAubMemDumpTests = Test<ClDeviceFixture>;

GEN12LPTEST_F(Gen12LPAubMemDumpTests, GivenCcsThenExpectationsAreMet) {
    setupAUB<FamilyType>(pDevice, aub_stream::ENGINE_CCS);
}

} // namespace NEO
