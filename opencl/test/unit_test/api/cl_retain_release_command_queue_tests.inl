/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/unit_test_helper.h"

#include "opencl/source/context/context.h"
#include "opencl/test/unit_test/fixtures/device_host_queue_fixture.h"

using namespace NEO;
namespace DeviceHostQueue {
typedef ::testing::Types<CommandQueue, DeviceQueue> QueueTypes;

template <typename T>
class clRetainReleaseCommandQueueTests : public DeviceHostQueueFixture<T> {};

TYPED_TEST_CASE(clRetainReleaseCommandQueueTests, QueueTypes);

TYPED_TEST(clRetainReleaseCommandQueueTests, GivenValidCommandQueueWhenRetainingAndReleasingThenReferenceCountIsUpdatedCorrectly) {
    if (std::is_same<TypeParam, DeviceQueue>::value && !this->pDevice->getHardwareInfo().capabilityTable.supportsDeviceEnqueue) {
        return;
    }

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
