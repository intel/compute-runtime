/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_context.h"
#include "shared/source/source_level_debugger/source_level_debugger.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/helpers/kernel_filename_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/program/program.h"
#include "opencl/test/unit_test/fixtures/enqueue_handler_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/program/program_from_binary.h"

#include "compiler_options.h"

using namespace NEO;
using namespace ::testing;

typedef EnqueueHandlerTest EnqueueDebugKernelSimpleTest;

class EnqueueDebugKernelTest : public ProgramSimpleFixture,
                               public ::testing::Test {
  public:
    void SetUp() override {
        ProgramSimpleFixture::SetUp();
        device = pClDevice;
        pDevice->executionEnvironment->rootDeviceEnvironments[pDevice->getRootDeviceIndex()]->debugger.reset(new SourceLevelDebugger(nullptr));

        auto sipType = SipKernel::getSipKernelType(*pDevice);
        SipKernel::initSipKernel(sipType, *pDevice);

        if (pDevice->getHardwareInfo().platform.eRenderCoreFamily >= IGFX_GEN9_CORE) {
            pDevice->deviceInfo.debuggerActive = true;
            std::string filename;
            std::string kernelOption(CompilerOptions::debugKernelEnable);
            KernelFilenameHelper::getKernelFilenameFromInternalOption(kernelOption, filename);

            kbHelper = new KernelBinaryHelper(filename, false);
            CreateProgramWithSource(
                pContext,
                "copybuffer.cl");
            pProgram->enableKernelDebug();

            cl_int retVal = pProgram->build(pProgram->getDevices(), nullptr, false);
            ASSERT_EQ(CL_SUCCESS, retVal);

            // create a kernel
            pMultiDeviceKernel = MultiDeviceKernel::create(
                pProgram,
                pProgram->getKernelInfosForKernel("CopyBuffer"),
                &retVal);
            debugKernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);

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
            pMultiDeviceKernel->release();
        }
        ProgramSimpleFixture::TearDown();
    }
    cl_device_id device;
    Kernel *debugKernel = nullptr;
    MultiDeviceKernel *pMultiDeviceKernel = nullptr;
    KernelBinaryHelper *kbHelper = nullptr;
    MockContext context;
    MockBuffer bufferSrc;
    MockBuffer bufferDst;
};

HWTEST_F(EnqueueDebugKernelTest, givenDebugKernelWhenEnqueuedThenSSHAndBtiAreCorrectlySet) {
    if (pDevice->isDebuggerActive()) {
        using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
        using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
        std::unique_ptr<MockCommandQueueHw<FamilyType>> mockCmdQ(new MockCommandQueueHw<FamilyType>(&context, pClDevice, 0));

        size_t gws[] = {1, 1, 1};
        auto &ssh = mockCmdQ->getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 4096u);
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

HWTEST_F(EnqueueDebugKernelTest, givenDebugKernelWhenEnqueuedThenSurfaceStateForDebugSurfaceIsSetAtBindlessOffsetZero) {
    if (pDevice->isDebuggerActive()) {
        using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
        using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
        std::unique_ptr<MockCommandQueueHw<FamilyType>> mockCmdQ(new MockCommandQueueHw<FamilyType>(&context, pClDevice, 0));

        size_t gws[] = {1, 1, 1};
        auto &ssh = mockCmdQ->getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 4096u);
        mockCmdQ->enqueueKernel(debugKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

        auto debugSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ssh.getCpuBase());

        auto &commandStreamReceiver = mockCmdQ->getGpgpuCommandStreamReceiver();
        auto debugSurface = commandStreamReceiver.getDebugSurfaceAllocation();

        SURFACE_STATE_BUFFER_LENGTH length;
        length.Length = static_cast<uint32_t>(debugSurface->getUnderlyingBufferSize() - 1);

        EXPECT_EQ(length.SurfaceState.Depth + 1u, debugSurfaceState->getDepth());
        EXPECT_EQ(length.SurfaceState.Width + 1u, debugSurfaceState->getWidth());
        EXPECT_EQ(length.SurfaceState.Height + 1u, debugSurfaceState->getHeight());
        EXPECT_EQ(debugSurface->getGpuAddress(), debugSurfaceState->getSurfaceBaseAddress());

        EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, debugSurfaceState->getSurfaceType());
        EXPECT_EQ(UnitTestHelper<FamilyType>::getCoherencyTypeSupported(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT), debugSurfaceState->getCoherencyType());
    }
}

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

HWTEST_F(EnqueueDebugKernelSimpleTest, givenKernelFromProgramWithDebugEnabledWhenEnqueuedThenDebugSurfaceIsSetup) {
    MockProgram program(context, false, toClDeviceVector(*pClDevice));
    program.enableKernelDebug();
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(*pDevice, &program));
    kernel->initialize();
    std::unique_ptr<MockCommandQueueHwSetupDebugSurface<FamilyType>> mockCmdQ(new MockCommandQueueHwSetupDebugSurface<FamilyType>(context, pClDevice, 0));
    auto hwInfo = *NEO::defaultHwInfo.get();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    mockCmdQ->getGpgpuCommandStreamReceiver().allocateDebugSurface(hwHelper.getSipKernelMaxDbgSurfaceSize(hwInfo));
    mockCmdQ->setupDebugSurfaceParamsPassed.clear();

    EXPECT_TRUE(isValidOffset(kernel->getKernelInfo().kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful));

    size_t gws[] = {1, 1, 1};
    mockCmdQ->enqueueKernel(kernel.get(), 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(1u, mockCmdQ->setupDebugSurfaceCalled);
    EXPECT_EQ(kernel.get(), mockCmdQ->setupDebugSurfaceParamsPassed[0].kernel);
}

HWTEST_F(EnqueueDebugKernelSimpleTest, givenKernelWithoutSystemThreadSurfaceWhenEnqueuedThenDebugSurfaceIsNotSetup) {
    MockProgram program(context, false, toClDeviceVector(*pClDevice));
    program.enableKernelDebug();
    std::unique_ptr<MockKernel> kernel(MockKernel::create<MockKernel>(*pDevice, &program));
    kernel->initialize();

    EXPECT_FALSE(isValidOffset(kernel->getKernelInfo().kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful));

    std::unique_ptr<MockCommandQueueHwSetupDebugSurface<FamilyType>> mockCmdQ(new MockCommandQueueHwSetupDebugSurface<FamilyType>(context, pClDevice, 0));

    size_t gws[] = {1, 1, 1};
    mockCmdQ->enqueueKernel(kernel.get(), 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(0u, mockCmdQ->setupDebugSurfaceCalled);
}

HWTEST_F(EnqueueDebugKernelSimpleTest, givenKernelFromProgramWithoutDebugEnabledWhenEnqueuedThenDebugSurfaceIsNotSetup) {
    MockProgram program(context, false, toClDeviceVector(*pClDevice));
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(*pDevice, &program));
    std::unique_ptr<MockCommandQueueHwSetupDebugSurface<FamilyType>> mockCmdQ(new MockCommandQueueHwSetupDebugSurface<FamilyType>(context, pClDevice, nullptr));

    size_t gws[] = {1, 1, 1};
    mockCmdQ->enqueueKernel(kernel.get(), 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(nullptr, mockCmdQ->getGpgpuCommandStreamReceiver().getDebugSurfaceAllocation());
    EXPECT_EQ(0u, mockCmdQ->setupDebugSurfaceCalled);
}

using ActiveDebuggerTest = EnqueueDebugKernelTest;

HWTEST_F(ActiveDebuggerTest, givenKernelFromProgramWithoutDebugEnabledAndActiveDebuggerWhenEnqueuedThenDebugSurfaceIsSetup) {
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    std::unique_ptr<MockDebugKernel> kernel(MockKernel::create<MockDebugKernel>(*pDevice, &program));
    std::unique_ptr<CommandQueueHw<FamilyType>> cmdQ(new CommandQueueHw<FamilyType>(&context, pClDevice, nullptr, false));

    size_t gws[] = {1, 1, 1};
    cmdQ->enqueueKernel(kernel.get(), 1, nullptr, gws, nullptr, 0, nullptr, nullptr);

    EXPECT_NE(nullptr, cmdQ->getGpgpuCommandStreamReceiver().getDebugSurfaceAllocation());
}