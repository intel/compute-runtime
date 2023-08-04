/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/helpers/kernel_filename_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/mock_method_macros.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/program/program.h"
#include "opencl/test/unit_test/fixtures/enqueue_handler_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_debug_program.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/program/program_from_binary.h"

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
