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

#include "runtime/context/context.h"
#include "unit_tests/fixtures/device_host_queue_fixture.h"

using namespace OCLRT;
namespace DeviceHostQueue {
typedef ::testing::Types<CommandQueue, DeviceQueue> QueueTypes;

template <typename T>
class clRetainReleaseCommandQueueTests : public DeviceHostQueueFixture<T> {};

TYPED_TEST_CASE(clRetainReleaseCommandQueueTests, QueueTypes);

TYPED_TEST(clRetainReleaseCommandQueueTests, retain_release) {
    using BaseType = typename TypeParam::BaseType;

    auto queue = this->createClQueue();
    ASSERT_EQ(CL_SUCCESS, this->retVal);
    auto qObject = castToObject<TypeParam>(static_cast<BaseType *>(queue));
    ASSERT_NE(qObject, nullptr);

    cl_uint refCount;
    this->retVal = clGetCommandQueueInfo(queue, CL_QUEUE_REFERENCE_COUNT,
                                         sizeof(cl_uint), &refCount, NULL);
    EXPECT_EQ(CL_SUCCESS, this->retVal);
    EXPECT_EQ(1u, refCount);

    this->retVal = clRetainCommandQueue(queue);
    EXPECT_EQ(CL_SUCCESS, this->retVal);

    this->retVal = clGetCommandQueueInfo(queue, CL_QUEUE_REFERENCE_COUNT,
                                         sizeof(cl_uint), &refCount, NULL);
    EXPECT_EQ(CL_SUCCESS, this->retVal);
    EXPECT_EQ(2u, refCount);

    this->retVal = clReleaseCommandQueue(queue);
    EXPECT_EQ(CL_SUCCESS, this->retVal);

    this->retVal = clGetCommandQueueInfo(queue, CL_QUEUE_REFERENCE_COUNT,
                                         sizeof(cl_uint), &refCount, NULL);
    EXPECT_EQ(CL_SUCCESS, this->retVal);
    EXPECT_EQ(1u, refCount);

    this->retVal = clReleaseCommandQueue(queue);
    EXPECT_EQ(CL_SUCCESS, this->retVal);
}
} // namespace DeviceHostQueue
