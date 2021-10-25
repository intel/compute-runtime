/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

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
    UltDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];
    MemoryProperties memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;

    flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    flagsIntel = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    flags = 0;
    flagsIntel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    flags |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    flagsIntel |= CL_MEM_FORCE_LINEAR_STORAGE_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.forceLinearStorage);

    flags = 0;
    flagsIntel = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_FALSE(memoryProperties.flags.forceLinearStorage);
}

TEST(MemObjHelper, givenValidPropertiesWhenValidatingMemoryPropertiesThenTrueIsReturned) {
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    UltClDeviceFactory deviceFactory{1, 0};
    auto pDevice = &deviceFactory.rootDevices[0]->getDevice();
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    MockContext context{deviceFactory.rootDevices[0]};

    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flags = CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL | CL_MEM_NO_ACCESS_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL | CL_MEM_NO_ACCESS_INTEL;
    flagsIntel = 0;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flags = CL_MEM_NO_ACCESS_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = CL_MEM_NO_ACCESS_INTEL;
    flagsIntel = 0;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flags = CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR | CL_MEM_HOST_NO_ACCESS;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR | CL_MEM_HOST_NO_ACCESS;
    flagsIntel = 0;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flags = CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR | CL_MEM_HOST_WRITE_ONLY;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR | CL_MEM_HOST_WRITE_ONLY;
    flagsIntel = 0;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flags = CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS;
    flagsIntel = 0;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flagsIntel = CL_MEM_LOCALLY_UNCACHED_RESOURCE;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = 0;
    flagsIntel = CL_MEM_LOCALLY_UNCACHED_RESOURCE;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flagsIntel = CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = 0;
    flagsIntel = CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));

    flags = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    flags = 0;
    flagsIntel = 0;
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(memoryProperties, flags, flagsIntel, context));
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, flagsIntel, nullptr, context));
}

struct Image1dWithAccessFlagsUnrestricted : public Image1dDefaults {
    enum { flags = CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL };
};

TEST(MemObjHelper, givenParentMemObjAndHostPtrFlagsWhenValidatingMemoryPropertiesForImageThenFalseIsReturned) {
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    UltClDeviceFactory deviceFactory{1, 0};
    auto pDevice = &deviceFactory.rootDevices[0]->getDevice();
    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);

    MockContext context{deviceFactory.rootDevices[0]};
    auto image = clUniquePtr(Image1dHelper<>::create(&context));
    auto imageWithAccessFlagsUnrestricted = clUniquePtr(ImageHelper<Image1dWithAccessFlagsUnrestricted>::create(&context));

    cl_mem_flags hostPtrFlags[] = {CL_MEM_USE_HOST_PTR, CL_MEM_ALLOC_HOST_PTR, CL_MEM_COPY_HOST_PTR};

    for (auto hostPtrFlag : hostPtrFlags) {
        flags = hostPtrFlag;
        memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
        flags = hostPtrFlag;
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, 0, image.get(), context));
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, 0, imageWithAccessFlagsUnrestricted.get(), context));

        flags |= CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL;
        memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
        flags |= CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL;
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, 0, image.get(), context));
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(memoryProperties, flags, 0, imageWithAccessFlagsUnrestricted.get(), context));
    }
}

TEST(MemObjHelper, givenContextWithMultipleRootDevicesWhenIsSuitableForRenderCompressionIsCalledThenFalseIsReturned) {
    MockDefaultContext context;

    MemoryProperties memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &context.pRootDevice0->getDevice());
    EXPECT_FALSE(MemObjHelper::isSuitableForRenderCompression(true, memoryProperties, context, true));
}