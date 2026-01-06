/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/enqueue_handler_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

typedef EnqueueHandlerTest EnqueueDebugKernelSimpleTest;

template <typename GfxFamily>
class MockCommandQueueHwSetupDebugSurface : public CommandQueueHw<GfxFamily> {
    typedef CommandQueueHw<GfxFamily> BaseClass;

  public:
    MockCommandQueueHwSetupDebugSurface(Context *context, ClDevice *device, cl_queue_properties *properties) : BaseClass(context, device, properties, false) {
    }

    bool setupDebugSurface(Kernel *kernel) override {
        setupDebugSurfaceCalled++;
        setupDebugSurfaceParamsPassed.push_back({kernel});
        return setupDebugSurfaceResult;
    }

    struct SetupDebugSurfaceParams {
        Kernel *kernel = nullptr;
    };

    StackVec<SetupDebugSurfaceParams, 1> setupDebugSurfaceParamsPassed{};
    uint32_t setupDebugSurfaceCalled = 0u;
    bool setupDebugSurfaceResult = true;
};

HWTEST_F(EnqueueDebugKernelSimpleTest, givenKernelFromProgramWithoutDebugEnabledWhenEnqueuedThenDebugSurfaceIsNotSetup) {
    MockProgram program(context, false, toClDeviceVector(*pClDevice));
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(*pDevice, &program));
    std::unique_ptr<MockCommandQueueHwSetupDebugSurface<FamilyType>> mockCmdQ(new MockCommandQueueHwSetupDebugSurface<FamilyType>(context, pClDevice, nullptr));

    size_t gws[] = {1, 1, 1};
    mockCmdQ->enqueueKernel(kernel.get(), 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(nullptr, mockCmdQ->getGpgpuCommandStreamReceiver().getDebugSurfaceAllocation());
    EXPECT_EQ(0u, mockCmdQ->setupDebugSurfaceCalled);
}
