/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/page_fault_manager/mock_cpu_page_fault_manager.h"

#include "gtest/gtest.h"

#include <type_traits>

using namespace NEO;

typedef ::testing::Types<NullSurface, HostPtrSurface, GeneralSurface> SurfaceTypes;

namespace createSurface {
template <typename surfType>
Surface *create(char *data, GraphicsAllocation *gfxAllocation);

template <>
Surface *create<NullSurface>(char *data, GraphicsAllocation *gfxAllocation) {
    return new NullSurface;
}

template <>
Surface *create<HostPtrSurface>(char *data, GraphicsAllocation *gfxAllocation) {
    return new HostPtrSurface(data, 10, gfxAllocation);
}
template <>
Surface *create<GeneralSurface>(char *data, GraphicsAllocation *gfxAllocation) {
    return new GeneralSurface(gfxAllocation);
}
} // namespace createSurface

template <typename T>
class SurfaceTest : public ::testing::Test {
  public:
    char data[10];
    MockGraphicsAllocation gfxAllocation;
};

TYPED_TEST_CASE(SurfaceTest, SurfaceTypes);

HWTEST_TYPED_TEST(SurfaceTest, GivenSurfaceWhenInterfaceIsUsedThenSurfaceBehavesCorrectly) {
    int32_t execStamp;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto memoryManager = std::make_unique<MockMemoryManager>();
    executionEnvironment->memoryManager.reset(memoryManager.release());
    DeviceBitfield deviceBitfield(1);
    auto csr = std::make_unique<MockCsr<FamilyType>>(execStamp, *executionEnvironment, 0, deviceBitfield);
    auto hwInfo = *defaultHwInfo;
    auto engine = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo)[0];
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(engine, PreemptionHelper::getDefaultPreemptionMode(hwInfo)));
    csr->setupContext(*osContext);

    Surface *surface = createSurface::create<TypeParam>(this->data,
                                                        &this->gfxAllocation);
    ASSERT_NE(nullptr, surface); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    Surface *duplicatedSurface = surface->duplicate();
    ASSERT_NE(nullptr, duplicatedSurface); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    surface->makeResident(*csr);

    if (std::is_same<TypeParam, HostPtrSurface>::value ||
        std::is_same<TypeParam, GeneralSurface>::value) {
        EXPECT_EQ(1u, csr->madeResidentGfxAllocations.size());
    }

    delete duplicatedSurface;
    delete surface;
}

TEST(HostPtrSurfaceTest, givenHostPtrSurfaceWhenCreatedWithoutSpecifyingPtrCopyAllowanceThenPtrCopyIsNotAllowed) {
    char memory[2] = {};
    HostPtrSurface surface(memory, sizeof(memory));

    EXPECT_FALSE(surface.peekIsPtrCopyAllowed());
}

TEST(HostPtrSurfaceTest, givenHostPtrSurfaceWhenCreatedWithPtrCopyAllowedThenQueryReturnsTrue) {
    char memory[2] = {};
    HostPtrSurface surface(memory, sizeof(memory), true);

    EXPECT_TRUE(surface.peekIsPtrCopyAllowed());
}

TEST(HostPtrSurfaceTest, givenHostPtrSurfaceWhenCreatedWithPtrCopyNotAllowedThenQueryReturnsFalse) {
    char memory[2] = {};
    HostPtrSurface surface(memory, sizeof(memory), false);

    EXPECT_FALSE(surface.peekIsPtrCopyAllowed());
}

using GeneralSurfaceTest = ::testing::Test;

HWTEST_F(GeneralSurfaceTest, givenGeneralSurfaceWhenMigrationNeededThenMoveToGpuDomainCalled) {
    int32_t execStamp;
    MockGraphicsAllocation allocation;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto memoryManager = std::make_unique<MockMemoryManager>();
    executionEnvironment->memoryManager.reset(memoryManager.release());
    auto pageFaultManager = std::make_unique<MockPageFaultManager>();
    auto pageFaultManagerPtr = pageFaultManager.get();
    static_cast<MockMemoryManager *>(executionEnvironment->memoryManager.get())->pageFaultManager.reset(pageFaultManager.release());
    DeviceBitfield deviceBitfield(1);
    auto csr = std::make_unique<MockCsr<FamilyType>>(execStamp, *executionEnvironment, 0, deviceBitfield);
    auto hwInfo = *defaultHwInfo;
    auto engine = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo)[0];
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(engine, PreemptionHelper::getDefaultPreemptionMode(hwInfo)));
    csr->setupContext(*osContext);

    auto surface = std::make_unique<GeneralSurface>(&allocation, true);

    surface->makeResident(*csr);
    EXPECT_EQ(pageFaultManagerPtr->moveAllocationToGpuDomainCalled, 1);
}

HWTEST_F(GeneralSurfaceTest, givenGeneralSurfaceWhenMigrationNotNeededThenMoveToGpuDomainNotCalled) {
    int32_t execStamp;
    MockGraphicsAllocation allocation;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto memoryManager = std::make_unique<MockMemoryManager>();
    executionEnvironment->memoryManager.reset(memoryManager.release());
    auto pageFaultManager = std::make_unique<MockPageFaultManager>();
    auto pageFaultManagerPtr = pageFaultManager.get();
    static_cast<MockMemoryManager *>(executionEnvironment->memoryManager.get())->pageFaultManager.reset(pageFaultManager.release());
    DeviceBitfield deviceBitfield(1);
    auto csr = std::make_unique<MockCsr<FamilyType>>(execStamp, *executionEnvironment, 0, deviceBitfield);
    auto hwInfo = *defaultHwInfo;
    auto engine = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo)[0];
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(engine, PreemptionHelper::getDefaultPreemptionMode(hwInfo)));
    csr->setupContext(*osContext);

    auto surface = std::make_unique<GeneralSurface>(&allocation, false);

    surface->makeResident(*csr);
    EXPECT_EQ(pageFaultManagerPtr->moveAllocationToGpuDomainCalled, 0);
}