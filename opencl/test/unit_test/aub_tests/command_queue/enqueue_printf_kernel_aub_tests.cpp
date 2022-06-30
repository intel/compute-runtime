/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_kernel_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"

#include "command_enqueue_fixture.h"

using namespace NEO;

class AUBPrintfKernelFixture : public AUBFixture,
                               public HelloWorldKernelFixture,
                               public testing::Test {
  public:
    using HelloWorldKernelFixture::SetUp;

    void SetUp() override {
        AUBFixture::SetUp(nullptr);
        ASSERT_NE(nullptr, device.get());
        HelloWorldKernelFixture::SetUp(device.get(), programFile, kernelName);
    }
    void TearDown() override {
        if (IsSkipped()) {
            return;
        }
        HelloWorldKernelFixture::TearDown();
        AUBFixture::TearDown();
    }
    const char *programFile = "printf";
    const char *kernelName = "test_printf_number";
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
    pCmdQ->finish();
}