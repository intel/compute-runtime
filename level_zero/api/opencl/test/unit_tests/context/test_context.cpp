/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/context/leo_context.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(GetContextPropertiesTests, givenNullPropertiesWhenGetContextPropertiesThenReturnsZero) {
    bool found = true;
    auto result = Context::getContextProperties<intptr_t>(nullptr, CL_CONTEXT_PLATFORM, &found);
    EXPECT_EQ(0, result);
    EXPECT_FALSE(found);
}

TEST(GetContextPropertiesTests, givenPropertiesWithMatchingPropertyWhenGetContextPropertiesThenReturnsValueAndFoundIsTrue) {
    intptr_t platformValue = 0x12345678;
    cl_context_properties props[] = {CL_CONTEXT_PLATFORM, platformValue, 0};
    bool found = false;
    auto result = Context::getContextProperties<intptr_t>(props, CL_CONTEXT_PLATFORM, &found);
    EXPECT_EQ(platformValue, result);
    EXPECT_TRUE(found);
}

TEST(GetContextPropertiesTests, givenPropertiesWithoutMatchingPropertyWhenGetContextPropertiesThenFoundIsFalse) {
    intptr_t platformValue = 0x12345678;
    cl_context_properties props[] = {CL_CONTEXT_PLATFORM, platformValue, 0};
    bool found = true;
    auto result = Context::getContextProperties<intptr_t>(props, CL_CONTEXT_INTEROP_USER_SYNC, &found);
    EXPECT_EQ(0, result);
    EXPECT_FALSE(found);
}

TEST(GetContextPropertiesTests, givenEmptyPropertiesWhenGetContextPropertiesThenFoundIsFalse) {
    cl_context_properties props[] = {0};
    bool found = true;
    auto result = Context::getContextProperties<intptr_t>(props, CL_CONTEXT_PLATFORM, &found);
    EXPECT_EQ(0, result);
    EXPECT_FALSE(found);
}

TEST(GetContextPropertiesTests, givenMultiplePropertiesWhenGetContextPropertiesThenReturnsCorrectOne) {
    cl_context_properties props[] = {
        CL_CONTEXT_INTEROP_USER_SYNC, CL_TRUE,
        CL_CONTEXT_PLATFORM, 0xABCDEF,
        0};
    auto interopSync = Context::getContextProperties<cl_bool>(props, CL_CONTEXT_INTEROP_USER_SYNC);
    auto platform = Context::getContextProperties<intptr_t>(props, CL_CONTEXT_PLATFORM);
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), interopSync);
    EXPECT_EQ(static_cast<intptr_t>(0xABCDEF), platform);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
