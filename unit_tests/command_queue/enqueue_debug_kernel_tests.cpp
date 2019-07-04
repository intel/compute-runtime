/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "runtime/compiler_interface/compiler_options.h"
#include "runtime/os_interface/os_context.h"
#include "runtime/program/program.h"
#include "runtime/source_level_debugger/source_level_debugger.h"
#include "test.h"
#include "unit_tests/fixtures/enqueue_handler_fixture.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/helpers/kernel_filename_helper.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/program/program_from_binary.h"

#include "gmock/gmock.h"

using namespace NEO;
using namespace ::testing;

typedef EnqueueHandlerTest EnqueueDebugKernelSimpleTest;

class EnqueueDebugKernelTest : public ProgramSimpleFixture,
                               public ::testing::Test {
  public:
    void SetUp() override {
        ProgramSimpleFixture::SetUp();
        device = pDevice;
        pDevice->executionEnvironment->sourceLevelDebugger.reset(new SourceLevelDebugger(nullptr));

        if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {
            pDevice->deviceInfo.sourceLevelDebuggerActive = true;
            std::string filename;
            std::string kernelOption(CompilerOptions::debugKernelEnable);
            KernelFilenameHelper::getKernelFilenameFromInternalOption(kernelOption, filename);

            kbHelper = new KernelBinaryHelper(filename, false);
            CreateProgramWithSource(
                pContext,
                &device,
                "copybuffer.cl");
            pProgram->enableKernelDebug();

            cl_int retVal = pProgram->build(1, &device, nullptr, nullptr, nullptr, false);
            ASSERT_EQ(CL_SUCCESS, retVal);

            // create a kernel
            debugKernel = Kernel::create(
                pProgram,
                *pProgram->getKernelInfo("CopyBuffer"),
                &retVal);

            ASSERT_EQ(CL_SUCCESS, retVal);
            ASSERT_NE(nullptr, debugKernel);

            cl_mem src = &bufferSrc;
            cl_mem dst = &bufferDst;
            retVal = debugKernel->setArg(
                0,
                sizeof(cl_mem),
                &src);
            retVal = debugKernel->setArg(
                1,
                sizeof(cl_mem),
                &dst);
        }
    }

    void TearDown() override {
        if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {
            delete kbHelper;
            debugKernel->release();
        }
        ProgramSimpleFixture::TearDown();
    }
    cl_device_id device;
    Kernel *debugKernel = nullptr;
    KernelBinaryHelper *kbHelper = nullptr;
    MockContext context;
    MockBuffer bufferSrc;
    MockBuffer bufferDst;
};

HWTEST_F(EnqueueDebugKernelTest, givenDebugKernelWhenEnqueuedThenSSHAndBtiAreCorrectlySet) {
    if (pDevice->isSourceLevelDebuggerActive()) {
        using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
        using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
        std::unique_ptr<MockCommandQueueHw<FamilyType>> mockCmdQ(new MockCommandQueueHw<FamilyType>(&context, pDevice, 0));

        size_t gws[] = {1, 1, 1};
        auto &ssh = mockCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 4096u);
        void *surfaceStates = ssh.getSpace(0);

        mockCmdQ->enqueueKernel(debugKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

        auto *dstBtiTableBase = reinterpret_cast<BINDING_TABLE_STATE *>(ptrOffset(surfaceStates, debugKernel->getBindingTableOffset()));
        uint32_t surfaceStateOffset = dstBtiTableBase[0].getSurfaceStatePointer();

        auto debugSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh.getCpuBase(), surfaceStateOffset));

        auto &commandStreamReceiver = mockCmdQ->getGpgpuCommandStreamReceiver();
        auto debugSurface = commandStreamReceiver.getDebugSurfaceAllocation();
        EXPECT_EQ(1u, debugSurface->getTaskCount(commandStreamReceiver.getOsContext().getContextId()));

        EXPECT_EQ(debugSurface->getGpuAddress(), debugSurfaceState->getSurfaceBaseAddress());
    }
}

template <typename GfxFamily>
class GMockCommandQueueHw : public CommandQueueHw<GfxFamily> {
    typedef CommandQueueHw<GfxFamily> BaseClass;

  public:
    GMockCommandQueueHw(Context *context, Device *device, cl_queue_properties *properties) : BaseClass(context, device, properties) {
    }

    MOCK_METHOD1(setupDebugSurface, bool(Kernel *kernel));
};

HWTEST_F(EnqueueDebugKernelSimpleTest, givenKernelFromProgramWithDebugEnabledWhenEnqueuedThenDebugSurfaceIsSetup) {
    MockProgram program(*pDevice->getExecutionEnvironment());
    program.enableKernelDebug();
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(*pDevice, &program));
    kernel->setContext(context);
    std::unique_ptr<GMockCommandQueueHw<FamilyType>> mockCmdQ(new GMockCommandQueueHw<FamilyType>(context, pDevice, 0));

    EXPECT_CALL(*mockCmdQ.get(), setupDebugSurface(kernel.get())).Times(1).RetiresOnSaturation();

    size_t gws[] = {1, 1, 1};
    mockCmdQ->enqueueKernel(kernel.get(), 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    ::testing::Mock::VerifyAndClearExpectations(mockCmdQ.get());
}

HWTEST_F(EnqueueDebugKernelSimpleTest, givenKernelFromProgramWithoutDebugEnabledWhenEnqueuedThenDebugSurfaceIsNotSetup) {
    MockProgram program(*pDevice->getExecutionEnvironment());
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(*pDevice, &program));
    kernel->setContext(context);
    std::unique_ptr<NiceMock<GMockCommandQueueHw<FamilyType>>> mockCmdQ(new NiceMock<GMockCommandQueueHw<FamilyType>>(context, pDevice, nullptr));

    EXPECT_CALL(*mockCmdQ.get(), setupDebugSurface(kernel.get())).Times(0);

    size_t gws[] = {1, 1, 1};
    mockCmdQ->enqueueKernel(kernel.get(), 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    ::testing::Mock::VerifyAndClearExpectations(mockCmdQ.get());
}
