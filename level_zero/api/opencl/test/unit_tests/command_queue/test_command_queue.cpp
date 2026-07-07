/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(GetCmdQueuePropertiesTests, givenNullPropertiesWhenGetCmdQueuePropertiesThenReturnsZero) {
    auto result = CommandQueue::getCmdQueueProperties<cl_command_queue_properties>(nullptr);
    EXPECT_EQ(0u, result);
}

TEST(GetCmdQueuePropertiesTests, givenNullPropertiesWithFoundFlagWhenGetCmdQueuePropertiesThenFoundIsFalse) {
    bool found = true;
    CommandQueue::getCmdQueueProperties<cl_command_queue_properties>(nullptr, CL_QUEUE_PROPERTIES, &found);
    EXPECT_FALSE(found);
}

TEST(GetCmdQueuePropertiesTests, givenProfilingEnabledWhenGetCmdQueuePropertiesThenReturnsProfilingFlag) {
    cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    auto result = CommandQueue::getCmdQueueProperties<cl_command_queue_properties>(props);
    EXPECT_EQ(static_cast<cl_command_queue_properties>(CL_QUEUE_PROFILING_ENABLE), result);
}

TEST(GetCmdQueuePropertiesTests, givenPropertiesWithoutSearchedPropertyWhenGetCmdQueuePropertiesThenFoundIsFalse) {
    cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_PROFILING_ENABLE, 0};
    bool found = true;
    auto result = CommandQueue::getCmdQueueProperties<cl_uint>(props, CL_QUEUE_FAMILY_INTEL, &found);
    EXPECT_FALSE(found);
    EXPECT_EQ(0u, result);
}

TEST(GetCmdQueuePropertiesTests, givenPropertiesWithSearchedPropertyWhenGetCmdQueuePropertiesThenFoundIsTrue) {
    cl_queue_properties props[] = {CL_QUEUE_FAMILY_INTEL, 2, CL_QUEUE_INDEX_INTEL, 3, 0};
    bool found = false;
    auto result = CommandQueue::getCmdQueueProperties<cl_uint>(props, CL_QUEUE_FAMILY_INTEL, &found);
    EXPECT_TRUE(found);
    EXPECT_EQ(2u, result);
}

TEST(GetCmdQueuePropertiesTests, givenMultiplePropertiesWhenGetCmdQueuePropertiesForIndexThenReturnsCorrectValue) {
    cl_queue_properties props[] = {CL_QUEUE_FAMILY_INTEL, 5, CL_QUEUE_INDEX_INTEL, 7, 0};
    auto queueIndex = CommandQueue::getCmdQueueProperties<cl_uint>(props, CL_QUEUE_INDEX_INTEL);
    auto familyIndex = CommandQueue::getCmdQueueProperties<cl_uint>(props, CL_QUEUE_FAMILY_INTEL);
    EXPECT_EQ(7u, queueIndex);
    EXPECT_EQ(5u, familyIndex);
}

TEST(GetCmdQueuePropertiesTests, givenEmptyPropertiesWhenGetCmdQueuePropertiesThenReturnsZero) {
    cl_queue_properties props[] = {0};
    auto result = CommandQueue::getCmdQueueProperties<cl_command_queue_properties>(props);
    EXPECT_EQ(0u, result);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
