/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm_device_command_stream.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/helpers/cl_execution_environment_helper.h"
#include "opencl/test/unit_test/helpers/cl_hw_parse.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

using namespace NEO;

struct EnqueueBufferWindowsTest : public ClHardwareParse,
                                  public ::testing::Test {
    EnqueueBufferWindowsTest(void)
        : buffer(nullptr) {
    }

    void SetUp() override {
        debugManager.flags.EnableBlitterForEnqueueOperations.set(0);
        executionEnvironment = getClExecutionEnvironmentImpl(hwInfo, 1);
    }

    void TearDown() override {
        buffer.reset(nullptr);
    }

    template <typename FamilyType>
    void initializeFixture() {
        EnvironmentWithCsrWrapper environment;
        environment.setCsrType<WddmCommandStreamReceiver<FamilyType>>();

        memoryManager = new MockWddmMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        device = std::make_unique<MockClDevice>(Device::create<MockDevice>(executionEnvironment, rootDeviceIndex));
        context = std::make_unique<MockContext>(device.get());

        const size_t bufferMisalignment = 1;
        const size_t bufferSize = 16;
        bufferMemory = std::make_unique<uint32_t[]>(alignUp(bufferSize + bufferMisalignment, sizeof(uint32_t)));
        cl_int retVal = 0;

        buffer.reset(Buffer::create(context.get(),
                                    CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                    bufferSize,
                                    reinterpret_cast<char *>(bufferMemory.get()) + bufferMisalignment,
                                    retVal));

        EXPECT_EQ(CL_SUCCESS, retVal);
    }

  protected:
    DebugManagerStateRestore restore;
    HardwareInfo hardwareInfo;
    HardwareInfo *hwInfo = nullptr;
    ExecutionEnvironment *executionEnvironment;
    cl_queue_properties properties = {};
    std::unique_ptr<uint32_t[]> bufferMemory;
    std::unique_ptr<MockClDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<Buffer> buffer;
    const uint32_t rootDeviceIndex = 0u;

    MockWddmMemoryManager *memoryManager = nullptr;
};

HWTEST_F(EnqueueBufferWindowsTest, givenMisalignedHostPtrWhenEnqueueReadBufferCalledThenStateBaseAddressAddressIsAlignedAndMatchesKernelDispatchInfoParams) {
    if (executionEnvironment->memoryManager->isLimitedGPU(0)) {
        GTEST_SKIP();
    }
    initializeFixture<FamilyType>();
    const auto &compilerProductHelper = device->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    if (compilerProductHelper.isForceToStatelessRequired()) {
        GTEST_SKIP();
    }
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), &properties);
    char *misalignedPtr = reinterpret_cast<char *>(device->getMemoryManager()->getAlignedMallocRestrictions()->minAddress + 1);

    buffer->forceDisallowCPUCopy = true;
    auto retVal = cmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 4, misalignedPtr, nullptr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(0u, cmdQ->lastEnqueuedKernels.size());
    Kernel *kernel = cmdQ->lastEnqueuedKernels[0];

    auto hostPtrAllocation = cmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage()->getTemporaryAllocations().peekHead();

    while (hostPtrAllocation != nullptr) {
        if (hostPtrAllocation->getUnderlyingBuffer() == misalignedPtr) {
            break;
        }
        hostPtrAllocation = hostPtrAllocation->next;
    }
    ASSERT_NE(nullptr, hostPtrAllocation);

    uint64_t gpuVa = hostPtrAllocation->getGpuAddress();
    cmdQ->finish(false);

    parseCommands<FamilyType>(*cmdQ);
    auto &kernelInfo = kernel->getKernelInfo();

    if (hwInfo->capabilityTable.gpuAddressSpace == MemoryConstants::max48BitAddress) {
        const auto &surfaceStateDst = getSurfaceState<FamilyType>(&cmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0), 1);

        const auto &arg1AsPtr = kernelInfo.getArgDescriptorAt(1).as<ArgDescPointer>();
        if (arg1AsPtr.pointerSize == sizeof(uint64_t)) {
            auto pKernelArg = (uint64_t *)(kernel->getCrossThreadData() + arg1AsPtr.stateless);
            EXPECT_EQ(alignDown(gpuVa, 4), static_cast<uint64_t>(*pKernelArg));
            EXPECT_EQ(*pKernelArg, surfaceStateDst->getSurfaceBaseAddress());

        } else if (arg1AsPtr.pointerSize == sizeof(uint32_t)) {
            auto pKernelArg = (uint32_t *)(kernel->getCrossThreadData() + arg1AsPtr.stateless);
            EXPECT_EQ(alignDown(gpuVa, 4), static_cast<uint64_t>(*pKernelArg));
            EXPECT_EQ(static_cast<uint64_t>(*pKernelArg), surfaceStateDst->getSurfaceBaseAddress());
        }
    }

    auto arg3AsVal = kernelInfo.getArgDescriptorAt(3).as<ArgDescValue>();
    EXPECT_EQ(sizeof(uint32_t), arg3AsVal.elements[0].size);
    auto dstOffset = (uint32_t *)(kernel->getCrossThreadData() + arg3AsVal.elements[0].offset);
    EXPECT_EQ(ptrDiff(misalignedPtr, alignDown(misalignedPtr, 4)), *dstOffset);
}
