/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/mem_properties_parser_helper.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "CL/cl_ext_intel.h"
#include "gtest/gtest.h"

using namespace NEO;

struct MemoryPropertiesParserHelperTests : ::testing::Test {
    MockContext context;
    MemoryProperties memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
};

TEST_F(MemoryPropertiesParserHelperTests, givenNullPropertiesWhenParsingMemoryPropertiesThenTrueIsReturned) {
    EXPECT_TRUE(MemoryPropertiesParserHelper::parseMemoryProperties(nullptr, memoryProperties, flags, flagsIntel, allocflags,
                                                                    MemoryPropertiesParserHelper::MemoryPropertiesParserHelper::ObjType::UNKNOWN, context));
}

TEST_F(MemoryPropertiesParserHelperTests, givenEmptyPropertiesWhenParsingMemoryPropertiesThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {0};

    EXPECT_TRUE(MemoryPropertiesParserHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                    MemoryPropertiesParserHelper::MemoryPropertiesParserHelper::ObjType::UNKNOWN, context));
    EXPECT_TRUE(MemoryPropertiesParserHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                    MemoryPropertiesParserHelper::MemoryPropertiesParserHelper::ObjType::BUFFER, context));
    EXPECT_TRUE(MemoryPropertiesParserHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                    MemoryPropertiesParserHelper::MemoryPropertiesParserHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesParserHelperTests, givenValidPropertiesWhenParsingMemoryPropertiesThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR |
            CL_MEM_USE_HOST_PTR | CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS,
        CL_MEM_FLAGS_INTEL,
        CL_MEM_LOCALLY_UNCACHED_RESOURCE | CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE,
        CL_MEM_ALLOC_FLAGS_INTEL,
        CL_MEM_ALLOC_WRITE_COMBINED_INTEL, CL_MEM_ALLOC_DEFAULT_INTEL,
        0};

    EXPECT_TRUE(MemoryPropertiesParserHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                    MemoryPropertiesParserHelper::ObjType::UNKNOWN, context));
}

TEST_F(MemoryPropertiesParserHelperTests, givenValidPropertiesWhenParsingMemoryPropertiesForBufferThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForBuffer,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForBufferIntel,
        0};

    EXPECT_TRUE(MemoryPropertiesParserHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                    MemoryPropertiesParserHelper::ObjType::BUFFER, context));
}

TEST_F(MemoryPropertiesParserHelperTests, givenValidPropertiesWhenParsingMemoryPropertiesForImageThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForImage,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForImageIntel,
        0};

    EXPECT_TRUE(MemoryPropertiesParserHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                    MemoryPropertiesParserHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesParserHelperTests, givenInvalidPropertiesWhenParsingMemoryPropertiesThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        (1 << 30), CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR,
        0};

    EXPECT_FALSE(MemoryPropertiesParserHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                     MemoryPropertiesParserHelper::ObjType::UNKNOWN, context));
    EXPECT_FALSE(MemoryPropertiesParserHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                     MemoryPropertiesParserHelper::ObjType::BUFFER, context));
    EXPECT_FALSE(MemoryPropertiesParserHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                     MemoryPropertiesParserHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesParserHelperTests, givenInvalidPropertiesWhenParsingMemoryPropertiesForImageThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForBuffer,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForBufferIntel,
        0};

    EXPECT_FALSE(MemoryPropertiesParserHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                     MemoryPropertiesParserHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesParserHelperTests, givenInvalidFlagsWhenParsingMemoryPropertiesForImageThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        (1 << 30),
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForImageIntel,
        0};

    EXPECT_FALSE(MemoryPropertiesParserHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                     MemoryPropertiesParserHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesParserHelperTests, givenInvalidFlagsIntelWhenParsingMemoryPropertiesForImageThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForImage,
        CL_MEM_FLAGS_INTEL,
        (1 << 30),
        0};

    EXPECT_FALSE(MemoryPropertiesParserHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                     MemoryPropertiesParserHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesParserHelperTests, givenInvalidPropertiesWhenParsingMemoryPropertiesForBufferThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForImage,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForImageIntel,
        0};

    EXPECT_FALSE(MemoryPropertiesParserHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                     MemoryPropertiesParserHelper::ObjType::BUFFER, context));
}

TEST_F(MemoryPropertiesParserHelperTests, givenInvalidFlagsWhenParsingMemoryPropertiesForBufferThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        (1 << 30),
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForBufferIntel,
        0};

    EXPECT_FALSE(MemoryPropertiesParserHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                     MemoryPropertiesParserHelper::ObjType::BUFFER, context));
}

TEST_F(MemoryPropertiesParserHelperTests, givenInvalidFlagsIntelWhenParsingMemoryPropertiesForBufferThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForBuffer,
        CL_MEM_FLAGS_INTEL,
        (1 << 30),
        0};

    EXPECT_FALSE(MemoryPropertiesParserHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                     MemoryPropertiesParserHelper::ObjType::BUFFER, context));
}

TEST_F(MemoryPropertiesParserHelperTests, givenDifferentParametersWhenCallingFillCachePolicyInPropertiesThenFlushL3FlagsAreCorrectlySet) {
    AllocationProperties allocationProperties{0, 0, GraphicsAllocation::AllocationType::BUFFER};

    for (auto uncached : ::testing::Bool()) {
        for (auto readOnly : ::testing::Bool()) {
            for (auto deviceOnlyVisibilty : ::testing::Bool()) {
                if (uncached || readOnly || deviceOnlyVisibilty) {
                    allocationProperties.flags.flushL3RequiredForRead = true;
                    allocationProperties.flags.flushL3RequiredForWrite = true;
                    MemoryPropertiesParserHelper::fillCachePolicyInProperties(allocationProperties, uncached, readOnly, deviceOnlyVisibilty);
                    EXPECT_FALSE(allocationProperties.flags.flushL3RequiredForRead);
                    EXPECT_FALSE(allocationProperties.flags.flushL3RequiredForWrite);
                } else {
                    allocationProperties.flags.flushL3RequiredForRead = false;
                    allocationProperties.flags.flushL3RequiredForWrite = false;
                    MemoryPropertiesParserHelper::fillCachePolicyInProperties(allocationProperties, uncached, readOnly, deviceOnlyVisibilty);
                    EXPECT_TRUE(allocationProperties.flags.flushL3RequiredForRead);
                    EXPECT_TRUE(allocationProperties.flags.flushL3RequiredForWrite);
                }
            }
        }
    }
}
