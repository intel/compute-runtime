/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/memory_properties_flags_helpers.h"

#include "CL/cl_ext_intel.h"
#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryProperties, givenValidPropertiesWhenCreateMemoryPropertiesThenTrueIsReturned) {
    MemoryProperties properties;

    properties = MemoryPropertiesParser::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0);
    EXPECT_TRUE(properties.flags.readWrite);

    properties = MemoryPropertiesParser::createMemoryProperties(CL_MEM_WRITE_ONLY, 0, 0);
    EXPECT_TRUE(properties.flags.writeOnly);

    properties = MemoryPropertiesParser::createMemoryProperties(CL_MEM_READ_ONLY, 0, 0);
    EXPECT_TRUE(properties.flags.readOnly);

    properties = MemoryPropertiesParser::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0);
    EXPECT_TRUE(properties.flags.useHostPtr);

    properties = MemoryPropertiesParser::createMemoryProperties(CL_MEM_ALLOC_HOST_PTR, 0, 0);
    EXPECT_TRUE(properties.flags.allocHostPtr);

    properties = MemoryPropertiesParser::createMemoryProperties(CL_MEM_COPY_HOST_PTR, 0, 0);
    EXPECT_TRUE(properties.flags.copyHostPtr);

    properties = MemoryPropertiesParser::createMemoryProperties(CL_MEM_HOST_WRITE_ONLY, 0, 0);
    EXPECT_TRUE(properties.flags.hostWriteOnly);

    properties = MemoryPropertiesParser::createMemoryProperties(CL_MEM_HOST_READ_ONLY, 0, 0);
    EXPECT_TRUE(properties.flags.hostReadOnly);

    properties = MemoryPropertiesParser::createMemoryProperties(CL_MEM_HOST_NO_ACCESS, 0, 0);
    EXPECT_TRUE(properties.flags.hostNoAccess);

    properties = MemoryPropertiesParser::createMemoryProperties(CL_MEM_KERNEL_READ_AND_WRITE, 0, 0);
    EXPECT_TRUE(properties.flags.kernelReadAndWrite);

    properties = MemoryPropertiesParser::createMemoryProperties(CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL, 0, 0);
    EXPECT_TRUE(properties.flags.accessFlagsUnrestricted);

    properties = MemoryPropertiesParser::createMemoryProperties(CL_MEM_NO_ACCESS_INTEL, 0, 0);
    EXPECT_TRUE(properties.flags.noAccess);

    properties = MemoryPropertiesParser::createMemoryProperties(0, CL_MEM_LOCALLY_UNCACHED_RESOURCE, 0);
    EXPECT_TRUE(properties.flags.locallyUncachedResource);

    properties = MemoryPropertiesParser::createMemoryProperties(0, CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE, 0);
    EXPECT_TRUE(properties.flags.locallyUncachedInSurfaceState);

    properties = MemoryPropertiesParser::createMemoryProperties(CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL, 0, 0);
    EXPECT_TRUE(properties.flags.forceSharedPhysicalMemory);

    properties = MemoryPropertiesParser::createMemoryProperties(0, 0, CL_MEM_ALLOC_WRITE_COMBINED_INTEL);
    EXPECT_TRUE(properties.allocFlags.allocWriteCombined);

    properties = MemoryPropertiesParser::createMemoryProperties(0, CL_MEM_48BIT_RESOURCE_INTEL, 0);
    EXPECT_TRUE(properties.flags.resource48Bit);
}

TEST(MemoryProperties, givenClMemForceLinearStorageFlagWhenCreateMemoryPropertiesThenReturnProperValue) {
    MemoryProperties memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;

    flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    flagsIntel = 0;
    memoryProperties = MemoryPropertiesParser::createMemoryProperties(flags, flagsIntel, 0);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    flags = 0;
    flagsIntel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties = MemoryPropertiesParser::createMemoryProperties(flags, flagsIntel, 0);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    flagsIntel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties = MemoryPropertiesParser::createMemoryProperties(flags, flagsIntel, 0);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    flags = 0;
    flagsIntel = 0;
    memoryProperties = MemoryPropertiesParser::createMemoryProperties(flags, flagsIntel, 0);
    EXPECT_FALSE(memoryProperties.flags.forceLinearStorage);
}

TEST(MemoryProperties, givenClAllowUnrestrictedSizeFlagWhenCreateMemoryPropertiesThenReturnProperValue) {
    MemoryProperties memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;

    flags |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    flagsIntel = 0;
    memoryProperties = MemoryPropertiesParser::createMemoryProperties(flags, flagsIntel, 0);
    EXPECT_TRUE(memoryProperties.flags.allowUnrestrictedSize);

    flags = 0;
    flagsIntel |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    memoryProperties = MemoryPropertiesParser::createMemoryProperties(flags, flagsIntel, 0);
    EXPECT_TRUE(memoryProperties.flags.allowUnrestrictedSize);

    flags |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    flagsIntel |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    memoryProperties = MemoryPropertiesParser::createMemoryProperties(flags, flagsIntel, 0);
    EXPECT_TRUE(memoryProperties.flags.allowUnrestrictedSize);

    flags = 0;
    flagsIntel = 0;
    memoryProperties = MemoryPropertiesParser::createMemoryProperties(flags, flagsIntel, 0);
    EXPECT_FALSE(memoryProperties.flags.allowUnrestrictedSize);
}
