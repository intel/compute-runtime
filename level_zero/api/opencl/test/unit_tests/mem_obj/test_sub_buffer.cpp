/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/sharings/leo_sharing.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/driver/driver_handle.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <memory>

namespace NEO {
namespace LEO {
namespace ult {

TEST(BufferInheritFlagsTests, givenNoAccessFlagsWhenInheritingThenParentAccessFlagIsCopied) {
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_ONLY), Buffer::inheritFlags(0, CL_MEM_READ_ONLY));
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_WRITE_ONLY), Buffer::inheritFlags(0, CL_MEM_WRITE_ONLY));
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_WRITE), Buffer::inheritFlags(0, CL_MEM_READ_WRITE));
}

TEST(BufferInheritFlagsTests, givenOwnAccessFlagWhenInheritingThenParentAccessFlagIsNotCopied) {
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_ONLY), Buffer::inheritFlags(CL_MEM_READ_ONLY, CL_MEM_READ_WRITE));
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_WRITE_ONLY), Buffer::inheritFlags(CL_MEM_WRITE_ONLY, CL_MEM_READ_ONLY));
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_WRITE), Buffer::inheritFlags(CL_MEM_READ_WRITE, CL_MEM_READ_ONLY));
}

TEST(BufferInheritFlagsTests, givenNoHostAccessFlagsWhenInheritingThenParentHostAccessFlagIsCopied) {
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_HOST_READ_ONLY), Buffer::inheritFlags(0, CL_MEM_HOST_READ_ONLY));
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_HOST_WRITE_ONLY), Buffer::inheritFlags(0, CL_MEM_HOST_WRITE_ONLY));
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_HOST_NO_ACCESS), Buffer::inheritFlags(0, CL_MEM_HOST_NO_ACCESS));
}

TEST(BufferInheritFlagsTests, givenOwnHostAccessFlagWhenInheritingThenParentHostAccessFlagIsNotCopied) {
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_HOST_READ_ONLY), Buffer::inheritFlags(CL_MEM_HOST_READ_ONLY, CL_MEM_HOST_NO_ACCESS));
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_HOST_NO_ACCESS), Buffer::inheritFlags(CL_MEM_HOST_NO_ACCESS, CL_MEM_HOST_WRITE_ONLY));
}

TEST(BufferInheritFlagsTests, givenParentHostPtrFlagsWhenInheritingThenTheyAreAlwaysCopied) {
    const cl_mem_flags hostPtrFlags = CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR;

    auto inherited = Buffer::inheritFlags(CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS, hostPtrFlags);

    EXPECT_EQ(hostPtrFlags, inherited & hostPtrFlags);
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_ONLY), inherited & CL_MEM_READ_ONLY);
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_HOST_NO_ACCESS), inherited & CL_MEM_HOST_NO_ACCESS);
}

TEST(BufferInheritFlagsTests, givenIndividualParentHostPtrFlagWhenInheritingThenOnlyThatOneIsCopied) {
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR), Buffer::inheritFlags(0, CL_MEM_USE_HOST_PTR));
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_ALLOC_HOST_PTR), Buffer::inheritFlags(0, CL_MEM_ALLOC_HOST_PTR));
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_COPY_HOST_PTR), Buffer::inheritFlags(0, CL_MEM_COPY_HOST_PTR));
}

TEST(BufferInheritFlagsTests, givenNoParentFlagsWhenInheritingThenChildFlagsAreUnchanged) {
    EXPECT_EQ(0u, Buffer::inheritFlags(0, 0));
    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_ONLY), Buffer::inheritFlags(CL_MEM_READ_ONLY, 0));
}

TEST(BufferInheritFlagsTests, givenParentWithEveryInheritableFlagWhenInheritingThenAllOfThemAreCopied) {
    const cl_mem_flags parentFlags = CL_MEM_READ_ONLY | CL_MEM_HOST_NO_ACCESS |
                                     CL_MEM_USE_HOST_PTR | CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR;

    EXPECT_EQ(parentFlags, Buffer::inheritFlags(0, parentFlags));
}

TEST(BufferInheritFlagsTests, givenParentFlagOutsideTheInheritedSetsWhenInheritingThenItIsNotCopied) {
    EXPECT_EQ(0u, Buffer::inheritFlags(0, CL_MEM_NO_ACCESS_INTEL));
    EXPECT_EQ(0u, Buffer::inheritFlags(0, CL_MEM_FORCE_HOST_MEMORY_INTEL));
    EXPECT_EQ(0u, Buffer::inheritFlags(0, CL_MEM_KERNEL_READ_AND_WRITE));
}

TEST(BufferInheritFlagsTests, givenChildFlagOutsideTheInheritedSetsWhenInheritingThenItIsKept) {
    auto inherited = Buffer::inheritFlags(CL_MEM_NO_ACCESS_INTEL, CL_MEM_READ_ONLY);

    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_NO_ACCESS_INTEL | CL_MEM_READ_ONLY), inherited);
}

TEST(BufferInheritFlagsTests, givenOnlyAccessFlagInChildWhenInheritingThenHostAccessIsStillTakenFromParent) {
    auto inherited = Buffer::inheritFlags(CL_MEM_WRITE_ONLY, CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY);

    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY), inherited);
}

TEST(BufferInheritFlagsTests, givenOnlyHostAccessFlagInChildWhenInheritingThenAccessIsStillTakenFromParent) {
    auto inherited = Buffer::inheritFlags(CL_MEM_HOST_WRITE_ONLY, CL_MEM_READ_ONLY | CL_MEM_HOST_READ_ONLY);

    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_ONLY | CL_MEM_HOST_WRITE_ONLY), inherited);
}

struct SubBufferFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        capturingContext = std::make_unique<CapturingContext>(driverHandle.get(), clDevice->getL0Handle());
        capturingContext->getDriverHandleCallBase = true;
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
        rootDeviceIndex = clDevice->getRootDeviceIndex();
        memoryManager = driverHandle->getMemoryManager();
    }

    void TearDown() override {
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    Buffer *createParentBuffer(cl_mem_flags flags = CL_MEM_READ_WRITE) {
        auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, bufferSize});
        EXPECT_NE(nullptr, allocation);
        return Buffer::createSharedBuffer(leoContext.get(), flags, new SharingHandler(),
                                          GraphicsAllocationHelper::toMultiGraphicsAllocation(allocation));
    }

    static constexpr size_t bufferSize = 2 * MemoryConstants::pageSize;

    ClDevice *clDevice = nullptr;
    uint32_t rootDeviceIndex = 0u;
    MemoryManager *memoryManager = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;
};

TEST_F(SubBufferFixture, givenNullRegionWhenValidatingSubBufferRegionThenReturnsInvalidValue) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};

    EXPECT_EQ(CL_INVALID_VALUE, buffer->validateSubBufferRegion(nullptr));
}

TEST_F(SubBufferFixture, givenZeroSizedRegionWhenValidatingSubBufferRegionThenReturnsInvalidBufferSize) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};
    cl_buffer_region region{0u, 0u};

    EXPECT_EQ(CL_INVALID_BUFFER_SIZE, buffer->validateSubBufferRegion(&region));
}

TEST_F(SubBufferFixture, givenOriginBeyondTheBufferWhenValidatingSubBufferRegionThenReturnsInvalidValue) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};
    cl_buffer_region region{bufferSize + 4u, 4u};

    EXPECT_EQ(CL_INVALID_VALUE, buffer->validateSubBufferRegion(&region));
}

TEST_F(SubBufferFixture, givenRegionEndingBeyondTheBufferWhenValidatingSubBufferRegionThenReturnsInvalidValue) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};
    cl_buffer_region region{bufferSize - 4u, 8u};

    EXPECT_EQ(CL_INVALID_VALUE, buffer->validateSubBufferRegion(&region));
}

TEST_F(SubBufferFixture, givenMisalignedOriginWhenValidatingSubBufferRegionThenReturnsMisalignedSubBufferOffset) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};

    for (size_t origin : {1u, 2u, 3u, 5u, 7u}) {
        cl_buffer_region region{origin, 4u};
        EXPECT_EQ(CL_MISALIGNED_SUB_BUFFER_OFFSET, buffer->validateSubBufferRegion(&region)) << "origin " << origin;
    }
}

TEST_F(SubBufferFixture, givenAlignedOriginWhenValidatingSubBufferRegionThenReturnsSuccess) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};

    for (size_t origin : {0u, 4u, 8u, 64u, 128u}) {
        cl_buffer_region region{origin, 4u};
        EXPECT_EQ(CL_SUCCESS, buffer->validateSubBufferRegion(&region)) << "origin " << origin;
    }
}

TEST_F(SubBufferFixture, givenWholeBufferRegionWhenValidatingSubBufferRegionThenReturnsSuccess) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};
    cl_buffer_region region{0u, bufferSize};

    EXPECT_EQ(CL_SUCCESS, buffer->validateSubBufferRegion(&region));
}

TEST_F(SubBufferFixture, givenSubBufferWhenValidatingSubBufferRegionThenReturnsInvalidMemObject) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};
    cl_buffer_region region{0u, 64u};
    std::unique_ptr<Buffer> subBuffer{buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region)};
    ASSERT_NE(nullptr, subBuffer);

    cl_buffer_region nestedRegion{0u, 32u};
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, subBuffer->validateSubBufferRegion(&nestedRegion));
    EXPECT_EQ(CL_INVALID_MEM_OBJECT, subBuffer->validateSubBufferRegion(nullptr));
}

TEST_F(SubBufferFixture, givenFourByteAlignedOffsetsThenSubBufferOffsetIsValid) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};

    EXPECT_TRUE(buffer->isValidSubBufferOffset(0u));
    EXPECT_TRUE(buffer->isValidSubBufferOffset(4u));
    EXPECT_TRUE(buffer->isValidSubBufferOffset(1024u));
}

TEST_F(SubBufferFixture, givenOffsetsNotAlignedToFourBytesThenSubBufferOffsetIsInvalid) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};

    EXPECT_FALSE(buffer->isValidSubBufferOffset(1u));
    EXPECT_FALSE(buffer->isValidSubBufferOffset(2u));
    EXPECT_FALSE(buffer->isValidSubBufferOffset(1023u));
}

TEST_F(SubBufferFixture, givenRegionWhenCreatingSubBufferThenSizeAndParentAreSet) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};
    cl_buffer_region region{256u, 512u};

    std::unique_ptr<Buffer> subBuffer{buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region)};
    ASSERT_NE(nullptr, subBuffer);

    EXPECT_EQ(512u, subBuffer->getApiSize());
    EXPECT_TRUE(subBuffer->isSubBuffer());
    EXPECT_FALSE(buffer->isSubBuffer());
    EXPECT_EQ(static_cast<cl_mem_object_type>(CL_MEM_OBJECT_BUFFER), subBuffer->getClObjectType());
    EXPECT_EQ(leoContext.get(), subBuffer->getContext());
}

TEST_F(SubBufferFixture, givenRegionWhenCreatingSubBufferThenItsDeviceAddressIsOffsetFromTheParent) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};
    const auto parentAddress = castToUint64(buffer->getUsmPtr());
    cl_buffer_region region{256u, 512u};

    std::unique_ptr<Buffer> subBuffer{buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region)};
    ASSERT_NE(nullptr, subBuffer);

    EXPECT_EQ(parentAddress + 256u, castToUint64(subBuffer->getUsmPtr()));
}

TEST_F(SubBufferFixture, givenSubBufferWhenCreatedThenParentInternalReferenceIsHeld) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};
    const auto refCountBefore = buffer->getRefInternalCount();
    cl_buffer_region region{0u, 64u};

    auto subBuffer = buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region);
    ASSERT_NE(nullptr, subBuffer);
    EXPECT_EQ(refCountBefore + 1, buffer->getRefInternalCount());

    delete subBuffer;
}

TEST_F(SubBufferFixture, givenParentUsingSvmWhenCreatingSubBufferThenTheFlagIsPropagated) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};
    buffer->setUsesSvm(true);
    cl_buffer_region region{0u, 64u};

    std::unique_ptr<Buffer> subBuffer{buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region)};
    ASSERT_NE(nullptr, subBuffer);

    EXPECT_TRUE(subBuffer->getUsesSvm());
    buffer->setUsesSvm(false);
}

TEST_F(SubBufferFixture, givenParentWithoutSvmWhenCreatingSubBufferThenTheFlagIsNotSet) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};
    cl_buffer_region region{0u, 64u};

    std::unique_ptr<Buffer> subBuffer{buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region)};
    ASSERT_NE(nullptr, subBuffer);

    EXPECT_FALSE(subBuffer->getUsesSvm());
}

TEST_F(SubBufferFixture, givenParentWithSharingHandlerWhenCreatingSubBufferThenTheHandlerIsShared) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};
    cl_buffer_region region{0u, 64u};

    std::unique_ptr<Buffer> subBuffer{buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region)};
    ASSERT_NE(nullptr, subBuffer);

    EXPECT_NE(nullptr, subBuffer->peekSharingHandler());
    EXPECT_EQ(buffer->peekSharingHandler(), subBuffer->peekSharingHandler());
}

TEST_F(SubBufferFixture, givenNoAccessFlagsWhenCreatingSubBufferThenParentFlagsAreInherited) {
    std::unique_ptr<Buffer> buffer{createParentBuffer(CL_MEM_READ_ONLY)};
    cl_buffer_region region{0u, 64u};

    std::unique_ptr<Buffer> subBuffer{buffer->createSubBuffer(0, 0, &region)};
    ASSERT_NE(nullptr, subBuffer);

    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_ONLY), subBuffer->getFlags());
}

TEST_F(SubBufferFixture, givenOwnAccessFlagsWhenCreatingSubBufferThenTheyWinOverTheParentOnes) {
    std::unique_ptr<Buffer> buffer{createParentBuffer(CL_MEM_READ_WRITE)};
    cl_buffer_region region{0u, 64u};

    std::unique_ptr<Buffer> subBuffer{buffer->createSubBuffer(CL_MEM_READ_ONLY, 0, &region)};
    ASSERT_NE(nullptr, subBuffer);

    EXPECT_EQ(static_cast<cl_mem_flags>(CL_MEM_READ_ONLY), subBuffer->getFlags());
}

TEST_F(SubBufferFixture, givenSubBufferWhenQueryingMemObjectInfoThenOffsetAndAssociatedObjectAreReported) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};
    cl_buffer_region region{128u, 256u};

    std::unique_ptr<Buffer> subBuffer{buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &region)};
    ASSERT_NE(nullptr, subBuffer);

    size_t offset = 0;
    ASSERT_EQ(CL_SUCCESS, subBuffer->getMemObjectInfo(CL_MEM_OFFSET, sizeof(offset), &offset, nullptr));
    EXPECT_EQ(128u, offset);

    cl_mem associated = nullptr;
    ASSERT_EQ(CL_SUCCESS, subBuffer->getMemObjectInfo(CL_MEM_ASSOCIATED_MEMOBJECT, sizeof(associated), &associated, nullptr));
    EXPECT_EQ(static_cast<cl_mem>(buffer.get()), associated);

    size_t size = 0;
    ASSERT_EQ(CL_SUCCESS, subBuffer->getMemObjectInfo(CL_MEM_SIZE, sizeof(size), &size, nullptr));
    EXPECT_EQ(256u, size);
}

TEST_F(SubBufferFixture, givenParentBufferWhenQueryingMemObjectInfoThenNoOffsetOrAssociatedObjectIsReported) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};

    size_t offset = 123u;
    ASSERT_EQ(CL_SUCCESS, buffer->getMemObjectInfo(CL_MEM_OFFSET, sizeof(offset), &offset, nullptr));
    EXPECT_EQ(0u, offset);

    cl_mem associated = reinterpret_cast<cl_mem>(0x1234u);
    ASSERT_EQ(CL_SUCCESS, buffer->getMemObjectInfo(CL_MEM_ASSOCIATED_MEMOBJECT, sizeof(associated), &associated, nullptr));
    EXPECT_EQ(nullptr, associated);
}

TEST_F(SubBufferFixture, givenSeveralSubBuffersOfTheSameParentWhenCreatedThenEachKeepsItsOwnRegion) {
    std::unique_ptr<Buffer> buffer{createParentBuffer()};
    const auto parentAddress = castToUint64(buffer->getUsmPtr());

    cl_buffer_region firstRegion{0u, 64u};
    cl_buffer_region secondRegion{256u, 128u};
    std::unique_ptr<Buffer> firstSubBuffer{buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &firstRegion)};
    std::unique_ptr<Buffer> secondSubBuffer{buffer->createSubBuffer(CL_MEM_READ_WRITE, 0, &secondRegion)};
    ASSERT_NE(nullptr, firstSubBuffer);
    ASSERT_NE(nullptr, secondSubBuffer);

    EXPECT_EQ(parentAddress, castToUint64(firstSubBuffer->getUsmPtr()));
    EXPECT_EQ(parentAddress + 256u, castToUint64(secondSubBuffer->getUsmPtr()));
    EXPECT_EQ(64u, firstSubBuffer->getApiSize());
    EXPECT_EQ(128u, secondSubBuffer->getApiSize());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
