/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_stream/command_stream_receiver.h"
#include "unit_tests/command_stream/command_stream_fixture.h"
#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/indirect_heap/indirect_heap_fixture.h"
#include "unit_tests/fixtures/simple_arg_kernel_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"

namespace OCLRT {

struct SimpleArgFixtureFactory {
    typedef OCLRT::IndirectHeapFixture IndirectHeapFixture;
    typedef OCLRT::CommandStreamFixture CommandStreamFixture;
    typedef OCLRT::CommandQueueHwFixture CommandQueueFixture;
    typedef OCLRT::SimpleArgKernelFixture KernelFixture;
};
} // namespace OCLRT
