/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "cl_api_tests.h"
#include "runtime/context/context.h"
#include "unit_tests/fixtures/device_host_queue_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"

using namespace OCLRT;
namespace DeviceHostQueue {
typedef ::testing::Types<CommandQueue, DeviceQueue> QueueTypes;

template <typename T>
class clReleaseCommandQueueTypeTests : public DeviceHostQueueFixture<T> {};

TYPED_TEST_CASE(clReleaseCommandQueueTypeTests, QueueTypes);

TYPED_TEST(clReleaseCommandQueueTypeTests, returnsSucess) {
    using BaseType = typename TypeParam::BaseType;

    auto queue = this->createClQueue();
    ASSERT_EQ(CL_SUCCESS, this->retVal);
    auto qObject = castToObject<TypeParam>(static_cast<BaseType *>(queue));
    ASSERT_NE(qObject, nullptr);

    this->retVal = clReleaseCommandQueue(queue);
    EXPECT_EQ(CL_SUCCESS, this->retVal);
}

TEST(clReleaseCommandQueueTypeTests, nullCommandQueueReturnsError) {
    auto retVal = clReleaseCommandQueue(nullptr);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, retVal);
}
} // namespace DeviceHostQueue

namespace ULT {

typedef api_tests clReleaseCommandQueueTests;

TEST_F(clReleaseCommandQueueTests, givenBlockedEnqueueWithOutputEventStoredAsVirtualEventWhenReleaseCommandQueueIsCalledThenInternalRefCountIsDecrementedAndQueueDeleted) {
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    Device *device = (Device *)devices[0];
    MockKernelWithInternals kernelInternals(*device, pContext);
    Kernel *kernel = kernelInternals.mockKernel;

    cmdQ = clCreateCommandQueue(pContext, devices[0], properties, &retVal);

    ASSERT_NE(nullptr, cmdQ);
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t offset[] = {0, 0, 0};
    size_t gws[] = {1, 1, 1};

    cl_int retVal = CL_SUCCESS;
    cl_int success = CL_SUCCESS;
    cl_event event = clCreateUserEvent(pContext, &retVal);

    cl_event eventOut = nullptr;

    EXPECT_EQ(success, retVal);

    retVal = clEnqueueNDRangeKernel(cmdQ, kernel, 1, offset, gws, nullptr, 1, &event, &eventOut);

    EXPECT_EQ(success, retVal);
    EXPECT_NE(nullptr, eventOut);

    clSetUserEventStatus(event, CL_COMPLETE);

    clReleaseEvent(event);
    clReleaseEvent(eventOut);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(success, retVal);
}
} // namespace ULT
