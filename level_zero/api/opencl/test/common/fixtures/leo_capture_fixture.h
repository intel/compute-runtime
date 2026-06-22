/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/api/opencl/test/common/fixtures/capturing_command_list.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {

class CommandQueue;
class Context;

namespace ult {

struct LeoCaptureFixture : public OclFixture {
    void setUp();
    void tearDown();

    CommandQueue *getCommandQueue() const { return commandQueue; }

    cl_mem createBuffer(size_t size, cl_mem_flags flags = CL_MEM_READ_WRITE);

    ClDevice *clDevice = nullptr;
    cl_context clContext = nullptr;
    Context *context = nullptr;
    CapturingCommandList capturingCmdList{};
    CommandQueue *commandQueue = nullptr;
};

} // namespace ult
} // namespace LEO
} // namespace NEO
