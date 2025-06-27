/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/fixtures/linear_stream_fixture.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

namespace NEO {
class ClDevice;
class Program;
struct KernelInfo;

struct GpgpuWalkerTests : public ::testing::Test {
    void SetUp() override {
    }

    void TearDown() override {
    }
};

GEN12LPTEST_F(GpgpuWalkerTests, givenMiStoreRegMemWhenAdjustMiStoreRegMemModeThenMmioRemapEnableIsSet) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    MI_STORE_REGISTER_MEM cmd = FamilyType::cmdInitStoreRegisterMem;

    GpgpuWalkerHelper<FamilyType>::adjustMiStoreRegMemMode(&cmd);

    EXPECT_EQ(true, cmd.getMmioRemapEnable());
}

class MockKernelWithApplicableWa : public MockKernel {
  public:
    MockKernelWithApplicableWa(Program *program, const KernelInfo &kernelInfos, ClDevice &clDeviceArg) : MockKernel(program, kernelInfos, clDeviceArg) {}
    bool requiresWaDisableRccRhwoOptimization() const override {
        return waApplicable;
    }
    bool waApplicable = false;
};

struct HardwareInterfaceTests : public ClDeviceFixture, public LinearStreamFixture, public ::testing::Test {
    void SetUp() override {
        ClDeviceFixture::setUp();
        LinearStreamFixture::setUp();

        pContext = new NEO::MockContext(pClDevice);
        pCommandQueue = new MockCommandQueue(pContext, pClDevice, nullptr, false);
        pProgram = new MockProgram(pContext, false, toClDeviceVector(*pClDevice));
        auto kernelInfos = MockKernel::toKernelInfoContainer(pProgram->mockKernelInfo, rootDeviceIndex);
        pMultiDeviceKernel = MockMultiDeviceKernel::create<MockKernelWithApplicableWa>(pProgram, kernelInfos);
        pKernel = static_cast<MockKernelWithApplicableWa *>(pMultiDeviceKernel->getKernel(rootDeviceIndex));
    }

    void TearDown() override {
        pMultiDeviceKernel->release();
        pProgram->release();
        pCommandQueue->release();
        pContext->release();

        LinearStreamFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    CommandQueue *pCommandQueue = nullptr;
    Context *pContext = nullptr;
    MockProgram *pProgram = nullptr;
    MockKernelWithApplicableWa *pKernel = nullptr;
    MultiDeviceKernel *pMultiDeviceKernel = nullptr;
};

GEN12LPTEST_F(HardwareInterfaceTests, GivenKernelWithApplicableWaDisableRccRhwoOptimizationWhenDispatchWorkaroundsIsCalledThenWorkaroundIsApplied) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    bool enableWa = true;
    pKernel->waApplicable = true;

    HardwareInterface<FamilyType>::dispatchWorkarounds(&linearStream, *pCommandQueue, *pKernel, enableWa);
    size_t expectedUsedForEnableWa = (sizeof(PIPE_CONTROL) + sizeof(MI_LOAD_REGISTER_IMM));
    ASSERT_EQ(expectedUsedForEnableWa, linearStream.getUsed());

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(linearStream);
    auto itorPipeCtrl = find<PIPE_CONTROL *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    ASSERT_NE(hwParse.cmdList.end(), itorPipeCtrl);
    auto pipeControl = genCmdCast<PIPE_CONTROL *>(*itorPipeCtrl);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());

    auto itorLri = find<MI_LOAD_REGISTER_IMM *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    ASSERT_NE(hwParse.cmdList.end(), itorLri);
    auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorLri);
    ASSERT_NE(nullptr, lriCmd);
    EXPECT_EQ(0x7010u, lriCmd->getRegisterOffset());
    EXPECT_EQ(0x40004000u, lriCmd->getDataDword());

    enableWa = false;
    HardwareInterface<FamilyType>::dispatchWorkarounds(&linearStream, *pCommandQueue, *pKernel, enableWa);
    size_t expectedUsedForDisableWa = 2 * (sizeof(PIPE_CONTROL) + sizeof(MI_LOAD_REGISTER_IMM));
    ASSERT_EQ(expectedUsedForDisableWa, linearStream.getUsed());

    hwParse.tearDown();
    hwParse.parseCommands<FamilyType>(linearStream, expectedUsedForEnableWa);
    itorPipeCtrl = find<PIPE_CONTROL *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    ASSERT_NE(hwParse.cmdList.end(), itorPipeCtrl);
    pipeControl = genCmdCast<PIPE_CONTROL *>(*itorPipeCtrl);
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());

    itorLri = find<MI_LOAD_REGISTER_IMM *>(hwParse.cmdList.begin(), hwParse.cmdList.end());
    ASSERT_NE(hwParse.cmdList.end(), itorLri);
    lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorLri);
    ASSERT_NE(nullptr, lriCmd);
    EXPECT_EQ(0x7010u, lriCmd->getRegisterOffset());
    EXPECT_EQ(0x40000000u, lriCmd->getDataDword());
}

GEN12LPTEST_F(HardwareInterfaceTests, GivenKernelWithoutApplicableWaDisableRccRhwoOptimizationWhenDispatchWorkaroundsIsCalledThenWorkaroundIsApplied) {
    bool enableWa = true;
    pKernel->waApplicable = false;

    HardwareInterface<FamilyType>::dispatchWorkarounds(&linearStream, *pCommandQueue, *pKernel, enableWa);
    EXPECT_EQ(0u, linearStream.getUsed());

    enableWa = false;
    HardwareInterface<FamilyType>::dispatchWorkarounds(&linearStream, *pCommandQueue, *pKernel, enableWa);
    EXPECT_EQ(0u, linearStream.getUsed());
}

GEN12LPTEST_F(HardwareInterfaceTests, GivenKernelWithApplicableWaDisableRccRhwoOptimizationWhenCalculatingCommandsSizeThenAppropriateSizeIsReturned) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    pKernel->waApplicable = true;
    auto cmdSize = GpgpuWalkerHelper<FamilyType>::getSizeForWaDisableRccRhwoOptimization(pKernel);
    size_t expectedSize = 2 * (sizeof(PIPE_CONTROL) + sizeof(MI_LOAD_REGISTER_IMM));
    EXPECT_EQ(expectedSize, cmdSize);
}

GEN12LPTEST_F(HardwareInterfaceTests, GivenKernelWithoutApplicableWaDisableRccRhwoOptimizationWhenCalculatingCommandsSizeThenZeroIsReturned) {
    pKernel->waApplicable = false;
    auto cmdSize = GpgpuWalkerHelper<FamilyType>::getSizeForWaDisableRccRhwoOptimization(pKernel);
    EXPECT_EQ(0u, cmdSize);
}

} // namespace NEO
