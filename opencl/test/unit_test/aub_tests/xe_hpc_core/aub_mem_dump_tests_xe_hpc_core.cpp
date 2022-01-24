/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub_tests/command_stream/aub_mem_dump_tests.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using XeHpcCoreAubMemDumpTests = Test<NEO::ClDeviceFixture>;

XE_HPC_CORETEST_F(XeHpcCoreAubMemDumpTests, GivenCcsThenExpectationsAreMet) {
    setupAUB<FamilyType>(pDevice, aub_stream::ENGINE_CCS);
}
