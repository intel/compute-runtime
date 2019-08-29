/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/memory_properties_flags_helpers.h"
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

TEST(MemObjHelper, givenClMemForceLinearStorageFlagWhenCheckForLinearStorageForceThenReturnProperValue) {
    MemoryProperties properties;
    MemoryPropertiesFlags memoryProperties;

    properties.flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    properties.flags_intel = 0;
    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(properties);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    properties.flags = 0;
    properties.flags_intel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(properties);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    properties.flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    properties.flags_intel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(properties);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    properties.flags = 0;
    properties.flags_intel = 0;
    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(properties);
    EXPECT_FALSE(memoryProperties.flags.forceLinearStorage);
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

    properties.flags_intel = CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(properties));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, nullptr));

    properties.flags = 0;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(properties));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, nullptr));
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
