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

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_READ_WRITE);
    EXPECT_TRUE(properties.flags.readWrite);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_WRITE_ONLY);
    EXPECT_TRUE(properties.flags.writeOnly);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_READ_ONLY);
    EXPECT_TRUE(properties.flags.readOnly);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_USE_HOST_PTR);
    EXPECT_TRUE(properties.flags.useHostPtr);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_ALLOC_HOST_PTR);
    EXPECT_TRUE(properties.flags.allocHostPtr);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_COPY_HOST_PTR);
    EXPECT_TRUE(properties.flags.copyHostPtr);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_HOST_WRITE_ONLY);
    EXPECT_TRUE(properties.flags.hostWriteOnly);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_HOST_READ_ONLY);
    EXPECT_TRUE(properties.flags.hostReadOnly);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_HOST_NO_ACCESS);
    EXPECT_TRUE(properties.flags.hostNoAccess);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_KERNEL_READ_AND_WRITE);
    EXPECT_TRUE(properties.flags.kernelReadAndWrite);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);
    EXPECT_TRUE(properties.flags.accessFlagsUnrestricted);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_NO_ACCESS_INTEL);
    EXPECT_TRUE(properties.flags.noAccess);

    MemoryProperties memoryProperties;
    memoryProperties.flagsIntel = CL_MEM_LOCALLY_UNCACHED_RESOURCE;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_TRUE(properties.flags.locallyUncachedResource);

    memoryProperties.flagsIntel = CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_TRUE(properties.flags.locallyUncachedInSurfaceState);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL);
    EXPECT_TRUE(properties.flags.forceSharedPhysicalMemory);
}

TEST(MemoryPropertiesFlags, givenClMemForceLinearStorageFlagWhenCreateMemoryPropertiesFlagsThenReturnProperValue) {
    MemoryPropertiesFlags properties;
    MemoryProperties memoryProperties;

    memoryProperties.flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties.flagsIntel = 0;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_TRUE(properties.flags.forceLinearStorage);

    memoryProperties.flags = 0;
    memoryProperties.flagsIntel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_TRUE(properties.flags.forceLinearStorage);

    memoryProperties.flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties.flagsIntel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_TRUE(properties.flags.forceLinearStorage);

    memoryProperties.flags = 0;
    memoryProperties.flagsIntel = 0;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_FALSE(properties.flags.forceLinearStorage);
}

TEST(MemoryPropertiesFlags, givenClAllowUnrestrictedSizeFlagWhenCreateMemoryPropertiesFlagsThenReturnProperValue) {
    MemoryPropertiesFlags properties;
    MemoryProperties memoryProperties;

    memoryProperties.flags |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    memoryProperties.flagsIntel = 0;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_TRUE(properties.flags.allowUnrestrictedSize);

    memoryProperties.flags = 0;
    memoryProperties.flagsIntel |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_TRUE(properties.flags.allowUnrestrictedSize);

    memoryProperties.flags |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    memoryProperties.flagsIntel |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_TRUE(properties.flags.allowUnrestrictedSize);

    memoryProperties.flags = 0;
    memoryProperties.flagsIntel = 0;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_FALSE(properties.flags.allowUnrestrictedSize);
}
