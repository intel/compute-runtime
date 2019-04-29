/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/mem_obj_helper.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/utilities/base_object_utils.h"

#include "gtest/gtest.h"

using namespace NEO;

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

TEST(MemObjHelper, givenClMemForceLinearStorageFlagWhenCheckForLinearStorageForceThenReturnProperValue) {
    MemoryProperties properties;

    properties.flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    properties.flags_intel = 0;
    EXPECT_TRUE(MemObjHelper::isLinearStorageForced(properties));

    properties.flags = 0;
    properties.flags_intel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    EXPECT_TRUE(MemObjHelper::isLinearStorageForced(properties));

    properties.flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    properties.flags_intel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    EXPECT_TRUE(MemObjHelper::isLinearStorageForced(properties));

    properties.flags = 0;
    properties.flags_intel = 0;
    EXPECT_FALSE(MemObjHelper::isLinearStorageForced(properties));
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
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(properties));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, nullptr));

    properties.flags = CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL | CL_MEM_NO_ACCESS_INTEL;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, nullptr));

    properties.flags = CL_MEM_NO_ACCESS_INTEL;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, nullptr));

    properties.flags = CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR | CL_MEM_HOST_NO_ACCESS;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(properties));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, nullptr));

    properties.flags = CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR | CL_MEM_HOST_WRITE_ONLY;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(properties));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, nullptr));

    properties.flags = CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(properties));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, nullptr));

    properties.flags_intel = CL_MEM_LOCALLY_UNCACHED_RESOURCE;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(properties));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, nullptr));

    properties.flags = 0;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(properties));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, nullptr));
}

TEST(MemObjHelper, givenInvalidPropertiesWhenValidatingMemoryPropertiesThenFalseIsReturned) {
    MemoryProperties properties;
    properties.flags = (1 << 31);
    EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForBuffer(properties));
    EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(properties, nullptr));

    properties.flags = CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL | CL_MEM_NO_ACCESS_INTEL;
    EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForBuffer(properties));

    properties.flags = CL_MEM_NO_ACCESS_INTEL;
    EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForBuffer(properties));

    properties.flags_intel = (1 << 31);
    EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForBuffer(properties));
    EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(properties, nullptr));

    properties.flags = 0;
    EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForBuffer(properties));
    EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(properties, nullptr));
}

struct Image1dWithAccessFlagsUnrestricted : public Image1dDefaults {
    enum { flags = CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL };
};

TEST(MemObjHelper, givenParentMemObjAndHostPtrFlagsWhenValidatingMemoryPropertiesForImageThenFalseIsReturned) {
    MemoryProperties properties;
    MockContext context;
    auto image = clUniquePtr(Image1dHelper<>::create(&context));
    auto imageWithAccessFlagsUnrestricted = clUniquePtr(ImageHelper<Image1dWithAccessFlagsUnrestricted>::create(&context));

    cl_mem_flags hostPtrFlags[] = {CL_MEM_USE_HOST_PTR, CL_MEM_ALLOC_HOST_PTR, CL_MEM_COPY_HOST_PTR};

    for (auto hostPtrFlag : hostPtrFlags) {
        properties.flags = hostPtrFlag;
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(properties, image.get()));
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(properties, imageWithAccessFlagsUnrestricted.get()));

        properties.flags |= CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL;
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(properties, image.get()));
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(properties, imageWithAccessFlagsUnrestricted.get()));
    }
}

TEST(MemObjHelper, givenDifferentParametersWhenCallingFillCachePolicyInPropertiesThenFlushL3FlagsAreCorrectlySet) {
    AllocationProperties allocationProperties{0, GraphicsAllocation::AllocationType::BUFFER};

    for (auto uncached : ::testing::Bool()) {
        for (auto readOnly : ::testing::Bool()) {
            for (auto deviceOnlyVisibilty : ::testing::Bool()) {
                if (uncached || readOnly || deviceOnlyVisibilty) {
                    allocationProperties.flags.flushL3RequiredForRead = true;
                    allocationProperties.flags.flushL3RequiredForWrite = true;
                    MemObjHelper::fillCachePolicyInProperties(allocationProperties, uncached, readOnly, deviceOnlyVisibilty);
                    EXPECT_FALSE(allocationProperties.flags.flushL3RequiredForRead);
                    EXPECT_FALSE(allocationProperties.flags.flushL3RequiredForWrite);
                } else {
                    allocationProperties.flags.flushL3RequiredForRead = false;
                    allocationProperties.flags.flushL3RequiredForWrite = false;
                    MemObjHelper::fillCachePolicyInProperties(allocationProperties, uncached, readOnly, deviceOnlyVisibilty);
                    EXPECT_TRUE(allocationProperties.flags.flushL3RequiredForRead);
                    EXPECT_TRUE(allocationProperties.flags.flushL3RequiredForWrite);
                }
            }
        }
    }
}
