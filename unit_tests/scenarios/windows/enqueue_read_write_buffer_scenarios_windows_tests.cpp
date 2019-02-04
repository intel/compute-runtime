/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/internal_allocation_storage.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/windows/wddm_device_command_stream.h"

#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/helpers/hw_info_helper.h"
#include "unit_tests/helpers/execution_environment_helper.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/os_interface/windows/mock_wddm_memory_manager.h"
#include "test.h"

using namespace OCLRT;

struct EnqueueBufferWindowsTest : public HardwareParse,
                                  public ::testing::Test {
    EnqueueBufferWindowsTest(void)
        : buffer(nullptr) {
    }

    void SetUp() override {
        executionEnvironment = getExecutionEnvironmentImpl(hwInfo);
    }

    void TearDown() override {
        buffer.reset(nullptr);
    }

    template <typename FamilyType>
    void initializeFixture() {
        auto wddmCsr = new WddmCommandStreamReceiver<FamilyType>(*hwInfo, *executionEnvironment);

        executionEnvironment->commandStreamReceivers.resize(1);
        executionEnvironment->commandStreamReceivers[0].push_back(std::unique_ptr<CommandStreamReceiver>(wddmCsr));

        memoryManager = new MockWddmMemoryManager(executionEnvironment->osInterface->get()->getWddm(), *executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        device = std::unique_ptr<MockDevice>(Device::create<MockDevice>(hwInfo, executionEnvironment, 0));

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
    HwInfoHelper hwInfoHelper;
    HardwareInfo *hwInfo = nullptr;
    ExecutionEnvironment *executionEnvironment;
    cl_queue_properties properties = {};
    std::unique_ptr<uint32_t[]> bufferMemory;
    std::unique_ptr<MockDevice> device;
    std::unique_ptr<MockContext> context;
    std::unique_ptr<Buffer> buffer;

    MockWddmMemoryManager *memoryManager = nullptr;
};

HWTEST_F(EnqueueBufferWindowsTest, givenMisalignedHostPtrWhenEnqueueReadBufferCalledThenStateBaseAddressAddressIsAlignedAndMatchesKernelDispatchInfoParams) {
    initializeFixture<FamilyType>();
    auto cmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(context.get(), device.get(), &properties);
    uint32_t memory[2] = {};
    char *misalignedPtr = reinterpret_cast<char *>(memory) + 1;

    buffer->forceDisallowCPUCopy = true;
    auto retVal = cmdQ->enqueueReadBuffer(buffer.get(), CL_FALSE, 0, 4, misalignedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(0, cmdQ->lastEnqueuedKernels.size());
    Kernel *kernel = cmdQ->lastEnqueuedKernels[0];

    auto hostPtrAllcoation = cmdQ->getCommandStreamReceiver().getInternalAllocationStorage()->getTemporaryAllocations().peekHead();

    while (hostPtrAllcoation != nullptr) {
        if (hostPtrAllcoation->getUnderlyingBuffer() == misalignedPtr) {
            break;
        }
        hostPtrAllcoation = hostPtrAllcoation->next;
    }
    ASSERT_NE(nullptr, hostPtrAllcoation);

    uint64_t gpuVa = hostPtrAllcoation->getGpuAddress();
    cmdQ->finish(true);

    parseCommands<FamilyType>(*cmdQ);

    if (hwInfo->capabilityTable.gpuAddressSpace == MemoryConstants::max48BitAddress) {
        const auto &surfaceStateDst = getSurfaceState<FamilyType>(&cmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0), 1);

        if (kernel->getKernelInfo().kernelArgInfo[1].kernelArgPatchInfoVector[0].size == sizeof(uint64_t)) {
            auto pKernelArg = (uint64_t *)(kernel->getCrossThreadData() +
                                           kernel->getKernelInfo().kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset);
            EXPECT_EQ(reinterpret_cast<uint64_t>(alignDown(misalignedPtr, 4)), *pKernelArg);
            EXPECT_EQ(*pKernelArg, surfaceStateDst.getSurfaceBaseAddress());

        } else if (kernel->getKernelInfo().kernelArgInfo[1].kernelArgPatchInfoVector[0].size == sizeof(uint32_t)) {
            auto pKernelArg = (uint32_t *)(kernel->getCrossThreadData() +
                                           kernel->getKernelInfo().kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset);
            EXPECT_EQ(alignDown(gpuVa, 4), static_cast<uint64_t>(*pKernelArg));
            EXPECT_EQ(static_cast<uint64_t>(*pKernelArg), surfaceStateDst.getSurfaceBaseAddress());
        }
    }

    if (kernel->getKernelInfo().kernelArgInfo[3].kernelArgPatchInfoVector[0].size == sizeof(uint32_t)) {
        auto dstOffset = (uint32_t *)(kernel->getCrossThreadData() +
                                      kernel->getKernelInfo().kernelArgInfo[3].kernelArgPatchInfoVector[0].crossthreadOffset);
        EXPECT_EQ(ptrDiff(misalignedPtr, alignDown(misalignedPtr, 4)), *dstOffset);
    } else {
        // dstOffset arg should be 4 bytes in size, if that changes, above if path should be modified
        EXPECT_TRUE(false);
    }
}
