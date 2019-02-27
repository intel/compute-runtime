/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "runtime/builtin_kernels_simulation/scheduler_simulation.h"
#include "runtime/command_queue/gpgpu_walker.h"
#include "runtime/command_queue/local_id_gen.h"
#include "runtime/device_queue/device_queue_hw.h"
#include "runtime/event/user_event.h"
#include "runtime/helpers/per_thread_data.h"
#include "runtime/kernel/kernel.h"
#include "unit_tests/fixtures/device_host_queue_fixture.h"
#include "unit_tests/fixtures/execution_model_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/gtest_helpers.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_device_queue.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/mocks/mock_mdi.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"
#include "unit_tests/utilities/base_object_utils.h"

using namespace OCLRT;

static const char *binaryFile = "simple_block_kernel";
static const char *KernelNames[] = {"kernel_reflection", "simple_block_kernel"};

typedef ExecutionModelKernelTest ParentKernelEnqueueTest;

HWCMDTEST_P(IGFX_GEN8_CORE, ParentKernelEnqueueTest, givenParentKernelWhenEnqueuedThenDeviceQueueDSHHasCorrectlyFilledInterfaceDesriptorTables) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {
        DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);

        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        pKernel->createReflectionSurface();

        BlockKernelManager *blockManager = pProgram->getBlockKernelManager();
        uint32_t blockCount = static_cast<uint32_t>(blockManager->getCount());

        auto *executionModelDshAllocation = pDevQueueHw->getDshBuffer();
        void *executionModelDsh = executionModelDshAllocation->getUnderlyingBuffer();

        EXPECT_NE(nullptr, executionModelDsh);

        INTERFACE_DESCRIPTOR_DATA *idData = static_cast<INTERFACE_DESCRIPTOR_DATA *>(ptrOffset(executionModelDsh, DeviceQueue::colorCalcStateSize));

        size_t executionModelDSHUsedBefore = pDevQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE)->getUsed();
        uint32_t colorCalcSize = DeviceQueue::colorCalcStateSize;
        EXPECT_EQ(colorCalcSize, executionModelDSHUsedBefore);

        MockMultiDispatchInfo multiDispatchInfo(pKernel);

        auto graphicsAllocation = pKernel->getKernelInfo().getGraphicsAllocation();
        auto kernelIsaAddress = graphicsAllocation->getGpuAddressToPatch();

        uint64_t lowPart = kernelIsaAddress & 0xffffffff;
        uint64_t hightPart = (kernelIsaAddress & 0xffffffff00000000) >> 32;

        pCmdQ->enqueueKernel(pKernel, 1, globalOffsets, workItems, workItems, 0, nullptr, nullptr);

        if (pKernel->getKernelInfo().name == "kernel_reflection") {
            EXPECT_NE(0u, idData[0].getSamplerCount());
            EXPECT_NE(0u, idData[0].getSamplerStatePointer());
        }

        EXPECT_NE(0u, idData[0].getConstantIndirectUrbEntryReadLength());
        EXPECT_NE(0u, idData[0].getCrossThreadConstantDataReadLength());
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::DENORM_MODE_SETBYKERNEL, idData[0].getDenormMode());
        EXPECT_EQ((uint32_t)lowPart, idData[0].getKernelStartPointer());
        EXPECT_EQ((uint32_t)hightPart, idData[0].getKernelStartPointerHigh());

        const uint32_t blockFirstIndex = 1;

        for (uint32_t i = 0; i < blockCount; i++) {
            const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);

            ASSERT_NE(nullptr, pBlockInfo);
            ASSERT_NE(nullptr, pBlockInfo->patchInfo.dataParameterStream);
            ASSERT_NE(nullptr, pBlockInfo->patchInfo.executionEnvironment);
            ASSERT_NE(nullptr, pBlockInfo->patchInfo.threadPayload);

            const uint32_t sizeCrossThreadData = pBlockInfo->patchInfo.dataParameterStream->DataParameterStreamSize / sizeof(GRF);

            auto numChannels = PerThreadDataHelper::getNumLocalIdChannels(*pBlockInfo->patchInfo.threadPayload);
            auto sizePerThreadData = getPerThreadSizeLocalIDs(pBlockInfo->patchInfo.executionEnvironment->LargestCompiledSIMDSize, numChannels);
            uint32_t numGrfPerThreadData = static_cast<uint32_t>(sizePerThreadData / sizeof(GRF));
            numGrfPerThreadData = std::max(numGrfPerThreadData, 1u);

            EXPECT_EQ(numGrfPerThreadData, idData[blockFirstIndex + i].getConstantIndirectUrbEntryReadLength());
            EXPECT_EQ(sizeCrossThreadData, idData[blockFirstIndex + i].getCrossThreadConstantDataReadLength());
            EXPECT_NE((uint64_t)0u, ((uint64_t)idData[blockFirstIndex + i].getKernelStartPointerHigh() << 32) | (uint64_t)idData[blockFirstIndex + i].getKernelStartPointer());

            uint64_t kernelAddress = ((uint64_t)idData[blockFirstIndex + i].getKernelStartPointerHigh() << 32) | (uint64_t)idData[blockFirstIndex + i].getKernelStartPointer();
            EXPECT_EQ(pBlockInfo->getGraphicsAllocation()->getGpuAddressToPatch(), kernelAddress);
        }
    }
}

HWTEST_P(ParentKernelEnqueueTest, GivenParentKernelWithPrivateSurfaceWhenEnqueueKernelCalledThenResidencyCountIncreased) {
    if (pDevice->getSupportedClVersion() >= 20) {
        size_t offset[3] = {0, 0, 0};
        size_t gws[3] = {1, 1, 1};
        int32_t executionStamp = 0;
        auto mockCSR = new MockCsr<FamilyType>(executionStamp, *pDevice->executionEnvironment);
        pDevice->resetCommandStreamReceiver(mockCSR);
        GraphicsAllocation *privateSurface = mockCSR->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});

        pKernel->getProgram()->getBlockKernelManager()->pushPrivateSurface(privateSurface, 0);
        pCmdQ->enqueueKernel(pKernel, 1, offset, gws, gws, 0, nullptr, nullptr);

        EXPECT_TRUE(privateSurface->isResident(mockCSR->getOsContext().getContextId()));
    }
}

HWTEST_P(ParentKernelEnqueueTest, GivenBlocksWithPrivateMemoryWhenEnqueueKernelThatIsBlockedByUserEventIsCalledThenPrivateAllocationIsMadeResidentWhenEventUnblocks) {
    if (pDevice->getSupportedClVersion() >= 20) {
        size_t offset[3] = {0, 0, 0};
        size_t gws[3] = {1, 1, 1};

        auto blockKernelManager = pKernel->getProgram()->getBlockKernelManager();
        auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
        csr.storeMakeResidentAllocations = true;

        auto privateAllocation = csr.getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
        blockKernelManager->pushPrivateSurface(privateAllocation, 0);

        auto uEvent = make_releaseable<UserEvent>(pContext);
        auto clEvent = static_cast<cl_event>(uEvent.get());

        pCmdQ->enqueueKernel(pKernel, 1, offset, gws, gws, 1, &clEvent, nullptr);

        EXPECT_FALSE(csr.isMadeResident(privateAllocation));
        uEvent->setStatus(CL_COMPLETE);
        EXPECT_TRUE(csr.isMadeResident(privateAllocation));
    }
}

HWTEST_P(ParentKernelEnqueueTest, GivenParentKernelWithBlocksWhenEnqueueKernelIsCalledThenBlockKernelIsaAllocationIsMadeResident) {
    if (pDevice->getSupportedClVersion() >= 20) {
        size_t offset[3] = {0, 0, 0};
        size_t gws[3] = {1, 1, 1};

        auto blockKernelManager = pKernel->getProgram()->getBlockKernelManager();
        auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
        csr.storeMakeResidentAllocations = true;

        pCmdQ->enqueueKernel(pKernel, 1, offset, gws, gws, 0, nullptr, nullptr);

        auto blockCount = blockKernelManager->getCount();
        for (auto blockId = 0u; blockId < blockCount; blockId++) {
            EXPECT_TRUE(csr.isMadeResident(blockKernelManager->getBlockKernelInfo(blockId)->getGraphicsAllocation()));
        }
    }
}

HWTEST_P(ParentKernelEnqueueTest, GivenBlockKernelManagerFilledWithBlocksWhenMakeInternalAllocationsResidentIsCalledThenAllSurfacesAreMadeResident) {
    if (pDevice->getSupportedClVersion() >= 20) {
        auto blockKernelManager = pKernel->getProgram()->getBlockKernelManager();
        auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
        csr.storeMakeResidentAllocations = true;

        blockKernelManager->makeInternalAllocationsResident(csr);

        auto blockCount = blockKernelManager->getCount();
        for (auto blockId = 0u; blockId < blockCount; blockId++) {
            EXPECT_TRUE(csr.isMadeResident(blockKernelManager->getBlockKernelInfo(blockId)->getGraphicsAllocation()));
        }
    }
}

HWTEST_P(ParentKernelEnqueueTest, GivenParentKernelWithBlocksWhenEnqueueKernelThatIsBlockedByUserEventIsCalledThenBlockKernelIsaAllocationIsMadeResidentWhenEventUnblocks) {
    if (pDevice->getSupportedClVersion() >= 20) {
        size_t offset[3] = {0, 0, 0};
        size_t gws[3] = {1, 1, 1};

        auto blockKernelManager = pKernel->getProgram()->getBlockKernelManager();
        auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
        csr.storeMakeResidentAllocations = true;

        auto uEvent = make_releaseable<UserEvent>(pContext);
        auto clEvent = static_cast<cl_event>(uEvent.get());

        pCmdQ->enqueueKernel(pKernel, 1, offset, gws, gws, 1, &clEvent, nullptr);

        auto blockCount = blockKernelManager->getCount();
        for (auto blockId = 0u; blockId < blockCount; blockId++) {
            EXPECT_FALSE(csr.isMadeResident(blockKernelManager->getBlockKernelInfo(blockId)->getGraphicsAllocation()));
        }

        uEvent->setStatus(CL_COMPLETE);

        for (auto blockId = 0u; blockId < blockCount; blockId++) {
            EXPECT_TRUE(csr.isMadeResident(blockKernelManager->getBlockKernelInfo(blockId)->getGraphicsAllocation()));
        }
    }
}

HWTEST_P(ParentKernelEnqueueTest, givenParentKernelWhenEnqueuedSecondTimeThenDeviceQueueDSHIsResetToInitialOffset) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {
        DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);

        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        auto dsh = pDevQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE);
        size_t executionModelDSHUsedBefore = dsh->getUsed();

        uint32_t colorCalcSize = DeviceQueue::colorCalcStateSize;
        EXPECT_EQ(colorCalcSize, executionModelDSHUsedBefore);

        MockMultiDispatchInfo multiDispatchInfo(pKernel);

        pCmdQ->enqueueKernel(pKernel, 1, globalOffsets, workItems, workItems, 0, nullptr, nullptr);

        size_t executionModelDSHUsedAfterFirst = dsh->getUsed();
        EXPECT_LT(executionModelDSHUsedBefore, executionModelDSHUsedAfterFirst);

        pDevQueueHw->resetDeviceQueue();

        pCmdQ->enqueueKernel(pKernel, 1, globalOffsets, workItems, workItems, 0, nullptr, nullptr);

        size_t executionModelDSHUsedAfterSecond = dsh->getUsed();
        EXPECT_EQ(executionModelDSHUsedAfterFirst, executionModelDSHUsedAfterSecond);
    }
}

HWTEST_P(ParentKernelEnqueueTest, givenParentKernelWhenEnqueuedThenBlocksSurfaceStatesAreCopied) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {

        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        pKernel->createReflectionSurface();

        BlockKernelManager *blockManager = pProgram->getBlockKernelManager();
        uint32_t blockCount = static_cast<uint32_t>(blockManager->getCount());

        size_t parentKernelSSHSize = pKernel->getSurfaceStateHeapSize();

        MockMultiDispatchInfo multiDispatchInfo(pKernel);

        auto ssh = &getIndirectHeap<FamilyType, IndirectHeap::SURFACE_STATE>(*pCmdQ, multiDispatchInfo);
        // prealign the ssh so that it won't need to be realigned in enqueueKernel
        // this way, we can assume the location in memory into which the surface states
        // will be coies
        ssh->align(BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);

        // mark the assumed place for surface states
        size_t parentSshOffset = ssh->getUsed();

        pCmdQ->enqueueKernel(pKernel, 1, globalOffsets, workItems, workItems, 0, nullptr, nullptr);

        void *blockSSH = ptrOffset(ssh->getCpuBase(), parentSshOffset + parentKernelSSHSize); // note : unaligned at this point

        for (uint32_t i = 0; i < blockCount; i++) {
            const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);

            ASSERT_NE(nullptr, pBlockInfo);
            ASSERT_NE(nullptr, pBlockInfo->patchInfo.dataParameterStream);
            ASSERT_NE(nullptr, pBlockInfo->patchInfo.executionEnvironment);
            ASSERT_NE(nullptr, pBlockInfo->patchInfo.threadPayload);

            Kernel *blockKernel = Kernel::create(pKernel->getProgram(), *pBlockInfo, nullptr);
            blockSSH = alignUp(blockSSH, BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);
            if (blockKernel->getNumberOfBindingTableStates() > 0) {
                ASSERT_NE(nullptr, pBlockInfo->patchInfo.bindingTableState);
                auto dstBlockBti = ptrOffset(blockSSH, pBlockInfo->patchInfo.bindingTableState->Offset);
                EXPECT_EQ(0U, reinterpret_cast<uintptr_t>(dstBlockBti) % INTERFACE_DESCRIPTOR_DATA::BINDINGTABLEPOINTER_ALIGN_SIZE);
                auto dstBindingTable = reinterpret_cast<BINDING_TABLE_STATE *>(dstBlockBti);

                auto srcBlockBti = ptrOffset(pBlockInfo->heapInfo.pSsh, pBlockInfo->patchInfo.bindingTableState->Offset);
                auto srcBindingTable = reinterpret_cast<BINDING_TABLE_STATE *>(srcBlockBti);
                for (uint32_t i = 0; i < blockKernel->getNumberOfBindingTableStates(); ++i) {
                    uint32_t dstSurfaceStatePointer = dstBindingTable[i].getSurfaceStatePointer();
                    uint32_t srcSurfaceStatePointer = srcBindingTable[i].getSurfaceStatePointer();
                    auto *dstSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(ssh->getCpuBase(), dstSurfaceStatePointer));
                    auto *srcSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ptrOffset(pBlockInfo->heapInfo.pSsh, srcSurfaceStatePointer));
                    EXPECT_EQ(0, memcmp(srcSurfaceState, dstSurfaceState, sizeof(RENDER_SURFACE_STATE)));
                }

                blockSSH = ptrOffset(blockSSH, blockKernel->getSurfaceStateHeapSize());
            }

            delete blockKernel;
        }
    }
}

HWTEST_P(ParentKernelEnqueueTest, givenParentKernelWhenEnqueuedThenReflectionSurfaceIsCreated) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {

        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        MockMultiDispatchInfo multiDispatchInfo(pKernel);
        pCmdQ->enqueueKernel(pKernel, 1, globalOffsets, workItems, workItems, 0, nullptr, nullptr);

        EXPECT_NE(nullptr, pKernel->getKernelReflectionSurface());
    }
}

HWCMDTEST_P(IGFX_GEN8_CORE, ParentKernelEnqueueTest, givenBlockedQueueWhenParentKernelIsEnqueuedThenDeviceQueueIsNotReset) {
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {

        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};
        cl_queue_properties properties[3] = {0};

        MockMultiDispatchInfo multiDispatchInfo(pKernel);
        MockDeviceQueueHw<FamilyType> mockDevQueue(context, pDevice, properties[0]);

        context->setDefaultDeviceQueue(&mockDevQueue);
        // Acquire CS to check if reset queue was called
        mockDevQueue.acquireEMCriticalSection();

        auto mockEvent = make_releaseable<UserEvent>(context);

        cl_event eventBlocking = mockEvent.get();

        pCmdQ->enqueueKernel(pKernel, 1, globalOffsets, workItems, workItems, 1, &eventBlocking, nullptr);

        EXPECT_FALSE(mockDevQueue.isEMCriticalSectionFree());
    }
}

HWCMDTEST_P(IGFX_GEN8_CORE, ParentKernelEnqueueTest, givenNonBlockedQueueWhenParentKernelIsEnqueuedThenDeviceQueueDSHAddressIsProgrammedInStateBaseAddressAndDSHIsMadeResident) {
    typedef typename FamilyType::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {
        DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);
        ASSERT_NE(nullptr, pDevQueueHw);

        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        MockMultiDispatchInfo multiDispatchInfo(pKernel);

        int32_t executionStamp = 0;
        auto mockCSR = new MockCsrBase<FamilyType>(executionStamp, *pDevice->executionEnvironment);
        pDevice->resetCommandStreamReceiver(mockCSR);

        pCmdQ->enqueueKernel(pKernel, 1, globalOffsets, workItems, workItems, 0, nullptr, nullptr);

        auto &cmdStream = mockCSR->getCS(0);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(cmdStream, 0);
        hwParser.findHardwareCommands<FamilyType>();

        auto stateBaseAddressItor = hwParser.itorStateBaseAddress;

        ASSERT_NE(hwParser.cmdList.end(), stateBaseAddressItor);

        auto *stateBaseAddress = (STATE_BASE_ADDRESS *)*stateBaseAddressItor;

        uint64_t addressProgrammed = stateBaseAddress->getDynamicStateBaseAddress();

        EXPECT_EQ(addressProgrammed, pDevQueue->getDshBuffer()->getGpuAddress());

        bool dshAllocationResident = false;

        for (uint32_t i = 0; i < mockCSR->madeResidentGfxAllocations.size(); i++) {
            if (mockCSR->madeResidentGfxAllocations[i] == pDevQueue->getDshBuffer()) {
                dshAllocationResident = true;
                break;
            }
        }
        EXPECT_TRUE(dshAllocationResident);
    }
}

INSTANTIATE_TEST_CASE_P(ParentKernelEnqueueTest,
                        ParentKernelEnqueueTest,
                        ::testing::Combine(
                            ::testing::Values(binaryFile),
                            ::testing::ValuesIn(KernelNames)));

class ParentKernelEnqueueFixture : public ExecutionModelSchedulerTest,
                                   public testing::Test {
  public:
    void SetUp() override {
        ExecutionModelSchedulerTest::SetUp();
    }

    void TearDown() override {
        ExecutionModelSchedulerTest::TearDown();
    }
};

TEST_F(ParentKernelEnqueueFixture, GivenParentKernelWhenEnqueuedTheDefaultDeviceQueueAndEventPoolIsPatched) {

    if (pDevice->getSupportedClVersion() >= 20) {
        size_t offset[3] = {0, 0, 0};
        size_t gws[3] = {1, 1, 1};

        pCmdQ->enqueueKernel(parentKernel, 1, offset, gws, gws, 0, nullptr, nullptr);

        const auto &patchInfo = parentKernel->getKernelInfo().patchInfo;

        if (patchInfo.pAllocateStatelessDefaultDeviceQueueSurface) {
            auto patchLocation = ptrOffset(reinterpret_cast<uint64_t *>(parentKernel->getCrossThreadData()),
                                           patchInfo.pAllocateStatelessDefaultDeviceQueueSurface->DataParamOffset);

            EXPECT_EQ(pDevQueue->getQueueBuffer()->getGpuAddress(), *patchLocation);
        }

        if (patchInfo.pAllocateStatelessEventPoolSurface) {
            auto patchLocation = ptrOffset(reinterpret_cast<uint64_t *>(parentKernel->getCrossThreadData()),
                                           patchInfo.pAllocateStatelessEventPoolSurface->DataParamOffset);

            EXPECT_EQ(pDevQueue->getEventPoolBuffer()->getGpuAddress(), *patchLocation);
        }
    }
}

HWTEST_F(ParentKernelEnqueueFixture, GivenParentKernelWhenEnqueuedThenBlocksDSHOnReflectionSurfaceArePatchedWithDeviceQueueAndEventPoolAddresses) {

    if (pDevice->getSupportedClVersion() >= 20) {
        size_t offset[3] = {0, 0, 0};
        size_t gws[3] = {1, 1, 1};
        DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);

        pCmdQ->enqueueKernel(parentKernel, 1, offset, gws, gws, 0, nullptr, nullptr);

        void *reflectionSurface = parentKernel->getKernelReflectionSurface()->getUnderlyingBuffer();

        BlockKernelManager *blockManager = parentKernel->getProgram()->getBlockKernelManager();
        uint32_t blockCount = static_cast<uint32_t>(blockManager->getCount());

        for (uint32_t i = 0; i < blockCount; i++) {
            const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);

            uint32_t defaultQueueOffset = pBlockInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface->DataParamOffset;
            uint32_t eventPoolOffset = pBlockInfo->patchInfo.pAllocateStatelessEventPoolSurface->DataParamOffset;

            uint32_t defaultQueueSize = pBlockInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface->DataParamSize;
            uint32_t eventPoolSize = pBlockInfo->patchInfo.pAllocateStatelessEventPoolSurface->DataParamSize;

            uint32_t offset = MockKernel::ReflectionSurfaceHelperPublic::getConstantBufferOffset(reflectionSurface, i);

            if (defaultQueueSize == sizeof(uint64_t)) {
                EXPECT_EQ_VAL(pDevQueueHw->getQueueBuffer()->getGpuAddressToPatch(), *(uint64_t *)ptrOffset(reflectionSurface, offset + defaultQueueOffset));
            } else {
                EXPECT_EQ((uint32_t)pDevQueueHw->getQueueBuffer()->getGpuAddressToPatch(), *(uint32_t *)ptrOffset(reflectionSurface, offset + defaultQueueOffset));
            }

            if (eventPoolSize == sizeof(uint64_t)) {
                EXPECT_EQ_VAL(pDevQueueHw->getEventPoolBuffer()->getGpuAddressToPatch(), *(uint64_t *)ptrOffset(reflectionSurface, offset + eventPoolOffset));
            } else {
                EXPECT_EQ((uint32_t)pDevQueueHw->getEventPoolBuffer()->getGpuAddressToPatch(), *(uint32_t *)ptrOffset(reflectionSurface, offset + eventPoolOffset));
            }
        }
    }
}

HWTEST_F(ParentKernelEnqueueFixture, GivenParentKernelWhenEnqueuedToNonBlockedQueueThenDeviceQueueCriticalSetionIsAcquired) {

    if (pDevice->getSupportedClVersion() >= 20) {
        size_t offset[3] = {0, 0, 0};
        size_t gws[3] = {1, 1, 1};
        DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);

        EXPECT_TRUE(pDevQueueHw->isEMCriticalSectionFree());

        pCmdQ->enqueueKernel(parentKernel, 1, offset, gws, gws, 0, nullptr, nullptr);

        EXPECT_FALSE(pDevQueueHw->isEMCriticalSectionFree());
    }
}

HWTEST_F(ParentKernelEnqueueFixture, GivenParentKernelWhenEnqueuedToBlockedQueueThenDeviceQueueCriticalSetionIsNotAcquired) {

    if (pDevice->getSupportedClVersion() >= 20) {
        size_t offset[3] = {0, 0, 0};
        size_t gws[3] = {1, 1, 1};
        DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);

        auto mockEvent = make_releaseable<MockEvent<UserEvent>>(context);
        cl_event eventBlocking = mockEvent.get();

        EXPECT_TRUE(pDevQueueHw->isEMCriticalSectionFree());

        pCmdQ->enqueueKernel(parentKernel, 1, offset, gws, gws, 1, &eventBlocking, nullptr);

        EXPECT_TRUE(pDevQueueHw->isEMCriticalSectionFree());
        mockEvent->setStatus(-1);
    }
}

HWTEST_F(ParentKernelEnqueueFixture, ParentKernelEnqueuedToNonBlockedQueueFlushesCSRWithSLM) {

    if (pDevice->getSupportedClVersion() >= 20) {
        size_t offset[3] = {0, 0, 0};
        size_t gws[3] = {1, 1, 1};
        int32_t execStamp;
        auto mockCsr = new MockCsr<FamilyType>(execStamp, *pDevice->executionEnvironment);
        pDevice->resetCommandStreamReceiver(mockCsr);

        pCmdQ->enqueueKernel(parentKernel, 1, offset, gws, gws, 0, nullptr, nullptr);

        EXPECT_TRUE(mockCsr->slmUsedInLastFlushTask);
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, ParentKernelEnqueueFixture, ParentKernelEnqueuedWithSchedulerReturnInstanceRunsSimulation) {

    if (pDevice->getSupportedClVersion() >= 20) {

        DebugManagerStateRestore dbgRestorer;
        DebugManager.flags.SchedulerSimulationReturnInstance.set(1);

        MockDeviceQueueHw<FamilyType> *mockDeviceQueueHw = new MockDeviceQueueHw<FamilyType>(context, pDevice, DeviceHostQueue::deviceQueueProperties::minimumProperties[0]);
        mockDeviceQueueHw->resetDeviceQueue();

        context->setDefaultDeviceQueue(mockDeviceQueueHw);

        size_t offset[3] = {0, 0, 0};
        size_t gws[3] = {1, 1, 1};
        int32_t execStamp;
        auto mockCsr = new MockCsr<FamilyType>(execStamp, *pDevice->executionEnvironment);

        BuiltinKernelsSimulation::SchedulerSimulation<FamilyType>::enabled = false;

        pDevice->resetCommandStreamReceiver(mockCsr);

        pCmdQ->enqueueKernel(parentKernel, 1, offset, gws, gws, 0, nullptr, nullptr);

        BuiltinKernelsSimulation::SchedulerSimulation<FamilyType>::enabled = true;

        EXPECT_TRUE(BuiltinKernelsSimulation::SchedulerSimulation<FamilyType>::simulationRun);
        delete mockDeviceQueueHw;
    }
}

HWTEST_F(ParentKernelEnqueueFixture, givenCsrInBatchingModeWhenExecutionModelKernelIsSubmittedThenItIsFlushed) {
    if (pDevice->getSupportedClVersion() >= 20) {
        auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
        mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
        pDevice->resetCommandStreamReceiver(mockCsr);

        auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
        mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

        size_t offset[3] = {0, 0, 0};
        size_t gws[3] = {1, 1, 1};

        MockContext context(pDevice);
        std::unique_ptr<MockParentKernel> kernelToRun(MockParentKernel::create(context, false, false, false, false, false));

        pCmdQ->enqueueKernel(kernelToRun.get(), 1, offset, gws, gws, 0, nullptr, nullptr);

        EXPECT_TRUE(mockedSubmissionsAggregator->peekCmdBufferList().peekIsEmpty());
        EXPECT_EQ(1, mockCsr->flushCalledCount);
    }
}

HWTEST_F(ParentKernelEnqueueFixture, ParentKernelEnqueueMarksCSRMediaVFEStateDirty) {

    if (pDevice->getSupportedClVersion() >= 20) {
        size_t offset[3] = {0, 0, 0};
        size_t gws[3] = {1, 1, 1};
        int32_t execStamp;
        auto mockCsr = new MockCsr<FamilyType>(execStamp, *pDevice->executionEnvironment);
        pDevice->resetCommandStreamReceiver(mockCsr);

        mockCsr->setMediaVFEStateDirty(false);
        pCmdQ->enqueueKernel(parentKernel, 1, offset, gws, gws, 0, nullptr, nullptr);

        EXPECT_TRUE(mockCsr->peekMediaVfeStateDirty());
    }
}
