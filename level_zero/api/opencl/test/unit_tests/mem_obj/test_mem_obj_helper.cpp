/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/memory_properties_helpers.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/mem_obj/leo_mem_obj_helper.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(MemObjHelperSubBufferFlagsTests, givenAllowedFlagsWhenCheckMemFlagsForSubBufferThenReturnsTrue) {
    EXPECT_TRUE(MemObjHelper::checkMemFlagsForSubBuffer(0));
    EXPECT_TRUE(MemObjHelper::checkMemFlagsForSubBuffer(CL_MEM_READ_WRITE));
    EXPECT_TRUE(MemObjHelper::checkMemFlagsForSubBuffer(CL_MEM_WRITE_ONLY));
    EXPECT_TRUE(MemObjHelper::checkMemFlagsForSubBuffer(CL_MEM_READ_ONLY));
    EXPECT_TRUE(MemObjHelper::checkMemFlagsForSubBuffer(CL_MEM_HOST_WRITE_ONLY));
    EXPECT_TRUE(MemObjHelper::checkMemFlagsForSubBuffer(CL_MEM_HOST_READ_ONLY));
    EXPECT_TRUE(MemObjHelper::checkMemFlagsForSubBuffer(CL_MEM_HOST_NO_ACCESS));
    EXPECT_TRUE(MemObjHelper::checkMemFlagsForSubBuffer(CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS));
}

TEST(MemObjHelperSubBufferFlagsTests, givenDisallowedFlagsWhenCheckMemFlagsForSubBufferThenReturnsFalse) {
    EXPECT_FALSE(MemObjHelper::checkMemFlagsForSubBuffer(CL_MEM_USE_HOST_PTR));
    EXPECT_FALSE(MemObjHelper::checkMemFlagsForSubBuffer(CL_MEM_ALLOC_HOST_PTR));
    EXPECT_FALSE(MemObjHelper::checkMemFlagsForSubBuffer(CL_MEM_COPY_HOST_PTR));
    EXPECT_FALSE(MemObjHelper::checkMemFlagsForSubBuffer(CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR));
}

TEST(MemObjHelperSvmPropertiesTests, givenNoFlagsWhenGetSvmAllocationPropertiesThenAllPropertiesAreFalse) {
    auto properties = MemObjHelper::getSvmAllocationProperties(0);
    EXPECT_FALSE(properties.coherent);
    EXPECT_FALSE(properties.hostPtrReadOnly);
    EXPECT_FALSE(properties.readOnly);
}

TEST(MemObjHelperSvmPropertiesTests, givenFineGrainBufferFlagWhenGetSvmAllocationPropertiesThenCoherentIsSet) {
    auto properties = MemObjHelper::getSvmAllocationProperties(CL_MEM_SVM_FINE_GRAIN_BUFFER);
    EXPECT_TRUE(properties.coherent);
    EXPECT_FALSE(properties.hostPtrReadOnly);
    EXPECT_FALSE(properties.readOnly);
}

TEST(MemObjHelperSvmPropertiesTests, givenHostReadOnlyFlagWhenGetSvmAllocationPropertiesThenHostPtrReadOnlyIsSet) {
    auto properties = MemObjHelper::getSvmAllocationProperties(CL_MEM_HOST_READ_ONLY);
    EXPECT_TRUE(properties.hostPtrReadOnly);
    EXPECT_FALSE(properties.readOnly);
}

TEST(MemObjHelperSvmPropertiesTests, givenHostNoAccessFlagWhenGetSvmAllocationPropertiesThenHostPtrReadOnlyIsSet) {
    auto properties = MemObjHelper::getSvmAllocationProperties(CL_MEM_HOST_NO_ACCESS);
    EXPECT_TRUE(properties.hostPtrReadOnly);
}

TEST(MemObjHelperSvmPropertiesTests, givenReadOnlyFlagWhenGetSvmAllocationPropertiesThenReadOnlyIsSet) {
    auto properties = MemObjHelper::getSvmAllocationProperties(CL_MEM_READ_ONLY);
    EXPECT_TRUE(properties.readOnly);
    EXPECT_FALSE(properties.hostPtrReadOnly);
}

TEST(MemObjHelperSvmPropertiesTests, givenAllRelevantFlagsWhenGetSvmAllocationPropertiesThenAllPropertiesAreSet) {
    auto properties = MemObjHelper::getSvmAllocationProperties(CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_HOST_READ_ONLY | CL_MEM_READ_ONLY);
    EXPECT_TRUE(properties.coherent);
    EXPECT_TRUE(properties.hostPtrReadOnly);
    EXPECT_TRUE(properties.readOnly);
}

struct LeoMemObjHelperFixtureTest : public Test<OclFixture> {
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

    MemoryProperties createProperties(cl_mem_flags flags, cl_mem_flags_intel flagsIntel = 0) {
        return ClMemoryPropertiesHelper::createMemoryProperties(flags, flagsIntel, 0, &clDevice->getDevice());
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;
};

TEST_F(LeoMemObjHelperFixtureTest, givenValidFlagsWhenValidateMemoryPropertiesForBufferThenReturnsTrue) {
    auto properties = createProperties(CL_MEM_READ_WRITE);
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(properties, CL_MEM_READ_WRITE, 0, *leoContext));
}

TEST_F(LeoMemObjHelperFixtureTest, givenConflictingAccessFlagsWhenValidateMemoryPropertiesForBufferThenReturnsFalse) {
    const cl_mem_flags conflictingCombinations[] = {
        CL_MEM_READ_WRITE | CL_MEM_READ_ONLY,
        CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY,
        CL_MEM_READ_ONLY | CL_MEM_WRITE_ONLY,
        CL_MEM_ALLOC_HOST_PTR | CL_MEM_USE_HOST_PTR,
        CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR,
        CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS,
        CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_WRITE_ONLY,
        CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS};

    for (auto flags : conflictingCombinations) {
        auto properties = createProperties(flags);
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForBuffer(properties, flags, 0, *leoContext))
            << "unexpectedly accepted flags " << flags;
    }
}

TEST_F(LeoMemObjHelperFixtureTest, givenBothCompressionHintsWhenValidateMemoryPropertiesForBufferThenReturnsFalse) {
    const cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_COMPRESSED_HINT_INTEL | CL_MEM_UNCOMPRESSED_HINT_INTEL;
    auto properties = createProperties(flags);
    EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForBuffer(properties, flags, 0, *leoContext));
}

TEST_F(LeoMemObjHelperFixtureTest, givenCompressionHintsSplitBetweenFlagsAndFlagsIntelWhenValidateMemoryPropertiesForBufferThenReturnsFalse) {
    auto properties = createProperties(CL_MEM_COMPRESSED_HINT_INTEL, CL_MEM_UNCOMPRESSED_HINT_INTEL);
    EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForBuffer(properties, CL_MEM_COMPRESSED_HINT_INTEL,
                                                                 CL_MEM_UNCOMPRESSED_HINT_INTEL, *leoContext));
}

TEST_F(LeoMemObjHelperFixtureTest, givenOnlyCompressedHintWhenValidateMemoryPropertiesForBufferThenReturnsTrue) {
    const cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_COMPRESSED_HINT_INTEL;
    auto properties = createProperties(flags);
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForBuffer(properties, flags, 0, *leoContext));
}

TEST_F(LeoMemObjHelperFixtureTest, givenValidFlagsWhenValidateMemoryPropertiesForImageThenReturnsTrue) {
    auto properties = createProperties(CL_MEM_READ_WRITE);
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, CL_MEM_READ_WRITE, 0, nullptr, *leoContext));
}

TEST_F(LeoMemObjHelperFixtureTest, givenConflictingFlagsWhenValidateMemoryPropertiesForImageThenReturnsFalse) {
    const cl_mem_flags conflictingCombinations[] = {
        CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY,
        CL_MEM_READ_WRITE | CL_MEM_READ_ONLY,
        CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY,
        CL_MEM_ALLOC_HOST_PTR | CL_MEM_USE_HOST_PTR,
        CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR,
        CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY,
        CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS,
        CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS,
        CL_MEM_NO_ACCESS_INTEL | CL_MEM_READ_WRITE,
        CL_MEM_NO_ACCESS_INTEL | CL_MEM_WRITE_ONLY,
        CL_MEM_NO_ACCESS_INTEL | CL_MEM_READ_ONLY};

    for (auto flags : conflictingCombinations) {
        auto properties = createProperties(flags);
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(properties, flags, 0, nullptr, *leoContext))
            << "unexpectedly accepted flags " << flags;
    }
}

TEST_F(LeoMemObjHelperFixtureTest, givenUnrestrictedAccessFlagWhenValidateMemoryPropertiesForImageThenConflictingFlagsAreAccepted) {
    const cl_mem_flags flags = CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL | CL_MEM_READ_WRITE | CL_MEM_READ_ONLY;
    auto properties = createProperties(flags);
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, flags, 0, nullptr, *leoContext));
}

TEST_F(LeoMemObjHelperFixtureTest, givenNullParentWhenValidateMemoryPropertiesForImageThenParentChecksAreSkipped) {
    auto properties = createProperties(CL_MEM_USE_HOST_PTR);
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, CL_MEM_USE_HOST_PTR, 0, nullptr, *leoContext));
}

struct LeoMemObjHelperParentTest : public LeoMemObjHelperFixtureTest {
    Buffer *createBuffer(cl_mem_flags flags) {
        auto memoryProperties = createProperties(flags);
        return new Buffer(leoContext.get(), memoryProperties, flags, &dummyStorage, nullptr, sizeof(dummyStorage), false);
    }

    uint64_t dummyStorage = 0u;
};

TEST_F(LeoMemObjHelperParentTest, givenParentAndHostPtrFlagsWhenValidateMemoryPropertiesForImageThenReturnsFalse) {
    auto parent = createBuffer(CL_MEM_READ_WRITE);

    for (cl_mem_flags flags : {static_cast<cl_mem_flags>(CL_MEM_ALLOC_HOST_PTR),
                               static_cast<cl_mem_flags>(CL_MEM_COPY_HOST_PTR),
                               static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR)}) {
        auto properties = createProperties(flags);
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(properties, flags, 0, parent, *leoContext))
            << "unexpectedly accepted flags " << flags;
    }

    delete parent;
}

TEST_F(LeoMemObjHelperParentTest, givenWriteOnlyParentWhenValidateMemoryPropertiesForImageWithWiderAccessThenReturnsFalse) {
    auto parent = createBuffer(CL_MEM_WRITE_ONLY);

    for (cl_mem_flags flags : {static_cast<cl_mem_flags>(CL_MEM_READ_WRITE), static_cast<cl_mem_flags>(CL_MEM_READ_ONLY)}) {
        auto properties = createProperties(flags);
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(properties, flags, 0, parent, *leoContext))
            << "unexpectedly accepted flags " << flags;
    }

    delete parent;
}

TEST_F(LeoMemObjHelperParentTest, givenReadOnlyParentWhenValidateMemoryPropertiesForImageWithWiderAccessThenReturnsFalse) {
    auto parent = createBuffer(CL_MEM_READ_ONLY);

    for (cl_mem_flags flags : {static_cast<cl_mem_flags>(CL_MEM_READ_WRITE), static_cast<cl_mem_flags>(CL_MEM_WRITE_ONLY)}) {
        auto properties = createProperties(flags);
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(properties, flags, 0, parent, *leoContext))
            << "unexpectedly accepted flags " << flags;
    }

    delete parent;
}

TEST_F(LeoMemObjHelperParentTest, givenHostNoAccessParentWhenValidateMemoryPropertiesForImageWithHostAccessThenReturnsFalse) {
    auto parent = createBuffer(CL_MEM_HOST_NO_ACCESS);

    for (cl_mem_flags flags : {static_cast<cl_mem_flags>(CL_MEM_HOST_WRITE_ONLY), static_cast<cl_mem_flags>(CL_MEM_HOST_READ_ONLY)}) {
        auto properties = createProperties(flags);
        EXPECT_FALSE(MemObjHelper::validateMemoryPropertiesForImage(properties, flags, 0, parent, *leoContext))
            << "unexpectedly accepted flags " << flags;
    }

    delete parent;
}

TEST_F(LeoMemObjHelperParentTest, givenReadWriteParentWhenValidateMemoryPropertiesForImageWithNarrowerAccessThenReturnsTrue) {
    auto parent = createBuffer(CL_MEM_READ_WRITE);

    auto properties = createProperties(CL_MEM_READ_ONLY);
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, CL_MEM_READ_ONLY, 0, parent, *leoContext));

    delete parent;
}

TEST_F(LeoMemObjHelperParentTest, givenParentAndZeroFlagsWhenValidateMemoryPropertiesForImageThenParentChecksAreSkipped) {
    auto parent = createBuffer(CL_MEM_WRITE_ONLY);

    auto properties = createProperties(0);
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, 0, 0, parent, *leoContext));

    delete parent;
}

TEST_F(LeoMemObjHelperParentTest, givenUnrestrictedParentWhenValidateMemoryPropertiesForImageWithWiderAccessThenReturnsTrue) {
    auto parent = createBuffer(CL_MEM_WRITE_ONLY | CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);

    auto properties = createProperties(CL_MEM_READ_ONLY);
    EXPECT_TRUE(MemObjHelper::validateMemoryPropertiesForImage(properties, CL_MEM_READ_ONLY, 0, parent, *leoContext));

    delete parent;
}

TEST_F(LeoMemObjHelperFixtureTest, givenCompressionUnsupportedWhenIsSuitableForCompressionThenReturnsFalse) {
    auto properties = createProperties(CL_MEM_READ_WRITE | CL_MEM_COMPRESSED_HINT_INTEL);
    EXPECT_FALSE(MemObjHelper::isSuitableForCompression(false, properties, *leoContext, true));
    EXPECT_FALSE(MemObjHelper::isSuitableForCompression(false, properties, *leoContext, false));
}

TEST_F(LeoMemObjHelperFixtureTest, givenPreferCompressionAndUncompressedHintWhenIsSuitableForCompressionThenReturnsFalse) {
    auto properties = createProperties(CL_MEM_READ_WRITE | CL_MEM_UNCOMPRESSED_HINT_INTEL);
    ASSERT_TRUE(properties.flags.uncompressedHint);
    EXPECT_FALSE(MemObjHelper::isSuitableForCompression(true, properties, *leoContext, true));
}

TEST_F(LeoMemObjHelperFixtureTest, givenPreferCompressionAndCompressedHintWhenIsSuitableForCompressionThenReturnsTrue) {
    auto properties = createProperties(CL_MEM_READ_WRITE | CL_MEM_COMPRESSED_HINT_INTEL);
    ASSERT_TRUE(properties.flags.compressedHint);
    EXPECT_TRUE(MemObjHelper::isSuitableForCompression(true, properties, *leoContext, true));
}

TEST_F(LeoMemObjHelperFixtureTest, givenPreferCompressionAndNoHintWhenIsSuitableForCompressionThenReturnsTrue) {
    auto properties = createProperties(CL_MEM_READ_WRITE);
    EXPECT_TRUE(MemObjHelper::isSuitableForCompression(true, properties, *leoContext, true));
}

TEST_F(LeoMemObjHelperFixtureTest, givenNoPreferCompressionWhenIsSuitableForCompressionThenFollowsCompressedHint) {
    auto withHint = createProperties(CL_MEM_READ_WRITE | CL_MEM_COMPRESSED_HINT_INTEL);
    EXPECT_TRUE(MemObjHelper::isSuitableForCompression(true, withHint, *leoContext, false));

    auto withoutHint = createProperties(CL_MEM_READ_WRITE);
    EXPECT_FALSE(MemObjHelper::isSuitableForCompression(true, withoutHint, *leoContext, false));

    auto withUncompressedHint = createProperties(CL_MEM_READ_WRITE | CL_MEM_UNCOMPRESSED_HINT_INTEL);
    EXPECT_FALSE(MemObjHelper::isSuitableForCompression(true, withUncompressedHint, *leoContext, false));
}

TEST_F(LeoMemObjHelperFixtureTest, givenFlagTablesWhenInspectedThenBufferAndImageSetsShareTheCommonFlags) {
    EXPECT_EQ(MemObjHelper::commonFlags, MemObjHelper::commonFlags & MemObjHelper::validFlagsForBuffer);
    EXPECT_EQ(MemObjHelper::commonFlags, MemObjHelper::commonFlags & MemObjHelper::validFlagsForImage);
    EXPECT_EQ(MemObjHelper::commonFlagsIntel, MemObjHelper::commonFlagsIntel & MemObjHelper::validFlagsForBufferIntel);
    EXPECT_EQ(MemObjHelper::commonFlagsIntel, MemObjHelper::validFlagsForImageIntel);
}

TEST_F(LeoMemObjHelperFixtureTest, givenImageOnlyFlagsWhenInspectedThenTheyAreAbsentFromTheBufferSet) {
    EXPECT_EQ(0u, MemObjHelper::validFlagsForBuffer & CL_MEM_NO_ACCESS_INTEL);
    EXPECT_EQ(0u, MemObjHelper::validFlagsForBuffer & CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);
    EXPECT_NE(0u, MemObjHelper::validFlagsForImage & CL_MEM_NO_ACCESS_INTEL);
    EXPECT_NE(0u, MemObjHelper::validFlagsForImage & CL_MEM_ACCESS_FLAGS_UNRESTRICTED_INTEL);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
