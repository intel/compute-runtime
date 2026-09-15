/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/api/leo_api.h"
#include "level_zero/api/opencl/source/command_queue/leo_command_queue.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/kernel/leo_kernel.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/source/program/leo_program.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_command_list.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <memory>
#include <mutex>
#include <thread>

namespace NEO {
namespace LEO {
namespace ult {

struct KernelExposingOwnershipMutex : public Kernel {
    using Kernel::Kernel;
    using Kernel::mtx;
};

inline bool isLocked(std::recursive_mutex &mtx) {
    bool acquired = false;
    std::thread probe([&mtx, &acquired]() {
        if (mtx.try_lock()) {
            acquired = true;
            mtx.unlock();
        }
    });
    probe.join();
    return false == acquired;
}

struct OwnershipProbingCommandList : public CapturingCommandList {
    ze_result_t appendBarrier(ze_event_handle_t hSignalEvent, uint32_t numWaitEvents,
                              ze_event_handle_t *phWaitEvents, L0::CmdListWaitEventParameters &waitEventsParameters) override {
        UNRECOVERABLE_IF(nullptr == clKernelMutex);
        clKernelOwnedDuringBarrier = isLocked(*clKernelMutex);
        return CapturingCommandList::appendBarrier(hSignalEvent, numWaitEvents, phWaitEvents, waitEventsParameters);
    }

    std::recursive_mutex *clKernelMutex = nullptr;
    bool clKernelOwnedDuringBarrier = false;
};

struct OwnershipProbingKernel : public L0::ult::Mock<L0::KernelImp> {
    ze_result_t setGroupSize(uint32_t groupSizeX, uint32_t groupSizeY, uint32_t groupSizeZ) override {
        setGroupSizeCalled = true;
        clKernelOwnedDuringSetGroupSize = isClKernelOwned();
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t suggestGroupSize(uint32_t globalSizeX, uint32_t globalSizeY, uint32_t globalSizeZ,
                                 uint32_t *groupSizeX, uint32_t *groupSizeY, uint32_t *groupSizeZ) override {
        suggestGroupSizeCalled = true;
        clKernelOwnedDuringSuggestGroupSize = isClKernelOwned();
        *groupSizeX = 1u;
        *groupSizeY = 1u;
        *groupSizeZ = 1u;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setIndirectAccess(ze_kernel_indirect_access_flags_t flags) override {
        setIndirectAccessCalled = true;
        clKernelOwnedDuringSetIndirectAccess = isClKernelOwned();
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t setSchedulingHintExp(ze_scheduling_hint_exp_desc_t *pHint) override {
        setSchedulingHintCalled = true;
        clKernelOwnedDuringSetSchedulingHint = isClKernelOwned();
        return ZE_RESULT_SUCCESS;
    }

    const NEO::KernelDescriptor &getKernelDescriptor() const override {
        descriptorReadCount++;
        allDescriptorReadsOwned &= isClKernelOwned();
        return BaseClass::getKernelDescriptor();
    }

    bool isClKernelOwned() const {
        UNRECOVERABLE_IF(nullptr == clKernelMutex);
        return isLocked(*clKernelMutex);
    }

    void resetDescriptorProbe() {
        descriptorReadCount = 0u;
        allDescriptorReadsOwned = true;
    }

    std::recursive_mutex *clKernelMutex = nullptr;
    mutable uint32_t descriptorReadCount = 0u;
    mutable bool allDescriptorReadsOwned = true;
    bool setGroupSizeCalled = false;
    bool suggestGroupSizeCalled = false;
    bool setIndirectAccessCalled = false;
    bool setSchedulingHintCalled = false;
    bool clKernelOwnedDuringSetGroupSize = false;
    bool clKernelOwnedDuringSuggestGroupSize = false;
    bool clKernelOwnedDuringSetIndirectAccess = false;
    bool clKernelOwnedDuringSetSchedulingHint = false;
};

struct KernelOwnershipMtTests : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        device = platform->getDevices()[0].get();
        cl_device_id clDevice = device;
        context = std::make_unique<Context>(nullptr, nullptr, 1, &clDevice, true);
        commandQueue = std::make_unique<CommandQueue>(context.get(), device, nullptr, capturingCmdList.toHandle());
        l0Kernel = std::make_unique<OwnershipProbingKernel>();
        program = std::make_unique<Program>(context.get());
        std::map<uint32_t, ze_kernel_handle_t> kernelHandles{{0u, l0Kernel->toHandle()}};
        kernel = std::make_unique<KernelExposingOwnershipMutex>(std::move(kernelHandles), program.get());
        l0Kernel->clKernelMutex = &kernel->mtx;
        capturingCmdList.clKernelMutex = &kernel->mtx;
    }

    void TearDown() override {
        kernel.reset();
        program.reset();
        l0Kernel.release();
        commandQueue.reset();
        context.reset();
        Test<OclFixture>::TearDown();
    }

    ClDevice *device = nullptr;
    OwnershipProbingCommandList capturingCmdList{};
    std::unique_ptr<Context> context;
    std::unique_ptr<CommandQueue> commandQueue;
    std::unique_ptr<OwnershipProbingKernel> l0Kernel;
    std::unique_ptr<Program> program;
    std::unique_ptr<KernelExposingOwnershipMutex> kernel;
};

TEST_F(KernelOwnershipMtTests, givenEnqueueNDRangeKernelWhenGroupSizeIsSetOnSharedKernelThenClKernelIsOwned) {
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};

    auto retVal = clEnqueueNDRangeKernel(commandQueue.get(), kernel.get(), 1, globalWorkOffset, globalWorkSize, localWorkSize, 0, nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(l0Kernel->setGroupSizeCalled);
    EXPECT_TRUE(l0Kernel->clKernelOwnedDuringSetGroupSize);
}

TEST_F(KernelOwnershipMtTests, givenEnqueueNDCountKernelWhenGroupSizeIsSetOnSharedKernelThenClKernelIsOwned) {
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t workgroupCount[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};

    auto retVal = clEnqueueNDCountKernelINTEL(commandQueue.get(), kernel.get(), 1, globalWorkOffset, workgroupCount, localWorkSize, 0, nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(l0Kernel->setGroupSizeCalled);
    EXPECT_TRUE(l0Kernel->clKernelOwnedDuringSetGroupSize);
}

TEST_F(KernelOwnershipMtTests, givenEnqueueNDRangeKernelWhenExecutionTypeIsCheckedThenClKernelIsOwned) {
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    l0Kernel->resetDescriptorProbe();

    auto retVal = clEnqueueNDRangeKernel(commandQueue.get(), kernel.get(), 1, globalWorkOffset, globalWorkSize, localWorkSize, 0, nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(0u, l0Kernel->descriptorReadCount);
    EXPECT_TRUE(l0Kernel->allDescriptorReadsOwned);
}

TEST_F(KernelOwnershipMtTests, givenEnqueueNDCountKernelWhenExecutionTypeIsCheckedThenClKernelIsOwned) {
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t workgroupCount[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    l0Kernel->resetDescriptorProbe();

    auto retVal = clEnqueueNDCountKernelINTEL(commandQueue.get(), kernel.get(), 1, globalWorkOffset, workgroupCount, localWorkSize, 0, nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(0u, l0Kernel->descriptorReadCount);
    EXPECT_TRUE(l0Kernel->allDescriptorReadsOwned);
}

TEST_F(KernelOwnershipMtTests, givenSetKernelExecInfoIndirectAccessThenClKernelIsOwned) {
    cl_bool enabled = CL_TRUE;

    auto retVal = clSetKernelExecInfo(kernel.get(), CL_KERNEL_EXEC_INFO_INDIRECT_DEVICE_ACCESS_INTEL, sizeof(enabled), &enabled);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(l0Kernel->setIndirectAccessCalled);
    EXPECT_TRUE(l0Kernel->clKernelOwnedDuringSetIndirectAccess);
}

TEST_F(KernelOwnershipMtTests, givenSetKernelExecInfoThreadArbitrationPolicyThenClKernelIsOwned) {
    cl_uint policy = CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_ROUND_ROBIN_INTEL;

    auto retVal = clSetKernelExecInfo(kernel.get(), CL_KERNEL_EXEC_INFO_THREAD_ARBITRATION_POLICY_INTEL, sizeof(policy), &policy);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(l0Kernel->setSchedulingHintCalled);
    EXPECT_TRUE(l0Kernel->clKernelOwnedDuringSetSchedulingHint);
}

TEST_F(KernelOwnershipMtTests, givenNullGlobalWorkSizeWhenEnqueueNDRangeKernelThenClKernelIsNotOwnedDuringBarrier) {
    size_t globalWorkOffset[3] = {0, 0, 0};

    auto retVal = clEnqueueNDRangeKernel(commandQueue.get(), kernel.get(), 1, globalWorkOffset, nullptr, nullptr, 0, nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_TRUE(capturingCmdList.appendBarrierArgs.wasCalled());
    EXPECT_FALSE(capturingCmdList.clKernelOwnedDuringBarrier);
    EXPECT_FALSE(l0Kernel->setGroupSizeCalled);
}

TEST_F(KernelOwnershipMtTests, givenGetKernelSuggestedLocalWorkSizeWhenGroupSizeIsSuggestedOnSharedKernelThenClKernelIsOwned) {
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t suggestedLocalWorkSize[3] = {0, 0, 0};

    auto retVal = clGetKernelSuggestedLocalWorkSizeKHR(commandQueue.get(), kernel.get(), 1, nullptr, globalWorkSize, suggestedLocalWorkSize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(l0Kernel->suggestGroupSizeCalled);
    EXPECT_TRUE(l0Kernel->clKernelOwnedDuringSuggestGroupSize);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
