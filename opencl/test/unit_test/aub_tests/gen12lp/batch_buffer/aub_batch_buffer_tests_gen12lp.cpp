/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aub_batch_buffer_tests_gen12lp.h"

#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/event/event.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using Gen12LPAubBatchBufferTests = Test<NEO::ClDeviceFixture>;
using Gen12LPTimestampTests = Test<HelloWorldFixture<AUBHelloWorldFixtureFactory>>;

static constexpr auto gpuBatchBufferAddr = 0x400400001000; // 47-bit GPU address

GEN12LPTEST_F(Gen12LPAubBatchBufferTests, givenSimpleRCSWithBatchBufferWhenItHasMSBSetInGpuAddressThenAUBShouldBeSetupSuccessfully) {
    setupAUBWithBatchBuffer<FamilyType>(pDevice, aub_stream::ENGINE_RCS, gpuBatchBufferAddr);
}

GEN12LPTEST_F(Gen12LPAubBatchBufferTests, givenSimpleCCSWithBatchBufferWhenItHasMSBSetInGpuAddressThenAUBShouldBeSetupSuccessfully) {
    setupAUBWithBatchBuffer<FamilyType>(pDevice, aub_stream::ENGINE_CCS, gpuBatchBufferAddr);
}
