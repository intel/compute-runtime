/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/queue_helpers.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(QueueHelpersTest, givenCommandQueueWithoutVirtualEventWhenReleaseQueueIsCalledThenCmdQInternalRefCountIsNotDecremented) {
    cl_int retVal = CL_SUCCESS;
    MockCommandQueue *cmdQ = new MockCommandQueue;
    EXPECT_EQ(1, cmdQ->getRefInternalCount());

    EXPECT_EQ(1, cmdQ->getRefInternalCount());

    cmdQ->incRefInternal();
    EXPECT_EQ(2, cmdQ->getRefInternalCount());

    releaseQueue<CommandQueue>(cmdQ, retVal);

    EXPECT_EQ(1, cmdQ->getRefInternalCount());
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
