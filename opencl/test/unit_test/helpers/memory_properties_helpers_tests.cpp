/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/mocks/mock_device.h"
#include "shared/test/unit_test/mocks/ult_device_factory.h"

#include "opencl/source/helpers/memory_properties_helpers.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"

#include "CL/cl_ext_intel.h"
#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryProperties, givenValidPropertiesWhenCreateMemoryPropertiesThenTrueIsReturned) {
    UltDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];
    MemoryProperties properties;

    properties = MemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.readWrite);

    properties = MemoryPropertiesHelper::createMemoryProperties(CL_MEM_WRITE_ONLY, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.writeOnly);

    properties = MemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_ONLY, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.readOnly);

    properties = MemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.useHostPtr);

    properties = MemoryPropertiesHelper::createMemoryProperties(CL_MEM_ALLOC_HOST_PTR, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.allocHostPtr);

    properties = MemoryPropertiesHelper::createMemoryProperties(CL_MEM_COPY_HOST_PTR, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.copyHostPtr);

    properties = MemoryPropertiesHelper::createMemoryProperties(CL_MEM_HOST_WRITE_ONLY, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.hostWriteOnly);

    properties = MemoryPropertiesHelper::createMemoryProperties(CL_MEM_HOST_READ_ONLY, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.hostReadOnly);

    properties = MemoryPropertiesHelper::createMemoryProperties(CL_MEM_HOST_NO_ACCESS, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.hostNoAccess);

    properties = MemoryPropertiesHelper::createMemoryProperties(CL_MEM_KERNEL_READ_AND_WRITE, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.kernelReadAndWrite);

    properties = MemoryPropertiesHelper::createMemoryProperties(CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.accessFlagsUnrestricted);

    properties = MemoryPropertiesHelper::createMemoryProperties(CL_MEM_NO_ACCESS_INTEL, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.noAccess);

    properties = MemoryPropertiesHelper::createMemoryProperties(0, CL_MEM_LOCALLY_UNCACHED_RESOURCE, 0, pDevice);
    EXPECT_TRUE(properties.flags.locallyUncachedResource);

    properties = MemoryPropertiesHelper::createMemoryProperties(0, CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE, 0, pDevice);
    EXPECT_TRUE(properties.flags.locallyUncachedInSurfaceState);

    properties = MemoryPropertiesHelper::createMemoryProperties(CL_MEM_FORCE_HOST_MEMORY_INTEL, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.forceHostMemory);

    properties = MemoryPropertiesHelper::createMemoryProperties(0, 0, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, pDevice);
    EXPECT_TRUE(properties.allocFlags.allocWriteCombined);

    properties = MemoryPropertiesHelper::createMemoryProperties(0, CL_MEM_48BIT_RESOURCE_INTEL, 0, pDevice);
    EXPECT_TRUE(properties.flags.resource48Bit);
}

TEST(MemoryProperties, givenClMemForceLinearStorageFlagWhenCreateMemoryPropertiesThenReturnProperValue) {
    UltDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];
    MemoryProperties memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;

    flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    flagsIntel = 0;
    memoryProperties = MemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    flags = 0;
    flagsIntel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties = MemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    flagsIntel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties = MemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    flags = 0;
    flagsIntel = 0;
    memoryProperties = MemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_FALSE(memoryProperties.flags.forceLinearStorage);
}

TEST(MemoryProperties, givenClAllowUnrestrictedSizeFlagWhenCreateMemoryPropertiesThenReturnProperValue) {
    UltDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];
    MemoryProperties memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;

    flags |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    flagsIntel = 0;
    memoryProperties = MemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.allowUnrestrictedSize);

    flags = 0;
    flagsIntel |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    memoryProperties = MemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.allowUnrestrictedSize);

    flags |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    flagsIntel |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    memoryProperties = MemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.allowUnrestrictedSize);

    flags = 0;
    flagsIntel = 0;
    memoryProperties = MemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_FALSE(memoryProperties.flags.allowUnrestrictedSize);
}

struct MemoryPropertiesHelperTests : ::testing::Test {
    MockContext context;
    MemoryProperties memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
};

TEST_F(MemoryPropertiesHelperTests, givenNullPropertiesWhenParsingMemoryPropertiesThenTrueIsReturned) {
    EXPECT_TRUE(MemoryPropertiesHelper::parseMemoryProperties(nullptr, memoryProperties, flags, flagsIntel, allocflags,
                                                              MemoryPropertiesHelper::ObjType::UNKNOWN, context));
}

TEST_F(MemoryPropertiesHelperTests, givenEmptyPropertiesWhenParsingMemoryPropertiesThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {0};

    EXPECT_TRUE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                              MemoryPropertiesHelper::ObjType::UNKNOWN, context));
    EXPECT_TRUE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                              MemoryPropertiesHelper::ObjType::BUFFER, context));
    EXPECT_TRUE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                              MemoryPropertiesHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesHelperTests, givenValidPropertiesWhenParsingMemoryPropertiesThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR |
            CL_MEM_USE_HOST_PTR | CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS,
        CL_MEM_FLAGS_INTEL,
        CL_MEM_LOCALLY_UNCACHED_RESOURCE | CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE,
        CL_MEM_ALLOC_FLAGS_INTEL,
        CL_MEM_ALLOC_WRITE_COMBINED_INTEL, CL_MEM_ALLOC_DEFAULT_INTEL,
        0};

    EXPECT_TRUE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                              MemoryPropertiesHelper::ObjType::UNKNOWN, context));
}

TEST_F(MemoryPropertiesHelperTests, givenValidPropertiesWhenParsingMemoryPropertiesForBufferThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForBuffer,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForBufferIntel,
        0};

    EXPECT_TRUE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                              MemoryPropertiesHelper::ObjType::BUFFER, context));
}

TEST_F(MemoryPropertiesHelperTests, givenValidPropertiesWhenParsingMemoryPropertiesForImageThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForImage,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForImageIntel,
        0};

    EXPECT_TRUE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                              MemoryPropertiesHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesHelperTests, givenInvalidPropertiesWhenParsingMemoryPropertiesThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        (1 << 30), CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR,
        0};

    EXPECT_FALSE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                               MemoryPropertiesHelper::ObjType::UNKNOWN, context));
    EXPECT_FALSE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                               MemoryPropertiesHelper::ObjType::BUFFER, context));
    EXPECT_FALSE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                               MemoryPropertiesHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesHelperTests, givenInvalidPropertiesWhenParsingMemoryPropertiesForImageThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForBuffer,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForBufferIntel,
        0};

    EXPECT_FALSE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                               MemoryPropertiesHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesHelperTests, givenInvalidFlagsWhenParsingMemoryPropertiesForImageThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        (1 << 30),
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForImageIntel,
        0};

    EXPECT_FALSE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                               MemoryPropertiesHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesHelperTests, givenInvalidFlagsIntelWhenParsingMemoryPropertiesForImageThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForImage,
        CL_MEM_FLAGS_INTEL,
        (1 << 30),
        0};

    EXPECT_FALSE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                               MemoryPropertiesHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesHelperTests, givenInvalidPropertiesWhenParsingMemoryPropertiesForBufferThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForImage,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForImageIntel,
        0};

    EXPECT_FALSE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                               MemoryPropertiesHelper::ObjType::BUFFER, context));
}

TEST_F(MemoryPropertiesHelperTests, givenInvalidFlagsWhenParsingMemoryPropertiesForBufferThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        (1 << 30),
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForBufferIntel,
        0};

    EXPECT_FALSE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                               MemoryPropertiesHelper::ObjType::BUFFER, context));
}

TEST_F(MemoryPropertiesHelperTests, givenInvalidFlagsIntelWhenParsingMemoryPropertiesForBufferThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForBuffer,
        CL_MEM_FLAGS_INTEL,
        (1 << 30),
        0};

    EXPECT_FALSE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                               MemoryPropertiesHelper::ObjType::BUFFER, context));
}

TEST_F(MemoryPropertiesHelperTests, givenDifferentParametersWhenCallingFillCachePolicyInPropertiesThenFlushL3FlagsAreCorrectlySet) {
    AllocationProperties allocationProperties{mockRootDeviceIndex, 0, GraphicsAllocation::AllocationType::BUFFER, mockDeviceBitfield};

    for (auto uncached : ::testing::Bool()) {
        for (auto readOnly : ::testing::Bool()) {
            for (auto deviceOnlyVisibilty : ::testing::Bool()) {
                if (uncached || readOnly || deviceOnlyVisibilty) {
                    allocationProperties.flags.flushL3RequiredForRead = true;
                    allocationProperties.flags.flushL3RequiredForWrite = true;
                    MemoryPropertiesHelper::fillCachePolicyInProperties(allocationProperties, uncached, readOnly, deviceOnlyVisibilty);
                    EXPECT_FALSE(allocationProperties.flags.flushL3RequiredForRead);
                    EXPECT_FALSE(allocationProperties.flags.flushL3RequiredForWrite);
                } else {
                    allocationProperties.flags.flushL3RequiredForRead = false;
                    allocationProperties.flags.flushL3RequiredForWrite = false;
                    MemoryPropertiesHelper::fillCachePolicyInProperties(allocationProperties, uncached, readOnly, deviceOnlyVisibilty);
                    EXPECT_TRUE(allocationProperties.flags.flushL3RequiredForRead);
                    EXPECT_TRUE(allocationProperties.flags.flushL3RequiredForWrite);
                }
            }
        }
    }
}

TEST_F(MemoryPropertiesHelperTests, givenMemFlagsWithFlagsAndPropertiesWhenParsingMemoryPropertiesThenTheyAreCorrectlyParsed) {
    struct TestInput {
        cl_mem_flags flagsParameter;
        cl_mem_properties_intel flagsProperties;
        cl_mem_flags expectedResult;
    };

    TestInput testInputs[] = {
        {0b0, 0b0, 0b0},
        {0b0, 0b1010, 0b1010},
        {0b1010, 0b0, 0b1010},
        {0b1010, 0b101, 0b1111},
        {0b1010, 0b1010, 0b1010},
        {0b1111, 0b1111, 0b1111}};

    for (auto &testInput : testInputs) {
        flags = testInput.flagsParameter;
        cl_mem_properties_intel properties[] = {
            CL_MEM_FLAGS, testInput.flagsProperties,
            0};
        EXPECT_TRUE(MemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                  MemoryPropertiesHelper::ObjType::UNKNOWN, context));
        EXPECT_EQ(testInput.expectedResult, flags);
    }
}
