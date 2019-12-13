/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/memory_properties_flags_helpers.h"

#include "CL/cl_ext_intel.h"
#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryPropertiesFlags, givenValidPropertiesWhenCreateMemoryPropertiesFlagsThenTrueIsReturned) {
    MemoryPropertiesFlags properties;

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_READ_WRITE, 0, 0);
    EXPECT_TRUE(properties.flags.readWrite);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_WRITE_ONLY, 0, 0);
    EXPECT_TRUE(properties.flags.writeOnly);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_READ_ONLY, 0, 0);
    EXPECT_TRUE(properties.flags.readOnly);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_USE_HOST_PTR, 0, 0);
    EXPECT_TRUE(properties.flags.useHostPtr);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_ALLOC_HOST_PTR, 0, 0);
    EXPECT_TRUE(properties.flags.allocHostPtr);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_COPY_HOST_PTR, 0, 0);
    EXPECT_TRUE(properties.flags.copyHostPtr);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_HOST_WRITE_ONLY, 0, 0);
    EXPECT_TRUE(properties.flags.hostWriteOnly);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_HOST_READ_ONLY, 0, 0);
    EXPECT_TRUE(properties.flags.hostReadOnly);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_HOST_NO_ACCESS, 0, 0);
    EXPECT_TRUE(properties.flags.hostNoAccess);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_KERNEL_READ_AND_WRITE, 0, 0);
    EXPECT_TRUE(properties.flags.kernelReadAndWrite);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL, 0, 0);
    EXPECT_TRUE(properties.flags.accessFlagsUnrestricted);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_NO_ACCESS_INTEL, 0, 0);
    EXPECT_TRUE(properties.flags.noAccess);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(0, CL_MEM_LOCALLY_UNCACHED_RESOURCE, 0);
    EXPECT_TRUE(properties.flags.locallyUncachedResource);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(0, CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE, 0);
    EXPECT_TRUE(properties.flags.locallyUncachedInSurfaceState);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL, 0, 0);
    EXPECT_TRUE(properties.flags.forceSharedPhysicalMemory);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(0, 0, CL_MEM_ALLOC_WRITE_COMBINED_INTEL);
    EXPECT_TRUE(properties.allocFlags.allocWriteCombined);
}

TEST(MemoryPropertiesFlags, givenClMemForceLinearStorageFlagWhenCreateMemoryPropertiesFlagsThenReturnProperValue) {
    MemoryPropertiesFlags memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;

    flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    flagsIntel = 0;
    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, flagsIntel, 0);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    flags = 0;
    flagsIntel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, flagsIntel, 0);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    flagsIntel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, flagsIntel, 0);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    flags = 0;
    flagsIntel = 0;
    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, flagsIntel, 0);
    EXPECT_FALSE(memoryProperties.flags.forceLinearStorage);
}

TEST(MemoryPropertiesFlags, givenClAllowUnrestrictedSizeFlagWhenCreateMemoryPropertiesFlagsThenReturnProperValue) {
    MemoryPropertiesFlags memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;

    flags |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    flagsIntel = 0;
    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, flagsIntel, 0);
    EXPECT_TRUE(memoryProperties.flags.allowUnrestrictedSize);

    flags = 0;
    flagsIntel |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, flagsIntel, 0);
    EXPECT_TRUE(memoryProperties.flags.allowUnrestrictedSize);

    flags |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    flagsIntel |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, flagsIntel, 0);
    EXPECT_TRUE(memoryProperties.flags.allowUnrestrictedSize);

    flags = 0;
    flagsIntel = 0;
    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, flagsIntel, 0);
    EXPECT_FALSE(memoryProperties.flags.allowUnrestrictedSize);
}
