/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub_tests/command_stream/aub_mem_dump_tests.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"

namespace NEO {

using Gen12LPAubMemDumpTests = Test<DeviceFixture>;

GEN12LPTEST_F(Gen12LPAubMemDumpTests, simpleCCS) {
    setupAUB<FamilyType>(pDevice, aub_stream::ENGINE_CCS);
}

} // namespace NEO
