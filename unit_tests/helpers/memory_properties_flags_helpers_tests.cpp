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
    EXPECT_TRUE(properties.readWrite);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_WRITE_ONLY);
    EXPECT_TRUE(properties.writeOnly);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_READ_ONLY);
    EXPECT_TRUE(properties.readOnly);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_USE_HOST_PTR);
    EXPECT_TRUE(properties.useHostPtr);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_ALLOC_HOST_PTR);
    EXPECT_TRUE(properties.allocHostPtr);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_COPY_HOST_PTR);
    EXPECT_TRUE(properties.copyHostPtr);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_HOST_WRITE_ONLY);
    EXPECT_TRUE(properties.hostWriteOnly);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_HOST_READ_ONLY);
    EXPECT_TRUE(properties.hostReadOnly);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_HOST_NO_ACCESS);
    EXPECT_TRUE(properties.hostNoAccess);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_KERNEL_READ_AND_WRITE);
    EXPECT_TRUE(properties.kernelReadAndWrite);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);
    EXPECT_TRUE(properties.accessFlagsUnrestricted);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_NO_ACCESS_INTEL);
    EXPECT_TRUE(properties.noAccess);

    MemoryProperties memoryProperties;
    memoryProperties.flags_intel = CL_MEM_LOCALLY_UNCACHED_RESOURCE;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_TRUE(properties.locallyUncachedResource);

    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(CL_MEM_FORCE_SHARED_PHYSICAL_MEMORY_INTEL);
    EXPECT_TRUE(properties.forceSharedPhysicalMemory);
}

TEST(MemoryPropertiesFlags, givenClMemForceLinearStorageFlagWhenCreateMemoryPropertiesFlagsThenReturnProperValue) {
    MemoryPropertiesFlags properties;
    MemoryProperties memoryProperties;

    memoryProperties.flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties.flags_intel = 0;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_TRUE(properties.forceLinearStorage);

    memoryProperties.flags = 0;
    memoryProperties.flags_intel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_TRUE(properties.forceLinearStorage);

    memoryProperties.flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties.flags_intel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_TRUE(properties.forceLinearStorage);

    memoryProperties.flags = 0;
    memoryProperties.flags_intel = 0;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_FALSE(properties.forceLinearStorage);
}

TEST(MemoryPropertiesFlags, givenClAllowUnrestrictedSizeFlagWhenCreateMemoryPropertiesFlagsThenReturnProperValue) {
    MemoryPropertiesFlags properties;
    MemoryProperties memoryProperties;

    memoryProperties.flags |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    memoryProperties.flags_intel = 0;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_TRUE(properties.allowUnrestrictedSize);

    memoryProperties.flags = 0;
    memoryProperties.flags_intel |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_TRUE(properties.allowUnrestrictedSize);

    memoryProperties.flags |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    memoryProperties.flags_intel |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_TRUE(properties.allowUnrestrictedSize);

    memoryProperties.flags = 0;
    memoryProperties.flags_intel = 0;
    properties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(memoryProperties);
    EXPECT_FALSE(properties.allowUnrestrictedSize);
}