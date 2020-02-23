/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/command_stream_receiver.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/command_stream/command_stream_fixture.h"
#include "opencl/test/unit_test/fixtures/memory_management_fixture.h"
#include "opencl/test/unit_test/fixtures/simple_arg_kernel_fixture.h"

namespace NEO {

struct SimpleArgFixtureFactory {
    typedef NEO::CommandStreamFixture CommandStreamFixture;
    typedef NEO::CommandQueueHwFixture CommandQueueFixture;
    typedef NEO::SimpleArgKernelFixture KernelFixture;
};
} // namespace NEO
