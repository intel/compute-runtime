/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/mem_obj_helper.h"
#include "gtest/gtest.h"

using namespace OCLRT;

TEST(MemObjHelper, givenValidMemFlagsForSubBufferWhenFlagsAreCheckedThenTrueIsReturned) {
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY |
                         CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS;

    EXPECT_TRUE(MemObjHelper::checkMemFlagsForSubBuffer(flags));
}

TEST(MemObjHelper, givenInvalidMemFlagsForSubBufferWhenFlagsAreCheckedThenTrueIsReturned) {
    cl_mem_flags flags = CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR;

    EXPECT_FALSE(MemObjHelper::checkMemFlagsForSubBuffer(flags));
}

TEST(MemObjHelper, givenNullPropertiesWhenParsingMemoryPropertiesThenTrueIsReturned) {
    MemoryProperties propertiesStruct;
    EXPECT_TRUE(MemObjHelper::parseMemoryProperties(nullptr, propertiesStruct));
}

TEST(MemObjHelper, givenEmptyPropertiesWhenParsingMemoryPropertiesThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {0};

    MemoryProperties propertiesStruct;
    EXPECT_TRUE(MemObjHelper::parseMemoryProperties(properties, propertiesStruct));
}

TEST(MemObjHelper, givenValidPropertiesWhenParsingMemoryPropertiesThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR |
            CL_MEM_USE_HOST_PTR | CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS,
        CL_MEM_FLAGS_INTEL,
        CL_MEM_LOCALLY_UNCACHED_RESOURCE,
        0};

    MemoryProperties propertiesStruct;
    EXPECT_TRUE(MemObjHelper::parseMemoryProperties(properties, propertiesStruct));
}

TEST(MemObjHelper, givenInvalidPropertiesWhenParsingMemoryPropertiesThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        (1 << 30), CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR,
        0};

    MemoryProperties propertiesStruct;
    EXPECT_FALSE(MemObjHelper::parseMemoryProperties(properties, propertiesStruct));
}

TEST(MemObjHelper, givenValidPropertiesWhenValidatingMemoryPropertiesThenTrueIsReturned) {
    MemoryProperties properties;
    EXPECT_TRUE(MemObjHelper::validateMemoryProperties(properties));

    properties.flags = CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR | CL_MEM_HOST_NO_ACCESS;
    EXPECT_TRUE(MemObjHelper::validateMemoryProperties(properties));

    properties.flags = CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR | CL_MEM_HOST_WRITE_ONLY;
    EXPECT_TRUE(MemObjHelper::validateMemoryProperties(properties));

    properties.flags = CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS;
    EXPECT_TRUE(MemObjHelper::validateMemoryProperties(properties));

    properties.flags_intel = CL_MEM_LOCALLY_UNCACHED_RESOURCE;
    EXPECT_TRUE(MemObjHelper::validateMemoryProperties(properties));

    properties.flags = 0;
    EXPECT_TRUE(MemObjHelper::validateMemoryProperties(properties));
}

TEST(MemObjHelper, givenInvalidPropertiesWhenValidatingMemoryPropertiesThenFalseIsReturned) {
    MemoryProperties properties;
    properties.flags = (1 << 31);
    EXPECT_FALSE(MemObjHelper::validateMemoryProperties(properties));

    properties.flags_intel = (1 << 31);
    EXPECT_FALSE(MemObjHelper::validateMemoryProperties(properties));

    properties.flags = 0;
    EXPECT_FALSE(MemObjHelper::validateMemoryProperties(properties));
}
