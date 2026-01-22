/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/mem_obj.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_kernel_fixture.h"

using namespace NEO;

class AUBPrintfKernelFixture : public AUBFixture,
                               public HelloWorldKernelFixture,
                               public testing::Test {
  public:
    using HelloWorldKernelFixture::setUp;

    void SetUp() override {
        AUBFixture::setUp(nullptr);
        ASSERT_NE(nullptr, device);
        HelloWorldKernelFixture::setUp(device, programFile, kernelName);
        initialized = true;
    }
    void TearDown() override {
        if (!initialized) {
            return;
        }
        HelloWorldKernelFixture::tearDown();
        AUBFixture::tearDown();
    }
    const char *programFile = "simple_kernels";
    const char *kernelName = "test_printf_number";
    bool initialized = false;
};

HWTEST_F(AUBPrintfKernelFixture, GivenPrintfKernelThenEnqueuingSucceeds) {
    ASSERT_NE(nullptr, pKernel);

    size_t offset[3] = {0, 0, 0};
    size_t gws[3] = {4, 1, 1};
    size_t lws[3] = {4, 1, 1};

    std::unique_ptr<Buffer> buffer(BufferHelper<BufferUseHostPtr<>>::create(pContext));
    const uint32_t number = 4;
    *(reinterpret_cast<uint32_t *>(buffer->getCpuAddressForMemoryTransfer())) = number;

    cl_mem bufferMem = buffer.get();

    pKernel->setArg(
        0,
        sizeof(cl_mem),
        &bufferMem);

    pCmdQ->enqueueKernel(pKernel, 1, offset, gws, lws, 0, 0, 0);
    pCmdQ->finish(false);
}
