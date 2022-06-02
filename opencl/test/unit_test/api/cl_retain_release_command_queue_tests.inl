/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/unit_test_helper.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;
namespace ULT {

class ClRetainReleaseCommandQueueTests : public ApiFixture<>,
                                         public ::testing::Test {
  public:
    void SetUp() override {
        ApiFixture::SetUp();
    }
    void TearDown() override {
        ApiFixture::TearDown();
    }

    cl_command_queue createClQueue() {
        return clCreateCommandQueueWithProperties(pContext, testedClDevice, noProperties, &retVal);
    }

  protected:
    cl_queue_properties noProperties[5] = {0};
};

TEST_F(ClRetainReleaseCommandQueueTests, GivenValidCommandQueueWhenRetainingAndReleasingThenReferenceCountIsUpdatedCorrectly) {

    auto queue = this->createClQueue();
    ASSERT_EQ(CL_SUCCESS, this->retVal);
    auto qObject = castToObject<CommandQueue>(queue);
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
} // namespace ULT
