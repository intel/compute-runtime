/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/mem_properties_parser_helper.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"

#include "CL/cl_ext_intel.h"
#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryPropertiesParser, givenNullPropertiesWhenParsingMemoryPropertiesThenTrueIsReturned) {
    MemoryPropertiesFlags memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;

    EXPECT_TRUE(MemoryPropertiesParser::parseMemoryProperties(nullptr, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::MemoryPropertiesParser::ObjType::UNKNOWN));
}

TEST(MemoryPropertiesParser, givenEmptyPropertiesWhenParsingMemoryPropertiesThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {0};

    MemoryPropertiesFlags memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;

    EXPECT_TRUE(MemoryPropertiesParser::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::MemoryPropertiesParser::ObjType::UNKNOWN));
    EXPECT_TRUE(MemoryPropertiesParser::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::MemoryPropertiesParser::ObjType::BUFFER));
    EXPECT_TRUE(MemoryPropertiesParser::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::MemoryPropertiesParser::ObjType::IMAGE));
}

TEST(MemoryPropertiesParser, givenValidPropertiesWhenParsingMemoryPropertiesThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR |
            CL_MEM_USE_HOST_PTR | CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS,
        CL_MEM_FLAGS_INTEL,
        CL_MEM_LOCALLY_UNCACHED_RESOURCE | CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE,
        CL_MEM_ALLOC_FLAGS_INTEL,
        CL_MEM_ALLOC_WRITE_COMBINED_INTEL, CL_MEM_ALLOC_DEFAULT_INTEL,
        0};

    MemoryPropertiesFlags memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;

    EXPECT_TRUE(MemoryPropertiesParser::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::ObjType::UNKNOWN));
}

TEST(MemoryPropertiesParser, givenValidPropertiesWhenParsingMemoryPropertiesForBufferThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForBuffer,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForBufferIntel,
        0};

    MemoryPropertiesFlags memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;

    EXPECT_TRUE(MemoryPropertiesParser::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::ObjType::BUFFER));
}

TEST(MemoryPropertiesParser, givenValidPropertiesWhenParsingMemoryPropertiesForImageThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForImage,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForImageIntel,
        0};

    MemoryPropertiesFlags memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;

    EXPECT_TRUE(MemoryPropertiesParser::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::ObjType::IMAGE));
}

TEST(MemoryPropertiesParser, givenInvalidPropertiesWhenParsingMemoryPropertiesThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        (1 << 30), CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR,
        0};

    MemoryPropertiesFlags memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;

    EXPECT_FALSE(MemoryPropertiesParser::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::ObjType::UNKNOWN));
    EXPECT_FALSE(MemoryPropertiesParser::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::ObjType::BUFFER));
    EXPECT_FALSE(MemoryPropertiesParser::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::ObjType::IMAGE));
}

TEST(MemoryPropertiesParser, givenInvalidPropertiesWhenParsingMemoryPropertiesForImageThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForBuffer,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForBufferIntel,
        0};

    MemoryPropertiesFlags memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;

    EXPECT_FALSE(MemoryPropertiesParser::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::ObjType::IMAGE));
}

TEST(MemoryPropertiesParser, givenInvalidFlagsWhenParsingMemoryPropertiesForImageThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        (1 << 30),
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForImageIntel,
        0};

    MemoryPropertiesFlags memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;

    EXPECT_FALSE(MemoryPropertiesParser::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::ObjType::IMAGE));
}

TEST(MemoryPropertiesParser, givenInvalidFlagsIntelWhenParsingMemoryPropertiesForImageThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForImage,
        CL_MEM_FLAGS_INTEL,
        (1 << 30),
        0};

    MemoryPropertiesFlags memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;

    EXPECT_FALSE(MemoryPropertiesParser::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::ObjType::IMAGE));
}

TEST(MemoryPropertiesParser, givenInvalidPropertiesWhenParsingMemoryPropertiesForBufferThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForImage,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForImageIntel,
        0};

    MemoryPropertiesFlags memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;

    EXPECT_FALSE(MemoryPropertiesParser::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::ObjType::BUFFER));
}

TEST(MemoryPropertiesParser, givenInvalidFlagsWhenParsingMemoryPropertiesForBufferThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        (1 << 30),
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForBufferIntel,
        0};

    MemoryPropertiesFlags memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;

    EXPECT_FALSE(MemoryPropertiesParser::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::ObjType::BUFFER));
}

TEST(MemoryPropertiesParser, givenInvalidFlagsIntelWhenParsingMemoryPropertiesForBufferThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForBuffer,
        CL_MEM_FLAGS_INTEL,
        (1 << 30),
        0};

    MemoryPropertiesFlags memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;

    EXPECT_FALSE(MemoryPropertiesParser::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags, MemoryPropertiesParser::ObjType::BUFFER));
}

TEST(MemoryPropertiesParser, givenDifferentParametersWhenCallingFillCachePolicyInPropertiesThenFlushL3FlagsAreCorrectlySet) {
    AllocationProperties allocationProperties{0, 0, GraphicsAllocation::AllocationType::BUFFER};

    for (auto uncached : ::testing::Bool()) {
        for (auto readOnly : ::testing::Bool()) {
            for (auto deviceOnlyVisibilty : ::testing::Bool()) {
                if (uncached || readOnly || deviceOnlyVisibilty) {
                    allocationProperties.flags.flushL3RequiredForRead = true;
                    allocationProperties.flags.flushL3RequiredForWrite = true;
                    MemoryPropertiesParser::fillCachePolicyInProperties(allocationProperties, uncached, readOnly, deviceOnlyVisibilty);
                    EXPECT_FALSE(allocationProperties.flags.flushL3RequiredForRead);
                    EXPECT_FALSE(allocationProperties.flags.flushL3RequiredForWrite);
                } else {
                    allocationProperties.flags.flushL3RequiredForRead = false;
                    allocationProperties.flags.flushL3RequiredForWrite = false;
                    MemoryPropertiesParser::fillCachePolicyInProperties(allocationProperties, uncached, readOnly, deviceOnlyVisibilty);
                    EXPECT_TRUE(allocationProperties.flags.flushL3RequiredForRead);
                    EXPECT_TRUE(allocationProperties.flags.flushL3RequiredForWrite);
                }
            }
        }
    }
}
