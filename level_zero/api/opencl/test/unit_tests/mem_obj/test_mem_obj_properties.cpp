/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/mem_obj/leo_mem_obj.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(GetMemObjPropertiesTests, givenNullPropertiesWhenGetMemObjPropertiesThenReturnsZero) {
    auto result = MemObj::getMemObjProperties<cl_mem_flags>(nullptr, CL_MEM_FLAGS);
    EXPECT_EQ(0u, result);
}

TEST(GetMemObjPropertiesTests, givenNullPropertiesWithFoundFlagWhenGetMemObjPropertiesThenFoundIsFalse) {
    bool found = true;
    MemObj::getMemObjProperties<cl_mem_flags>(nullptr, CL_MEM_FLAGS, &found);
    EXPECT_FALSE(found);
}

TEST(GetMemObjPropertiesTests, givenPropertiesWithMatchingPropertyWhenGetMemObjPropertiesThenReturnsValueAndFoundIsTrue) {
    cl_mem_properties props[] = {CL_MEM_FLAGS, CL_MEM_READ_WRITE, 0};
    bool found = false;
    auto result = MemObj::getMemObjProperties<cl_mem_flags>(props, CL_MEM_FLAGS, &found);
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_WRITE), result);
    EXPECT_TRUE(found);
}

TEST(GetMemObjPropertiesTests, givenPropertiesWithoutMatchingPropertyWhenGetMemObjPropertiesThenFoundIsFalse) {
    cl_mem_properties props[] = {CL_MEM_FLAGS, CL_MEM_READ_WRITE, 0};
    bool found = true;
    auto result = MemObj::getMemObjProperties<cl_mem_flags_intel>(props, CL_MEM_FLAGS_INTEL, &found);
    EXPECT_EQ(0u, result);
    EXPECT_FALSE(found);
}

TEST(GetMemObjPropertiesTests, givenEmptyPropertiesWhenGetMemObjPropertiesThenReturnsZero) {
    cl_mem_properties props[] = {0};
    auto result = MemObj::getMemObjProperties<cl_mem_flags>(props, CL_MEM_FLAGS);
    EXPECT_EQ(0u, result);
}

TEST(GetMemObjPropertiesTests, givenMultiplePropertiesWhenGetMemObjPropertiesThenReturnsCorrectOne) {
    cl_mem_properties props[] = {
        CL_MEM_FLAGS, CL_MEM_READ_ONLY,
        CL_MEM_FLAGS_INTEL, CL_MEM_LOCALLY_UNCACHED_RESOURCE_INTEL,
        0};
    auto flags = MemObj::getMemObjProperties<cl_mem_flags>(props, CL_MEM_FLAGS);
    auto flagsIntel = MemObj::getMemObjProperties<cl_mem_flags_intel>(props, CL_MEM_FLAGS_INTEL);
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_ONLY), flags);
    EXPECT_EQ(static_cast<cl_mem_flags_intel>(CL_MEM_LOCALLY_UNCACHED_RESOURCE_INTEL), flagsIntel);
}

TEST(MapInfoTests, givenDefaultConstructionWhenCreateMapInfoThenAllFieldsAreZero) {
    MapInfo info;
    EXPECT_EQ(nullptr, info.ptr);
    EXPECT_EQ(0u, info.ptrLength);
    EXPECT_FALSE(info.readOnly);
}

TEST(MapInfoTests, givenParameterizedConstructionWhenCreateMapInfoThenFieldsAreSet) {
    void *ptr = reinterpret_cast<void *>(0x1000);
    MemObjSizeArray size = {8, 4, 2};
    MemObjOffsetArray offset = {1, 2, 3};
    MapInfo info(ptr, 64, size, offset);
    EXPECT_EQ(ptr, info.ptr);
    EXPECT_EQ(64u, info.ptrLength);
    EXPECT_EQ(size, info.size);
    EXPECT_EQ(offset, info.offset);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
