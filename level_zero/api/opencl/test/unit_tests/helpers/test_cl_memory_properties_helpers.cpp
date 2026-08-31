/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/driver/driver_handle.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <memory>

namespace NEO {
namespace LEO {
namespace ult {

struct CreateMemoryPropertiesFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        neoDevice = &clDevice->getDevice();
    }

    MemoryProperties fromFlags(cl_mem_flags flags) {
        return ClMemoryPropertiesHelper::createMemoryProperties(flags, 0, 0, neoDevice);
    }

    MemoryProperties fromFlagsIntel(cl_mem_flags_intel flagsIntel) {
        return ClMemoryPropertiesHelper::createMemoryProperties(0, flagsIntel, 0, neoDevice);
    }

    MemoryProperties fromAllocFlags(cl_mem_alloc_flags_intel allocFlags) {
        return ClMemoryPropertiesHelper::createMemoryProperties(0, 0, allocFlags, neoDevice);
    }

    ClDevice *clDevice = nullptr;
    NEO::Device *neoDevice = nullptr;
};

TEST_F(CreateMemoryPropertiesFixture, givenNoFlagsWhenCreatingMemoryPropertiesThenNothingIsSetAndDeviceIsStored) {
    auto properties = fromFlags(0);

    EXPECT_EQ(0u, properties.allFlags);
    EXPECT_EQ(0u, properties.allAllocFlags);
    EXPECT_EQ(neoDevice, properties.pDevice);
    EXPECT_EQ(0u, properties.handle);
    EXPECT_EQ(0u, properties.handleType);
    EXPECT_EQ(0u, properties.hostptr);
}

TEST_F(CreateMemoryPropertiesFixture, givenAccessFlagsWhenCreatingMemoryPropertiesThenMatchingBitIsSet) {
    EXPECT_TRUE(fromFlags(CL_MEM_READ_WRITE).flags.readWrite);
    EXPECT_TRUE(fromFlags(CL_MEM_WRITE_ONLY).flags.writeOnly);
    EXPECT_TRUE(fromFlags(CL_MEM_READ_ONLY).flags.readOnly);

    EXPECT_FALSE(fromFlags(CL_MEM_READ_WRITE).flags.readOnly);
    EXPECT_FALSE(fromFlags(CL_MEM_READ_WRITE).flags.writeOnly);
    EXPECT_FALSE(fromFlags(CL_MEM_READ_ONLY).flags.readWrite);
}

TEST_F(CreateMemoryPropertiesFixture, givenHostPtrFlagsWhenCreatingMemoryPropertiesThenMatchingBitIsSet) {
    EXPECT_TRUE(fromFlags(CL_MEM_USE_HOST_PTR).flags.useHostPtr);
    EXPECT_TRUE(fromFlags(CL_MEM_ALLOC_HOST_PTR).flags.allocHostPtr);
    EXPECT_TRUE(fromFlags(CL_MEM_COPY_HOST_PTR).flags.copyHostPtr);

    EXPECT_FALSE(fromFlags(CL_MEM_USE_HOST_PTR).flags.allocHostPtr);
    EXPECT_FALSE(fromFlags(CL_MEM_ALLOC_HOST_PTR).flags.copyHostPtr);
}

TEST_F(CreateMemoryPropertiesFixture, givenHostAccessFlagsWhenCreatingMemoryPropertiesThenMatchingBitIsSet) {
    EXPECT_TRUE(fromFlags(CL_MEM_HOST_WRITE_ONLY).flags.hostWriteOnly);
    EXPECT_TRUE(fromFlags(CL_MEM_HOST_READ_ONLY).flags.hostReadOnly);
    EXPECT_TRUE(fromFlags(CL_MEM_HOST_NO_ACCESS).flags.hostNoAccess);

    EXPECT_FALSE(fromFlags(CL_MEM_HOST_WRITE_ONLY).flags.hostReadOnly);
    EXPECT_FALSE(fromFlags(CL_MEM_HOST_READ_ONLY).flags.hostNoAccess);
}

TEST_F(CreateMemoryPropertiesFixture, givenKernelReadAndWriteFlagWhenCreatingMemoryPropertiesThenBitIsSet) {
    EXPECT_TRUE(fromFlags(CL_MEM_KERNEL_READ_AND_WRITE).flags.kernelReadAndWrite);
    EXPECT_FALSE(fromFlags(0).flags.kernelReadAndWrite);
}

TEST_F(CreateMemoryPropertiesFixture, givenAccessFlagsUnrestrictedWhenCreatingMemoryPropertiesThenBitIsSet) {
    EXPECT_TRUE(fromFlags(CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL).flags.accessFlagsUnrestricted);
    EXPECT_FALSE(fromFlagsIntel(CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL).flags.accessFlagsUnrestricted);
}

TEST_F(CreateMemoryPropertiesFixture, givenNoAccessFlagWhenCreatingMemoryPropertiesThenBitIsSet) {
    EXPECT_TRUE(fromFlags(CL_MEM_NO_ACCESS_INTEL).flags.noAccess);
    EXPECT_FALSE(fromFlags(0).flags.noAccess);
}

TEST_F(CreateMemoryPropertiesFixture, givenForceHostMemoryFlagWhenCreatingMemoryPropertiesThenBitIsSet) {
    EXPECT_TRUE(fromFlags(CL_MEM_FORCE_HOST_MEMORY_INTEL).flags.forceHostMemory);
    EXPECT_FALSE(fromFlagsIntel(CL_MEM_FORCE_HOST_MEMORY_INTEL).flags.forceHostMemory);
}

TEST_F(CreateMemoryPropertiesFixture, givenForceLinearStorageFlagInEitherFlagSetWhenCreatingMemoryPropertiesThenBitIsSet) {
    EXPECT_TRUE(fromFlags(CL_MEM_FORCE_LINEAR_STORAGE_INTEL).flags.forceLinearStorage);
    EXPECT_TRUE(fromFlagsIntel(CL_MEM_FORCE_LINEAR_STORAGE_INTEL).flags.forceLinearStorage);
    EXPECT_FALSE(fromFlags(0).flags.forceLinearStorage);
}

TEST_F(CreateMemoryPropertiesFixture, givenAllowUnrestrictedSizeFlagInEitherFlagSetWhenCreatingMemoryPropertiesThenBitIsSet) {
    EXPECT_TRUE(fromFlags(CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL).flags.allowUnrestrictedSize);
    EXPECT_TRUE(fromFlagsIntel(CL_MEM_ALLOW_UNRESTRICTED_SIZE_INTEL).flags.allowUnrestrictedSize);
    EXPECT_FALSE(fromFlags(0).flags.allowUnrestrictedSize);
}

TEST_F(CreateMemoryPropertiesFixture, givenAllowUnrestrictedSizeDebugFlagWhenCreatingMemoryPropertiesThenBitIsSetWithoutAnyClFlag) {
    DebugManagerStateRestore restorer;
    debugManager.flags.AllowUnrestrictedSize.set(true);

    EXPECT_TRUE(fromFlags(0).flags.allowUnrestrictedSize);
}

TEST_F(CreateMemoryPropertiesFixture, givenCompressionHintsInEitherFlagSetWhenCreatingMemoryPropertiesThenBitIsSet) {
    EXPECT_TRUE(fromFlags(CL_MEM_COMPRESSED_HINT_INTEL).flags.compressedHint);
    EXPECT_TRUE(fromFlagsIntel(CL_MEM_COMPRESSED_HINT_INTEL).flags.compressedHint);
    EXPECT_TRUE(fromFlags(CL_MEM_UNCOMPRESSED_HINT_INTEL).flags.uncompressedHint);
    EXPECT_TRUE(fromFlagsIntel(CL_MEM_UNCOMPRESSED_HINT_INTEL).flags.uncompressedHint);

    EXPECT_FALSE(fromFlags(CL_MEM_COMPRESSED_HINT_INTEL).flags.uncompressedHint);
    EXPECT_FALSE(fromFlags(CL_MEM_UNCOMPRESSED_HINT_INTEL).flags.compressedHint);
}

TEST_F(CreateMemoryPropertiesFixture, givenBothCompressionHintsWhenCreatingMemoryPropertiesThenBothBitsAreSet) {
    auto properties = fromFlags(CL_MEM_COMPRESSED_HINT_INTEL | CL_MEM_UNCOMPRESSED_HINT_INTEL);

    EXPECT_TRUE(properties.flags.compressedHint);
    EXPECT_TRUE(properties.flags.uncompressedHint);
}

TEST_F(CreateMemoryPropertiesFixture, givenIntelOnlyFlagsWhenCreatingMemoryPropertiesThenMatchingBitIsSet) {
    EXPECT_TRUE(fromFlagsIntel(CL_MEM_LOCALLY_UNCACHED_RESOURCE).flags.locallyUncachedResource);
    EXPECT_TRUE(fromFlagsIntel(CL_MEM_LOCALLY_UNCACHED_SURFACE_STATE_RESOURCE).flags.locallyUncachedInSurfaceState);
    EXPECT_TRUE(fromFlagsIntel(CL_MEM_48BIT_RESOURCE_INTEL).flags.resource48Bit);

    EXPECT_FALSE(fromFlags(CL_MEM_LOCALLY_UNCACHED_RESOURCE).flags.locallyUncachedResource);
    EXPECT_FALSE(fromFlags(CL_MEM_48BIT_RESOURCE_INTEL).flags.resource48Bit);
}

TEST_F(CreateMemoryPropertiesFixture, givenAllocFlagsWhenCreatingMemoryPropertiesThenMatchingAllocBitIsSet) {
    EXPECT_TRUE(fromAllocFlags(CL_MEM_ALLOC_WRITE_COMBINED_INTEL).allocFlags.allocWriteCombined);
    EXPECT_TRUE(fromAllocFlags(CL_MEM_ALLOC_INITIAL_PLACEMENT_DEVICE_INTEL).allocFlags.usmInitialPlacementGpu);
    EXPECT_TRUE(fromAllocFlags(CL_MEM_ALLOC_INITIAL_PLACEMENT_HOST_INTEL).allocFlags.usmInitialPlacementCpu);

    EXPECT_FALSE(fromAllocFlags(CL_MEM_ALLOC_WRITE_COMBINED_INTEL).allocFlags.usmInitialPlacementGpu);
    EXPECT_FALSE(fromAllocFlags(CL_MEM_ALLOC_INITIAL_PLACEMENT_DEVICE_INTEL).allocFlags.usmInitialPlacementCpu);
}

TEST_F(CreateMemoryPropertiesFixture, givenBothInitialPlacementAllocFlagsWhenCreatingMemoryPropertiesThenBothBitsAreSet) {
    auto properties = fromAllocFlags(CL_MEM_ALLOC_INITIAL_PLACEMENT_DEVICE_INTEL | CL_MEM_ALLOC_INITIAL_PLACEMENT_HOST_INTEL);

    EXPECT_TRUE(properties.allocFlags.usmInitialPlacementGpu);
    EXPECT_TRUE(properties.allocFlags.usmInitialPlacementCpu);
}

TEST_F(CreateMemoryPropertiesFixture, givenAllocFlagsWhenCreatingMemoryPropertiesThenRegularFlagsStayUntouched) {
    auto properties = fromAllocFlags(CL_MEM_ALLOC_WRITE_COMBINED_INTEL);

    EXPECT_EQ(0u, properties.allFlags);
    EXPECT_NE(0u, properties.allAllocFlags);
}

TEST_F(CreateMemoryPropertiesFixture, givenSeveralFlagsAtOnceWhenCreatingMemoryPropertiesThenAllMatchingBitsAreSet) {
    auto properties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR | CL_MEM_HOST_NO_ACCESS,
                                                                       CL_MEM_LOCALLY_UNCACHED_RESOURCE | CL_MEM_48BIT_RESOURCE_INTEL,
                                                                       CL_MEM_ALLOC_WRITE_COMBINED_INTEL,
                                                                       neoDevice);

    EXPECT_TRUE(properties.flags.readOnly);
    EXPECT_TRUE(properties.flags.useHostPtr);
    EXPECT_TRUE(properties.flags.hostNoAccess);
    EXPECT_TRUE(properties.flags.locallyUncachedResource);
    EXPECT_TRUE(properties.flags.resource48Bit);
    EXPECT_TRUE(properties.allocFlags.allocWriteCombined);
    EXPECT_FALSE(properties.flags.readWrite);
    EXPECT_FALSE(properties.flags.writeOnly);
}

TEST_F(CreateMemoryPropertiesFixture, givenNullDeviceWhenCreatingMemoryPropertiesThenNullIsStored) {
    auto properties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, nullptr);

    EXPECT_EQ(nullptr, properties.pDevice);
    EXPECT_TRUE(properties.flags.readWrite);
}

TEST_F(CreateMemoryPropertiesFixture, givenUnknownFlagBitsWhenCreatingMemoryPropertiesThenTheyAreIgnored) {
    auto properties = ClMemoryPropertiesHelper::createMemoryProperties(1ull << 62, 1ull << 61, 1ull << 60, neoDevice);

    EXPECT_EQ(0u, properties.allFlags);
    EXPECT_EQ(0u, properties.allAllocFlags);
}

struct ParseMemoryPropertiesFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        capturingContext = std::make_unique<CapturingContext>(driverHandle.get(), clDevice->getL0Handle());
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
    }

    void TearDown() override {
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    bool parse(const cl_mem_properties_intel *properties, ClMemoryPropertiesHelper::ObjType objType = ClMemoryPropertiesHelper::ObjType::buffer) {
        return ClMemoryPropertiesHelper::parseMemoryProperties(properties, memoryProperties, flags, flagsIntel, allocFlags, objType, *leoContext);
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;

    MemoryProperties memoryProperties{};
    cl_mem_flags flags = 0;
    cl_mem_flags_intel flagsIntel = 0;
    cl_mem_alloc_flags_intel allocFlags = 0;
};

TEST_F(ParseMemoryPropertiesFixture, givenNullPropertiesWhenParsingThenSucceedsAndFlagsAreUnchanged) {
    EXPECT_TRUE(parse(nullptr));

    EXPECT_EQ(0u, flags);
    EXPECT_EQ(0u, flagsIntel);
    EXPECT_EQ(0u, allocFlags);
    EXPECT_EQ(&clDevice->getDevice(), memoryProperties.pDevice);
}

TEST_F(ParseMemoryPropertiesFixture, givenEmptyPropertiesWhenParsingThenSucceedsAndFlagsAreUnchanged) {
    cl_mem_properties_intel properties[] = {0};

    EXPECT_TRUE(parse(properties));
    EXPECT_EQ(0u, flags);
    EXPECT_EQ(0u, memoryProperties.allFlags);
}

TEST_F(ParseMemoryPropertiesFixture, givenMemFlagsPropertyWhenParsingThenFlagsAreAccumulatedAndReflectedInMemoryProperties) {
    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS, CL_MEM_READ_ONLY, 0};

    ASSERT_TRUE(parse(properties));

    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_ONLY), flags);
    EXPECT_TRUE(memoryProperties.flags.readOnly);
}

TEST_F(ParseMemoryPropertiesFixture, givenMemFlagsPropertyWhenParsingWithPreexistingFlagsThenTheyAreCombined) {
    flags = CL_MEM_USE_HOST_PTR;
    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS, CL_MEM_READ_ONLY, 0};

    ASSERT_TRUE(parse(properties));

    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR), flags);
    EXPECT_TRUE(memoryProperties.flags.readOnly);
    EXPECT_TRUE(memoryProperties.flags.useHostPtr);
}

TEST_F(ParseMemoryPropertiesFixture, givenRepeatedMemFlagsPropertyWhenParsingThenAllEntriesAreAccumulated) {
    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS, CL_MEM_READ_ONLY, CL_MEM_FLAGS, CL_MEM_HOST_NO_ACCESS, 0};

    ASSERT_TRUE(parse(properties));

    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS), flags);
    EXPECT_TRUE(memoryProperties.flags.readOnly);
    EXPECT_TRUE(memoryProperties.flags.hostNoAccess);
}

TEST_F(ParseMemoryPropertiesFixture, givenIntelFlagsPropertyWhenParsingThenIntelFlagsAreAccumulated) {
    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS_INTEL, CL_MEM_LOCALLY_UNCACHED_RESOURCE, 0};

    ASSERT_TRUE(parse(properties));

    EXPECT_EQ(static_cast<cl_mem_flags_intel>(CL_MEM_LOCALLY_UNCACHED_RESOURCE), flagsIntel);
    EXPECT_EQ(0u, flags);
    EXPECT_TRUE(memoryProperties.flags.locallyUncachedResource);
}

TEST_F(ParseMemoryPropertiesFixture, givenAllocFlagsPropertyWhenParsingThenAllocFlagsAreAccumulated) {
    cl_mem_properties_intel properties[] = {CL_MEM_ALLOC_FLAGS_INTEL, CL_MEM_ALLOC_WRITE_COMBINED_INTEL, 0};

    ASSERT_TRUE(parse(properties));

    EXPECT_EQ(static_cast<cl_mem_alloc_flags_intel>(CL_MEM_ALLOC_WRITE_COMBINED_INTEL), allocFlags);
    EXPECT_TRUE(memoryProperties.allocFlags.allocWriteCombined);
}

TEST_F(ParseMemoryPropertiesFixture, givenAllThreeFlagKindsWhenParsingThenEachLandsInItsOwnBucket) {
    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS, CL_MEM_WRITE_ONLY,
                                            CL_MEM_FLAGS_INTEL, CL_MEM_48BIT_RESOURCE_INTEL,
                                            CL_MEM_ALLOC_FLAGS_INTEL, CL_MEM_ALLOC_INITIAL_PLACEMENT_DEVICE_INTEL,
                                            0};

    ASSERT_TRUE(parse(properties));

    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_WRITE_ONLY), flags);
    EXPECT_EQ(static_cast<cl_mem_flags_intel>(CL_MEM_48BIT_RESOURCE_INTEL), flagsIntel);
    EXPECT_EQ(static_cast<cl_mem_alloc_flags_intel>(CL_MEM_ALLOC_INITIAL_PLACEMENT_DEVICE_INTEL), allocFlags);
    EXPECT_TRUE(memoryProperties.flags.writeOnly);
    EXPECT_TRUE(memoryProperties.flags.resource48Bit);
    EXPECT_TRUE(memoryProperties.allocFlags.usmInitialPlacementGpu);
}

TEST_F(ParseMemoryPropertiesFixture, givenUseHostPtrPropertyWhenParsingThenHostPtrIsStoredInMemoryProperties) {
    uint64_t hostStorage = 0u;
    cl_mem_properties_intel properties[] = {CL_MEM_ALLOC_USE_HOST_PTR_INTEL, reinterpret_cast<cl_mem_properties_intel>(&hostStorage), 0};

    ASSERT_TRUE(parse(properties));

    EXPECT_EQ(reinterpret_cast<uintptr_t>(&hostStorage), memoryProperties.hostptr);
}

TEST_F(ParseMemoryPropertiesFixture, givenNoUseHostPtrPropertyWhenParsingThenHostPtrStaysZero) {
    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS, CL_MEM_READ_WRITE, 0};

    ASSERT_TRUE(parse(properties));

    EXPECT_EQ(0u, memoryProperties.hostptr);
}

TEST_F(ParseMemoryPropertiesFixture, givenAcceptedButUnusedPropertiesWhenParsingThenTheyAreSkippedWithoutFailing) {
    cl_mem_properties_intel properties[] = {CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR, 0x10,
                                            CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR, 0x20,
                                            CL_MEM_DEVICE_HANDLE_LIST_KHR, 0x30,
                                            CL_MEM_DEVICE_ID_INTEL_DEPRECATED, 0x40,
                                            CL_MEM_DEVICE_ID_INTEL, 0x50,
                                            CL_L0_MEM_OBJ_HANDLE, 0x60,
                                            0};

    EXPECT_TRUE(parse(properties));

    EXPECT_EQ(0u, flags);
    EXPECT_EQ(0u, memoryProperties.handle);
    EXPECT_EQ(0u, memoryProperties.handleType);
    EXPECT_TRUE(memoryProperties.associatedDevices.empty());
}

TEST_F(ParseMemoryPropertiesFixture, givenUnknownPropertyWhenParsingThenReturnsFalse) {
    cl_mem_properties_intel properties[] = {0xDEAD, 0x1, 0};

    EXPECT_FALSE(parse(properties));
}

TEST_F(ParseMemoryPropertiesFixture, givenUnknownPropertyAfterValidOnesWhenParsingThenReturnsFalseAndAlreadySeenFlagsStay) {
    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS, CL_MEM_READ_ONLY, 0xDEAD, 0x1, 0};

    EXPECT_FALSE(parse(properties));
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_ONLY), flags);
}

TEST_F(ParseMemoryPropertiesFixture, givenUnknownPropertyWhenParsingThenMemoryPropertiesAreNotProduced) {
    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS, CL_MEM_READ_ONLY, 0xDEAD, 0x1, 0};

    ASSERT_FALSE(parse(properties));
    EXPECT_EQ(nullptr, memoryProperties.pDevice);
    EXPECT_EQ(0u, memoryProperties.allFlags);
}

TEST_F(ParseMemoryPropertiesFixture, givenImageObjectTypeWhenParsingThenPropertiesAreParsedTheSameWay) {
    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS, CL_MEM_READ_ONLY, 0};

    ASSERT_TRUE(parse(properties, ClMemoryPropertiesHelper::ObjType::image));

    EXPECT_TRUE(memoryProperties.flags.readOnly);
    EXPECT_EQ(&clDevice->getDevice(), memoryProperties.pDevice);
}

TEST_F(ParseMemoryPropertiesFixture, givenUnknownObjectTypeWhenParsingThenPropertiesAreParsedTheSameWay) {
    cl_mem_properties_intel properties[] = {CL_MEM_FLAGS, CL_MEM_WRITE_ONLY, 0};

    ASSERT_TRUE(parse(properties, ClMemoryPropertiesHelper::ObjType::unknown));

    EXPECT_TRUE(memoryProperties.flags.writeOnly);
}

TEST_F(ParseMemoryPropertiesFixture, givenContextDeviceWhenParsingThenMemoryPropertiesPointAtTheContextDevice) {
    ASSERT_TRUE(parse(nullptr));

    EXPECT_EQ(&clDevice->getDevice(), memoryProperties.pDevice);
    EXPECT_EQ(clDevice->getRootDeviceIndex(), memoryProperties.pDevice->getRootDeviceIndex());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
