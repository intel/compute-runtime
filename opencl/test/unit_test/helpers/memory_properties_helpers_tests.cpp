/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/memory_properties_helpers.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "CL/cl_ext.h"
#include "gtest/gtest.h"
#include "memory_properties_flags.h"

using namespace NEO;

TEST(MemoryProperties, givenResource48BitMemoryPropertySetWhenGetAllocationPropertiesCalledThenSetAllocationPropertyToo) {
    UltDeviceFactory deviceFactory{1, 0};
    MemoryProperties memoryProperties{};
    memoryProperties.pDevice = deviceFactory.rootDevices[0];
    memoryProperties.flags.resource48Bit = true;

    DeviceBitfield deviceBitfield{0xf};

    HardwareInfo hwInfo(*defaultHwInfo);

    auto allocationProperties = MemoryPropertiesHelper::getAllocationProperties(0, memoryProperties, true, 0, AllocationType::BUFFER,
                                                                                false, hwInfo, deviceBitfield, false);

    EXPECT_EQ(1u, allocationProperties.flags.resource48Bit);
}

TEST(MemoryProperties, givenValidPropertiesWhenCreateMemoryPropertiesThenTrueIsReturned) {
    UltDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];
    MemoryProperties properties;

    properties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.readWrite);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_WRITE_ONLY, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.writeOnly);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_ONLY, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.readOnly);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_USE_HOST_PTR, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.useHostPtr);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_ALLOC_HOST_PTR, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.allocHostPtr);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_COPY_HOST_PTR, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.copyHostPtr);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_HOST_WRITE_ONLY, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.hostWriteOnly);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_HOST_READ_ONLY, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.hostReadOnly);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_HOST_NO_ACCESS, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.hostNoAccess);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_KERNEL_READ_AND_WRITE, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.kernelReadAndWrite);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.accessFlagsUnrestricted);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_NO_ACCESS_INTEL, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.noAccess);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(0, CL_MEM_LOCALLY_UNCACHED_RESOURCE, 0, pDevice);
    EXPECT_TRUE(properties.flags.locallyUncachedResource);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(0, CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE, 0, pDevice);
    EXPECT_TRUE(properties.flags.locallyUncachedInSurfaceState);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_FORCE_HOST_MEMORY_INTEL, 0, 0, pDevice);
    EXPECT_TRUE(properties.flags.forceHostMemory);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, pDevice);
    EXPECT_TRUE(properties.allocFlags.allocWriteCombined);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, CL_MEM_ALLOC_INITIAL_PLACEMENT_DEVICE_INTEL, pDevice);
    EXPECT_TRUE(properties.allocFlags.usmInitialPlacementGpu);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, CL_MEM_ALLOC_INITIAL_PLACEMENT_HOST_INTEL, pDevice);
    EXPECT_TRUE(properties.allocFlags.usmInitialPlacementCpu);

    properties = ClMemoryPropertiesHelper::createMemoryProperties(0, CL_MEM_48BIT_RESOURCE_INTEL, 0, pDevice);
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

TEST(MemoryProperties, givenClAllowUnrestrictedSizeFlagWhenCreateMemoryPropertiesThenReturnProperValue) {
    UltDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];
    MemoryProperties memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;

    flags |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    flagsIntel = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.allowUnrestrictedSize);

    flags = 0;
    flagsIntel |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.allowUnrestrictedSize);

    flags |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    flagsIntel |= CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.allowUnrestrictedSize);

    flags = 0;
    flagsIntel = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_FALSE(memoryProperties.flags.allowUnrestrictedSize);
}

TEST(MemoryProperties, givenClCompressedHintFlagWhenCreateMemoryPropertiesThenReturnProperValue) {
    MemoryProperties memoryProperties;
    UltDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];

    cl_mem_flags flags = CL_MEM_COMPRESSED_HINT_INTEL;
    cl_mem_flags_intel flagsIntel = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.compressedHint);

    flags = 0;
    flagsIntel |= CL_MEM_COMPRESSED_HINT_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.compressedHint);

    flags |= CL_MEM_COMPRESSED_HINT_INTEL;
    flagsIntel |= CL_MEM_COMPRESSED_HINT_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.compressedHint);

    flags = 0;
    flagsIntel = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_FALSE(memoryProperties.flags.compressedHint);
}

TEST(MemoryProperties, givenClUncompressedHintFlagWhenCreateMemoryPropertiesThenReturnProperValue) {
    MemoryProperties memoryProperties;
    UltDeviceFactory deviceFactory{1, 0};
    auto pDevice = deviceFactory.rootDevices[0];

    cl_mem_flags flags = CL_MEM_UNCOMPRESSED_HINT_INTEL;
    cl_mem_flags_intel flagsIntel = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.uncompressedHint);

    flags = 0;
    flagsIntel |= CL_MEM_UNCOMPRESSED_HINT_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.uncompressedHint);

    flags |= CL_MEM_UNCOMPRESSED_HINT_INTEL;
    flagsIntel |= CL_MEM_UNCOMPRESSED_HINT_INTEL;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_TRUE(memoryProperties.flags.uncompressedHint);

    flags = 0;
    flagsIntel = 0;
    memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, pDevice);
    EXPECT_FALSE(memoryProperties.flags.uncompressedHint);
}

struct MemoryPropertiesHelperTests : ::testing::Test {
    MockUnrestrictiveContext context;
    MemoryProperties memoryProperties;
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocflags = 0;
    cl_mem_properties_intel rootDeviceId = reinterpret_cast<cl_mem_properties_intel>(static_cast<cl_device_id>(context.pRootDevice));
    cl_mem_properties_intel subDevice0Id = reinterpret_cast<cl_mem_properties_intel>(static_cast<cl_device_id>(context.pSubDevice0));
    cl_mem_properties_intel subDevice1Id = reinterpret_cast<cl_mem_properties_intel>(static_cast<cl_device_id>(context.pSubDevice1));
};

TEST_F(MemoryPropertiesHelperTests, givenNullPropertiesWhenParsingMemoryPropertiesThenTrueIsReturned) {
    EXPECT_TRUE(ClMemoryPropertiesHelper::parseMemoryProperties(nullptr, memoryProperties, flags, flagsIntel, allocflags,
                                                                MemoryPropertiesHelper::ObjType::UNKNOWN, context));
}

TEST_F(MemoryPropertiesHelperTests, givenEmptyPropertiesWhenParsingMemoryPropertiesThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {0};

    EXPECT_TRUE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                MemoryPropertiesHelper::ObjType::UNKNOWN, context));
    EXPECT_TRUE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                MemoryPropertiesHelper::ObjType::BUFFER, context));
    EXPECT_TRUE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                MemoryPropertiesHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesHelperTests, givenValidPropertiesWhenParsingMemoryPropertiesThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR |
            CL_MEM_USE_HOST_PTR | CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS | CL_MEM_COMPRESSED_HINT_INTEL |
            CL_MEM_UNCOMPRESSED_HINT_INTEL,
        CL_MEM_FLAGS_INTEL,
        CL_MEM_LOCALLY_UNCACHED_RESOURCE | CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE | CL_MEM_COMPRESSED_HINT_INTEL |
            CL_MEM_UNCOMPRESSED_HINT_INTEL,
        CL_MEM_ALLOC_FLAGS_INTEL,
        CL_MEM_ALLOC_WRITE_COMBINED_INTEL, CL_MEM_ALLOC_DEFAULT_INTEL,
        CL_MEM_DEVICE_ID_INTEL,
        rootDeviceId,
        0};

    EXPECT_TRUE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                MemoryPropertiesHelper::ObjType::UNKNOWN, context));
}

TEST_F(MemoryPropertiesHelperTests, givenValidPropertiesWhenParsingMemoryPropertiesForBufferThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForBuffer,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForBufferIntel,
        0};

    EXPECT_TRUE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                MemoryPropertiesHelper::ObjType::BUFFER, context));
}

TEST_F(MemoryPropertiesHelperTests, givenValidPropertiesWhenParsingMemoryPropertiesForImageThenTrueIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForImage,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForImageIntel,
        0};

    EXPECT_TRUE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                MemoryPropertiesHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesHelperTests, givenInvalidPropertiesWhenParsingMemoryPropertiesThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        (1 << 30), CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR,
        0};

    EXPECT_FALSE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                 MemoryPropertiesHelper::ObjType::UNKNOWN, context));
    EXPECT_FALSE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                 MemoryPropertiesHelper::ObjType::BUFFER, context));
    EXPECT_FALSE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                 MemoryPropertiesHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesHelperTests, givenInvalidPropertiesWhenParsingMemoryPropertiesForImageThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForBuffer,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForBufferIntel,
        0};

    EXPECT_FALSE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                 MemoryPropertiesHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesHelperTests, givenInvalidFlagsWhenParsingMemoryPropertiesForImageThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        (1 << 30),
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForImageIntel,
        0};

    EXPECT_FALSE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                 MemoryPropertiesHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesHelperTests, givenInvalidFlagsIntelWhenParsingMemoryPropertiesForImageThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForImage,
        CL_MEM_FLAGS_INTEL,
        (1 << 30),
        0};

    EXPECT_FALSE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                 MemoryPropertiesHelper::ObjType::IMAGE, context));
}

TEST_F(MemoryPropertiesHelperTests, givenInvalidPropertiesWhenParsingMemoryPropertiesForBufferThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForImage,
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForImageIntel,
        0};

    EXPECT_FALSE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                 MemoryPropertiesHelper::ObjType::BUFFER, context));
}

TEST_F(MemoryPropertiesHelperTests, givenInvalidFlagsWhenParsingMemoryPropertiesForBufferThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        (1 << 30),
        CL_MEM_FLAGS_INTEL,
        MemObjHelper::validFlagsForBufferIntel,
        0};

    EXPECT_FALSE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                 MemoryPropertiesHelper::ObjType::BUFFER, context));
}

TEST_F(MemoryPropertiesHelperTests, givenInvalidFlagsIntelWhenParsingMemoryPropertiesForBufferThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_FLAGS,
        MemObjHelper::validFlagsForBuffer,
        CL_MEM_FLAGS_INTEL,
        (1 << 30),
        0};

    EXPECT_FALSE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                 MemoryPropertiesHelper::ObjType::BUFFER, context));
}

TEST_F(MemoryPropertiesHelperTests, givenDifferentParametersWhenCallingFillCachePolicyInPropertiesThenFlushL3FlagsAreCorrectlySet) {
    AllocationProperties allocationProperties{mockRootDeviceIndex, 0, AllocationType::BUFFER, mockDeviceBitfield};

    for (auto uncached : ::testing::Bool()) {
        for (auto readOnly : ::testing::Bool()) {
            for (auto deviceOnlyVisibilty : ::testing::Bool()) {
                if (uncached || readOnly || deviceOnlyVisibilty) {
                    allocationProperties.flags.flushL3RequiredForRead = true;
                    allocationProperties.flags.flushL3RequiredForWrite = true;
                    MemoryPropertiesHelper::fillCachePolicyInProperties(allocationProperties, uncached, readOnly, deviceOnlyVisibilty, 0);
                    EXPECT_FALSE(allocationProperties.flags.flushL3RequiredForRead);
                    EXPECT_FALSE(allocationProperties.flags.flushL3RequiredForWrite);
                } else {
                    allocationProperties.flags.flushL3RequiredForRead = false;
                    allocationProperties.flags.flushL3RequiredForWrite = false;
                    MemoryPropertiesHelper::fillCachePolicyInProperties(allocationProperties, uncached, readOnly, deviceOnlyVisibilty, 0);
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
        EXPECT_TRUE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                    MemoryPropertiesHelper::ObjType::UNKNOWN, context));
        EXPECT_EQ(testInput.expectedResult, flags);
    }
}

TEST_F(MemoryPropertiesHelperTests, givenDmaBufWhenParsePropertiesThenHandleIsSet) {
    cl_mem_properties_intel properties[] = {
        CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR,
        0x1234u,
        0};

    EXPECT_TRUE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                MemoryPropertiesHelper::ObjType::BUFFER, context));

    EXPECT_EQ(memoryProperties.handle, 0x1234u);
}

TEST_F(MemoryPropertiesHelperTests, WhenAdjustingDeviceBitfieldThenCorrectBitfieldIsReturned) {
    UltClDeviceFactory deviceFactory{2, 4};
    auto memoryPropertiesRootDevice0 = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &deviceFactory.rootDevices[0]->getDevice());
    auto memoryPropertiesRootDevice0Tile0 = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &deviceFactory.subDevices[0]->getDevice());
    auto memoryPropertiesRootDevice0Tile1 = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &deviceFactory.subDevices[1]->getDevice());
    auto memoryPropertiesRootDevice1 = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &deviceFactory.rootDevices[1]->getDevice());
    auto memoryPropertiesRootDevice1Tile0 = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &deviceFactory.subDevices[4]->getDevice());
    auto memoryPropertiesRootDevice1Tile1 = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &deviceFactory.subDevices[5]->getDevice());

    DeviceBitfield devicesInContextBitfield0001{0b1};
    DeviceBitfield devicesInContextBitfield0101{0b101};
    DeviceBitfield devicesInContextBitfield1010{0b1010};
    DeviceBitfield devicesInContextBitfield1111{0b1111};

    MemoryProperties memoryPropertiesToProcess[] = {
        memoryPropertiesRootDevice0, memoryPropertiesRootDevice0Tile0, memoryPropertiesRootDevice0Tile1,
        memoryPropertiesRootDevice1, memoryPropertiesRootDevice1Tile0, memoryPropertiesRootDevice1Tile1};

    DeviceBitfield devicesInContextBitfields[] = {devicesInContextBitfield0001, devicesInContextBitfield0101,
                                                  devicesInContextBitfield1010, devicesInContextBitfield1111};
    uint32_t rootDevicesToProcess[] = {0, 1, 2};

    EXPECT_EQ(0b1u, MemoryPropertiesHelper::adjustDeviceBitfield(0, memoryPropertiesRootDevice0Tile0, devicesInContextBitfield1111).to_ulong());
    EXPECT_EQ(0b10u, MemoryPropertiesHelper::adjustDeviceBitfield(0, memoryPropertiesRootDevice0Tile1, devicesInContextBitfield1111).to_ulong());
    EXPECT_EQ(0b1111u, MemoryPropertiesHelper::adjustDeviceBitfield(1, memoryPropertiesRootDevice0Tile0, devicesInContextBitfield1111).to_ulong());
    EXPECT_EQ(0b1111u, MemoryPropertiesHelper::adjustDeviceBitfield(1, memoryPropertiesRootDevice0Tile1, devicesInContextBitfield1111).to_ulong());

    EXPECT_EQ(0b101u, MemoryPropertiesHelper::adjustDeviceBitfield(0, memoryPropertiesRootDevice0, devicesInContextBitfield0101).to_ulong());
    EXPECT_EQ(0b1010u, MemoryPropertiesHelper::adjustDeviceBitfield(0, memoryPropertiesRootDevice0, devicesInContextBitfield1010).to_ulong());
    EXPECT_EQ(0b1111u, MemoryPropertiesHelper::adjustDeviceBitfield(0, memoryPropertiesRootDevice0, devicesInContextBitfield1111).to_ulong());

    for (auto processedRootDevice : rootDevicesToProcess) {
        for (auto devicesInContextBitfield : devicesInContextBitfields) {
            for (auto &memoryProperties : memoryPropertiesToProcess) {
                auto expectedDeviceBitfield = devicesInContextBitfield;
                if (processedRootDevice == memoryProperties.pDevice->getRootDeviceIndex()) {
                    expectedDeviceBitfield &= memoryProperties.pDevice->getDeviceBitfield();
                }
                auto adjustedDeviceBitfield = MemoryPropertiesHelper::adjustDeviceBitfield(
                    processedRootDevice, memoryProperties, devicesInContextBitfield);
                EXPECT_EQ(expectedDeviceBitfield, adjustedDeviceBitfield);
            }
        }
    }
}

TEST_F(MemoryPropertiesHelperTests, WhenCallingGetInitialPlacementThenCorrectValueIsReturned) {
    MemoryProperties memoryProperties{};
    EXPECT_EQ(GraphicsAllocation::UsmInitialPlacement::CPU, MemoryPropertiesHelper::getUSMInitialPlacement(memoryProperties));

    memoryProperties.allocFlags.usmInitialPlacementCpu = false;
    memoryProperties.allocFlags.usmInitialPlacementGpu = false;
    EXPECT_EQ(GraphicsAllocation::UsmInitialPlacement::CPU, MemoryPropertiesHelper::getUSMInitialPlacement(memoryProperties));

    memoryProperties.allocFlags.usmInitialPlacementCpu = false;
    memoryProperties.allocFlags.usmInitialPlacementGpu = true;
    EXPECT_EQ(GraphicsAllocation::UsmInitialPlacement::GPU, MemoryPropertiesHelper::getUSMInitialPlacement(memoryProperties));

    memoryProperties.allocFlags.usmInitialPlacementCpu = true;
    memoryProperties.allocFlags.usmInitialPlacementGpu = false;
    EXPECT_EQ(GraphicsAllocation::UsmInitialPlacement::CPU, MemoryPropertiesHelper::getUSMInitialPlacement(memoryProperties));

    memoryProperties.allocFlags.usmInitialPlacementCpu = true;
    memoryProperties.allocFlags.usmInitialPlacementGpu = true;
    EXPECT_EQ(GraphicsAllocation::UsmInitialPlacement::CPU, MemoryPropertiesHelper::getUSMInitialPlacement(memoryProperties));
}

TEST_F(MemoryPropertiesHelperTests, givenUsmInitialPlacementSetWhenCallingHasInitialPlacementCpuThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    MemoryProperties memoryProperties{};

    for (auto intialPlacement : {-1, 0, 1}) {
        DebugManager.flags.UsmInitialPlacement.set(intialPlacement);
        if (intialPlacement == 1) {
            EXPECT_EQ(GraphicsAllocation::UsmInitialPlacement::GPU, MemoryPropertiesHelper::getUSMInitialPlacement(memoryProperties));
        } else {
            EXPECT_EQ(GraphicsAllocation::UsmInitialPlacement::CPU, MemoryPropertiesHelper::getUSMInitialPlacement(memoryProperties));
        }
    }
}

TEST_F(MemoryPropertiesHelperTests, WhenCallingSetInitialPlacementThenCorrectValueIsSetInAllocationProperties) {
    AllocationProperties allocationProperties{mockRootDeviceIndex, 0, AllocationType::UNIFIED_SHARED_MEMORY, mockDeviceBitfield};

    for (auto initialPlacement : {GraphicsAllocation::UsmInitialPlacement::CPU, GraphicsAllocation::UsmInitialPlacement::GPU}) {
        MemoryPropertiesHelper::setUSMInitialPlacement(allocationProperties, initialPlacement);
        EXPECT_EQ(initialPlacement, allocationProperties.usmInitialPlacement);
    }
}

TEST_F(MemoryPropertiesHelperTests, givenDeviceSpecifiedMultipleTimesWhenParsingExtraMemoryPropertiesThenFalseIsReturned) {
    cl_mem_properties_intel propertiesToTest[][5] = {
        {CL_MEM_DEVICE_ID_INTEL, subDevice0Id, CL_MEM_DEVICE_ID_INTEL, subDevice0Id, 0},
        {CL_MEM_DEVICE_ID_INTEL, subDevice0Id, CL_MEM_DEVICE_ID_INTEL, subDevice1Id, 0}};

    for (auto properties : propertiesToTest) {
        EXPECT_FALSE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                     MemoryPropertiesHelper::ObjType::UNKNOWN, context));
    }
}

TEST_F(MemoryPropertiesHelperTests, givenInvalidDeviceIdWhenParsingExtraMemoryPropertiesThenFalseIsReturned) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_DEVICE_ID_INTEL, rootDeviceId + 1,
        0};

    EXPECT_FALSE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                 MemoryPropertiesHelper::ObjType::UNKNOWN, context));
}

TEST_F(MemoryPropertiesHelperTests, givenRootDeviceIdWhenParsingExtraMemoryPropertiesThenValuesAreProperlySet) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_DEVICE_ID_INTEL, rootDeviceId,
        0};

    EXPECT_TRUE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                MemoryPropertiesHelper::ObjType::UNKNOWN, context));
    EXPECT_EQ(0b11u, memoryProperties.pDevice->getDeviceBitfield().to_ulong());
    EXPECT_EQ(&context.pRootDevice->getDevice(), memoryProperties.pDevice);
}

TEST_F(MemoryPropertiesHelperTests, givenSubDeviceIdWhenParsingExtraMemoryPropertiesThenValuesAreProperlySet) {
    cl_mem_properties_intel properties[] = {
        CL_MEM_DEVICE_ID_INTEL, subDevice1Id,
        0};

    EXPECT_TRUE(ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocflags,
                                                                MemoryPropertiesHelper::ObjType::UNKNOWN, context));
    EXPECT_EQ(0b10u, memoryProperties.pDevice->getDeviceBitfield().to_ulong());
    EXPECT_EQ(&context.pSubDevice1->getDevice(), memoryProperties.pDevice);
}
