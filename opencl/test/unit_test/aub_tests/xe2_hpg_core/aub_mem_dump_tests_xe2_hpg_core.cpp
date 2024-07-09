/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/aub_tests/command_stream/aub_mem_dump_tests.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using Xe2HpgCoreAubMemDumpTests = Test<NEO::ClDeviceFixture>;

XE2_HPG_CORETEST_F(Xe2HpgCoreAubMemDumpTests, GivenCcsThenExpectationsAreMet) {
    setupAUB<FamilyType>(pDevice, aub_stream::ENGINE_CCS);
}
