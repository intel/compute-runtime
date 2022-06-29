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
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/memory_manager/mem_obj_surface.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

#include <type_traits>

using namespace NEO;

namespace createSurface {
Surface *create(char *data, MockBuffer *buffer, GraphicsAllocation *gfxAllocation) {
    return new MemObjSurface(buffer);
}
} // namespace createSurface

class SurfaceTest : public ::testing::Test {
  public:
    char data[10];
    MockBuffer buffer;
    MockGraphicsAllocation gfxAllocation{nullptr, 0};
};

HWTEST_F(SurfaceTest, GivenSurfaceWhenInterfaceIsUsedThenSurfaceBehavesCorrectly) {
    int32_t execStamp;

    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    DeviceBitfield deviceBitfield(1);
    auto csr = std::make_unique<MockCsr<FamilyType>>(execStamp, *executionEnvironment, 0, deviceBitfield);
    auto hwInfo = *defaultHwInfo;
    auto engine = HwHelper::get(hwInfo.platform.eRenderCoreFamily).getGpgpuEngineInstances(hwInfo)[0];
    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(engine, PreemptionHelper::getDefaultPreemptionMode(hwInfo)));
    csr->setupContext(*osContext);

    Surface *surface = createSurface::create(this->data,
                                             &this->buffer,
                                             &this->gfxAllocation);
    ASSERT_NE(nullptr, surface); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    Surface *duplicatedSurface = surface->duplicate();
    ASSERT_NE(nullptr, duplicatedSurface); // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)

    surface->makeResident(*csr);

    EXPECT_EQ(1u, csr->madeResidentGfxAllocations.size());

    delete duplicatedSurface;
    delete surface;
}

class CoherentMemObjSurface : public SurfaceTest {
  public:
    CoherentMemObjSurface() {
        this->buffer.getGraphicsAllocation(mockRootDeviceIndex)->setCoherent(true);
    }
};

TEST_F(CoherentMemObjSurface, GivenCoherentMemObjWhenCreatingSurfaceFromMemObjThenSurfaceIsCoherent) {
    Surface *surface = createSurface::create(this->data,
                                             &this->buffer,
                                             &this->gfxAllocation);

    EXPECT_TRUE(surface->IsCoherent);

    delete surface;
}