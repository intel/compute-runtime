/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/queue_helpers.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(QueueHelpersTest, givenCommandQueueWithoutVirtualEventWhenReleaseQueueIsCalledThenCmdQInternalRefCountIsNotDecremented) {
    cl_int retVal = CL_SUCCESS;
    MockCommandQueue *cmdQ = new MockCommandQueue;
    EXPECT_EQ(1, cmdQ->getRefInternalCount());

    EXPECT_EQ(1, cmdQ->getRefInternalCount());

    cmdQ->incRefInternal();
    EXPECT_EQ(2, cmdQ->getRefInternalCount());

    releaseQueue(cmdQ, retVal);

    EXPECT_EQ(1, cmdQ->getRefInternalCount()); // NOLINT(clang-analyzer-cplusplus.NewDelete)
    cmdQ->decRefInternal();
}

TEST(QueueHelpersTest, givenPropertyListWithPropertyOfValueZeroWhenGettingPropertyValueThenCorrectValueIsReturned) {
    cl_queue_properties propertyName1 = 0xA;
    cl_queue_properties propertyName2 = 0xB;
    cl_queue_properties properties[] = {propertyName1, 0, propertyName2, 0, 0};
    int testedPropertyValues[] = {-1, 0, 1};
    for (auto property1Value : testedPropertyValues) {
        properties[1] = property1Value;
        for (auto property2Value : testedPropertyValues) {
            properties[3] = property2Value;

            EXPECT_EQ(property1Value, getCmdQueueProperties<int>(properties, propertyName1));
            EXPECT_EQ(property2Value, getCmdQueueProperties<int>(properties, propertyName2));
        }
    }
}

TEST(QueueHelpersTest, givenPropertiesWhenGettingPropertyValuesThenReturnCorrectFoundPropertyValue) {
    cl_queue_properties nonExistantProperty = 0xCC;
    cl_queue_properties properties[] = {
        0xAA,
        3,
        0xBB,
        0,
        0};

    bool foundProperty = false;
    EXPECT_EQ(properties[1], getCmdQueueProperties<cl_queue_properties>(properties, properties[0], &foundProperty));
    EXPECT_TRUE(foundProperty);

    foundProperty = false;
    EXPECT_EQ(properties[3], getCmdQueueProperties<cl_queue_properties>(properties, properties[2], &foundProperty));
    EXPECT_TRUE(foundProperty);

    foundProperty = false;
    EXPECT_EQ(0u, getCmdQueueProperties<cl_queue_properties>(properties, nonExistantProperty, &foundProperty));
    EXPECT_FALSE(foundProperty);
}
