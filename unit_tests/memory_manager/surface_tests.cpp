/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gtest/gtest.h"
#include "test.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_csr.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "hw_cmds.h"
#include <type_traits>

using namespace OCLRT;

typedef ::testing::Types<NullSurface, HostPtrSurface, MemObjSurface, GeneralSurface> SurfaceTypes;

namespace createSurface {
template <typename surfType>
Surface *Create(char *data, MockBuffer *buffer, GraphicsAllocation *gfxAllocation);

template <>
Surface *Create<NullSurface>(char *data, MockBuffer *buffer, GraphicsAllocation *gfxAllocation) {
    return new NullSurface;
}

template <>
Surface *Create<HostPtrSurface>(char *data, MockBuffer *buffer, GraphicsAllocation *gfxAllocation) {
    return new HostPtrSurface(data, 10, gfxAllocation);
}

template <>
Surface *Create<MemObjSurface>(char *data, MockBuffer *buffer, GraphicsAllocation *gfxAllocation) {
    return new MemObjSurface(buffer);
}

template <>
Surface *Create<GeneralSurface>(char *data, MockBuffer *buffer, GraphicsAllocation *gfxAllocation) {
    return new GeneralSurface(gfxAllocation);
}
} // namespace createSurface

template <typename T>
class SurfaceTest : public ::testing::Test {
  public:
    char data[10];
    MockBuffer buffer;
    MockGraphicsAllocation gfxAllocation{nullptr, 0};
};

TYPED_TEST_CASE(SurfaceTest, SurfaceTypes);

HWTEST_TYPED_TEST(SurfaceTest, GivenSurfaceWhenInterfaceIsUsedThenSurfaceBehavesCorrectly) {
    int32_t execStamp;

    ExecutionEnvironment executionEnvironment;
    MockCsr<FamilyType> *csr = new MockCsr<FamilyType>(execStamp, executionEnvironment);
    executionEnvironment.commandStreamReceivers.push_back(std::unique_ptr<CommandStreamReceiver>(csr));
    executionEnvironment.memoryManager.reset(csr->createMemoryManager(false, false));

    Surface *surface = createSurface::Create<TypeParam>(this->data,
                                                        &this->buffer,
                                                        &this->gfxAllocation);
    ASSERT_NE(nullptr, surface);

    Surface *duplicatedSurface = surface->duplicate();
    ASSERT_NE(nullptr, duplicatedSurface);

    surface->makeResident(*csr);

    if (std::is_same<TypeParam, HostPtrSurface>::value ||
        std::is_same<TypeParam, MemObjSurface>::value ||
        std::is_same<TypeParam, GeneralSurface>::value) {
        EXPECT_EQ(1u, csr->madeResidentGfxAllocations.size());
    }

    delete duplicatedSurface;
    delete surface;
}

class CoherentMemObjSurface : public SurfaceTest<MemObjSurface> {
  public:
    CoherentMemObjSurface() {
        this->buffer.getGraphicsAllocation()->setCoherent(true);
    }
};

TEST_F(CoherentMemObjSurface, BufferFromCoherentSvm) {
    Surface *surface = createSurface::Create<MemObjSurface>(this->data,
                                                            &this->buffer,
                                                            &this->gfxAllocation);

    EXPECT_TRUE(surface->IsCoherent);

    delete surface;
}

TEST(HostPtrSurfaceTest, givenHostPtrSurfaceWhenCreatedWithoutSpecifyingPtrCopyAllowanceThenPtrCopyIsNotAllowed) {
    char memory[2];
    HostPtrSurface surface(memory, sizeof(memory));

    EXPECT_FALSE(surface.peekIsPtrCopyAllowed());
}

TEST(HostPtrSurfaceTest, givenHostPtrSurfaceWhenCreatedWithPtrCopyAllowedThenQueryReturnsTrue) {
    char memory[2];
    HostPtrSurface surface(memory, sizeof(memory), true);

    EXPECT_TRUE(surface.peekIsPtrCopyAllowed());
}

TEST(HostPtrSurfaceTest, givenHostPtrSurfaceWhenCreatedWithPtrCopyNotAllowedThenQueryReturnsFalse) {
    char memory[2];
    HostPtrSurface surface(memory, sizeof(memory), false);

    EXPECT_FALSE(surface.peekIsPtrCopyAllowed());
}
