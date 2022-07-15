/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/page_fault_manager/mock_cpu_page_fault_manager.h"

#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/mem_obj_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

#include <memory>

using namespace NEO;

class KernelArgSvmFixture : public ContextFixture, public ClDeviceFixture {

    using ContextFixture::SetUp;

  public:
    KernelArgSvmFixture() {
    }

  protected:
    void SetUp() {
        ClDeviceFixture::SetUp();
        cl_device_id device = pClDevice;
        ContextFixture::SetUp(1, &device);

        // define kernel info
        pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

        pKernelInfo->heapInfo.pSsh = pSshLocal;
        pKernelInfo->heapInfo.SurfaceStateHeapSize = sizeof(pSshLocal);

        pKernelInfo->addArgBuffer(0, 0x30, sizeof(void *));

        pProgram = new MockProgram(pContext, false, toClDeviceVector(*pClDevice));

        pKernel = new MockKernel(pProgram, *pKernelInfo, *pClDevice);
        ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
        pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));
    }

    void TearDown() {
        delete pKernel;

        delete pProgram;
        ContextFixture::TearDown();
        ClDeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    MockProgram *pProgram = nullptr;
    MockKernel *pKernel = nullptr;
    std::unique_ptr<MockKernelInfo> pKernelInfo;
    SKernelBinaryHeaderCommon kernelHeader;
    char pSshLocal[64];
    char pCrossThreadData[64];
};

typedef Test<KernelArgSvmFixture> KernelArgSvmTest;

TEST_F(KernelArgSvmTest, GivenValidSvmPtrWhenSettingKernelArgThenSvmPtrIsCorrect) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    char *svmPtr = new char[256];

    auto retVal = pKernel->setArgSvm(0, 256, svmPtr, nullptr, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->argAsPtr(0).stateless);
    EXPECT_EQ(svmPtr, *pKernelArg);

    delete[] svmPtr;
}

HWTEST_F(KernelArgSvmTest, GivenSvmPtrStatefulWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    char *svmPtr = new char[256];

    pKernelInfo->argAsPtr(0).bindful = 0;
    auto retVal = pKernel->setArgSvm(0, 256, svmPtr, nullptr, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsPtr(0).bindful));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(svmPtr, surfaceAddress);

    delete[] svmPtr;
}

TEST_F(KernelArgSvmTest, GivenValidSvmAllocWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    char *svmPtr = new char[256];

    MockGraphicsAllocation svmAlloc(svmPtr, 256);

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr, &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->argAsPtr(0).stateless);
    EXPECT_EQ(svmPtr, *pKernelArg);

    delete[] svmPtr;
}

TEST_F(KernelArgSvmTest, GivenSvmAllocWithUncacheableWhenSettingKernelArgThenKernelHasUncacheableArgs) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    auto svmPtr = std::make_unique<char[]>(256);

    MockGraphicsAllocation svmAlloc(svmPtr.get(), 256);
    svmAlloc.setUncacheable(true);

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr.get(), &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(pKernel->hasUncacheableStatelessArgs());
}

TEST_F(KernelArgSvmTest, GivenSvmAllocWithoutUncacheableAndKenelWithUncachebleArgWhenSettingKernelArgThenKernelDoesNotHaveUncacheableArgs) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    auto svmPtr = std::make_unique<char[]>(256);

    MockGraphicsAllocation svmAlloc(svmPtr.get(), 256);
    svmAlloc.setUncacheable(true);

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr.get(), &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(pKernel->hasUncacheableStatelessArgs());

    svmAlloc.setUncacheable(false);
    pKernel->kernelArguments[0].isStatelessUncacheable = true;
    retVal = pKernel->setArgSvmAlloc(0, svmPtr.get(), &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(pKernel->hasUncacheableStatelessArgs());
}

HWTEST_F(KernelArgSvmTest, GivenValidSvmAllocStatefulWhenSettingKernelArgThenArgumentsAreSetCorrectly) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    char *svmPtr = new char[256];

    MockGraphicsAllocation svmAlloc(svmPtr, 256);

    pKernelInfo->argAsPtr(0).bindful = 0;
    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr, &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(0u, pKernel->getSurfaceStateHeapSize());

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsPtr(0).bindful));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(svmPtr, surfaceAddress);

    delete[] svmPtr;
}

HWTEST_F(KernelArgSvmTest, givenOffsetedSvmPointerWhenSetArgSvmAllocIsCalledThenProperSvmAddressIsPatched) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }
    std::unique_ptr<char[]> svmPtr(new char[256]);

    auto offsetedPtr = svmPtr.get() + 4;

    MockGraphicsAllocation svmAlloc(svmPtr.get(), 256);

    pKernelInfo->argAsPtr(0).bindful = 0;
    pKernel->setArgSvmAlloc(0, offsetedPtr, &svmAlloc, 0u);

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsPtr(0).bindful));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(offsetedPtr, surfaceAddress);
}

HWTEST_F(KernelArgSvmTest, givenDeviceSupportingSharedSystemAllocationsWhenSetArgSvmIsCalledWithSurfaceStateThenSizeIsMaxAndAddressIsProgrammed) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    this->pClDevice->deviceInfo.sharedSystemMemCapabilities = CL_UNIFIED_SHARED_MEMORY_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS_INTEL | CL_UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS_INTEL;

    auto systemPointer = reinterpret_cast<void *>(0xfeedbac);

    pKernelInfo->argAsPtr(0).bindful = 0;
    pKernel->setArgSvmAlloc(0, systemPointer, nullptr, 0u);

    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    auto surfaceState = reinterpret_cast<const RENDER_SURFACE_STATE *>(
        ptrOffset(pKernel->getSurfaceStateHeap(),
                  pKernelInfo->argAsPtr(0).bindful));

    void *surfaceAddress = reinterpret_cast<void *>(surfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(systemPointer, surfaceAddress);
    EXPECT_EQ(128u, surfaceState->getWidth());
    EXPECT_EQ(2048u, surfaceState->getDepth());
    EXPECT_EQ(16384u, surfaceState->getHeight());
}

TEST_F(KernelArgSvmTest, givenNullArgValWhenSettingKernelArgImmediateThenInvalidArgValueErrorIsReturned) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    auto retVal = pKernel->setArgImmediate(0, 256, nullptr);
    EXPECT_EQ(CL_INVALID_ARG_VALUE, retVal);
}

HWTEST_F(KernelArgSvmTest, givenSvmAllocationWhenPassingSvmPointerByValueThenItIsAddedToKernelUnifiedMemoryGfxAllocations) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    auto mockKernelInfo = std::make_unique<MockKernelInfo>();
    mockKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    mockKernelInfo->heapInfo.pSsh = pSshLocal;
    mockKernelInfo->heapInfo.SurfaceStateHeapSize = sizeof(pSshLocal);

    mockKernelInfo->addArgImmediate(0, sizeof(void *) * 2, 0x40, sizeof(void *) * 2);
    mockKernelInfo->setAddressQualifier(0, KernelArgMetadata::AddressSpaceQualifier::AddrPrivate);

    mockKernelInfo->addArgImmediate(1, sizeof(void *), 0x30, sizeof(void *));
    mockKernelInfo->setAddressQualifier(1, KernelArgMetadata::AddressSpaceQualifier::AddrGlobal);

    mockKernelInfo->addArgImmediate(2, sizeof(void *), 0x20, sizeof(void *));
    mockKernelInfo->setAddressQualifier(2, KernelArgMetadata::AddressSpaceQualifier::AddrPrivate);

    mockKernelInfo->addArgImmediate(3, sizeof(void *), 0x10, sizeof(void *));
    mockKernelInfo->setAddressQualifier(3, KernelArgMetadata::AddressSpaceQualifier::AddrPrivate);

    auto mockProgram = std::make_unique<MockProgram>(pContext, false, toClDeviceVector(*pClDevice));
    auto mockKernel = std::make_unique<MockKernel>(mockProgram.get(), *mockKernelInfo, *pClDevice);
    ASSERT_EQ(mockKernel->initialize(), CL_SUCCESS);

    auto memoryManager = static_cast<MockMemoryManager *>(pClDevice->getExecutionEnvironment()->memoryManager.get());
    memoryManager->pageFaultManager.reset(new MockPageFaultManager());
    auto pageFaultManager = static_cast<MockPageFaultManager *>(memoryManager->pageFaultManager.get());
    auto svmManager = pContext->getSVMAllocsManager();
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::SHARED_UNIFIED_MEMORY, rootDeviceIndices, deviceBitfields);
    unifiedMemoryProperties.device = pDevice;
    auto allocation = svmManager->createUnifiedMemoryAllocation(1, unifiedMemoryProperties);
    pageFaultManager->insertAllocation(allocation, 1, pContext->getSVMAllocsManager(), pContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});
    auto allocation2 = svmManager->createUnifiedMemoryAllocation(1, unifiedMemoryProperties);
    pageFaultManager->insertAllocation(allocation2, 1, pContext->getSVMAllocsManager(), pContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});
    auto allocation3 = svmManager->createUnifiedMemoryAllocation(1, unifiedMemoryProperties);
    pageFaultManager->insertAllocation(allocation3, 1, pContext->getSVMAllocsManager(), pContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});
    void *unalignedPtr = reinterpret_cast<void *>(0x1u);
    EXPECT_EQ(mockKernel->setArgImmediate(0, sizeof(void *) * 2, &allocation), CL_SUCCESS);
    EXPECT_EQ(mockKernel->setArgImmediate(1, sizeof(void *), &allocation), CL_SUCCESS);
    EXPECT_EQ(mockKernel->setArgImmediate(2, sizeof(void *), unalignedPtr), CL_SUCCESS);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    commandStreamReceiver.storeMakeResidentAllocations = true;
    auto commandQueue = std::make_unique<MockCommandQueueHw<FamilyType>>(pContext, pClDevice, nullptr);
    const size_t offset = 0;
    const size_t size = 1;

    EXPECT_EQ(commandQueue->enqueueKernel(mockKernel.get(), 1, &offset, &size, &size, 0, nullptr, nullptr), CL_SUCCESS);
    EXPECT_FALSE(commandStreamReceiver.isMadeResident(svmManager->getSVMAlloc(allocation)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)));
    EXPECT_EQ(pageFaultManager->memoryData.at(allocation).domain, PageFaultManager::AllocationDomain::Cpu);
    EXPECT_FALSE(commandStreamReceiver.isMadeResident(svmManager->getSVMAlloc(allocation2)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)));
    EXPECT_EQ(pageFaultManager->memoryData.at(allocation2).domain, PageFaultManager::AllocationDomain::Cpu);
    EXPECT_FALSE(commandStreamReceiver.isMadeResident(svmManager->getSVMAlloc(allocation3)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)));
    EXPECT_EQ(pageFaultManager->memoryData.at(allocation3).domain, PageFaultManager::AllocationDomain::Cpu);

    commandStreamReceiver.makeResidentAllocations.clear();
    pageFaultManager->removeAllocation(allocation);
    pageFaultManager->insertAllocation(allocation, 1, pContext->getSVMAllocsManager(), pContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});
    pageFaultManager->removeAllocation(allocation2);
    pageFaultManager->insertAllocation(allocation2, 1, pContext->getSVMAllocsManager(), pContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});
    pageFaultManager->removeAllocation(allocation3);
    pageFaultManager->insertAllocation(allocation3, 1, pContext->getSVMAllocsManager(), pContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});

    EXPECT_EQ(mockKernel->setArgImmediate(3, sizeof(void *), &allocation2), CL_SUCCESS);
    EXPECT_EQ(commandQueue->enqueueKernel(mockKernel.get(), 1, &offset, &size, &size, 0, nullptr, nullptr), CL_SUCCESS);
    EXPECT_FALSE(commandStreamReceiver.isMadeResident(svmManager->getSVMAlloc(allocation)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)));
    EXPECT_EQ(pageFaultManager->memoryData.at(allocation).domain, PageFaultManager::AllocationDomain::Cpu);
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(svmManager->getSVMAlloc(allocation2)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)));
    EXPECT_EQ(pageFaultManager->memoryData.at(allocation2).domain, PageFaultManager::AllocationDomain::Gpu);
    EXPECT_FALSE(commandStreamReceiver.isMadeResident(svmManager->getSVMAlloc(allocation3)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)));
    EXPECT_EQ(pageFaultManager->memoryData.at(allocation3).domain, PageFaultManager::AllocationDomain::Cpu);

    commandStreamReceiver.makeResidentAllocations.clear();
    pageFaultManager->removeAllocation(allocation);
    pageFaultManager->insertAllocation(allocation, 1, pContext->getSVMAllocsManager(), pContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});
    pageFaultManager->removeAllocation(allocation2);
    pageFaultManager->insertAllocation(allocation2, 1, pContext->getSVMAllocsManager(), pContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});
    pageFaultManager->removeAllocation(allocation3);
    pageFaultManager->insertAllocation(allocation3, 1, pContext->getSVMAllocsManager(), pContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});

    mockKernel->isUnifiedMemorySyncRequired = false;
    EXPECT_EQ(mockKernel->setArgImmediate(3, sizeof(void *), &allocation3), CL_SUCCESS);
    EXPECT_EQ(commandQueue->enqueueKernel(mockKernel.get(), 1, &offset, &size, &size, 0, nullptr, nullptr), CL_SUCCESS);
    EXPECT_FALSE(commandStreamReceiver.isMadeResident(svmManager->getSVMAlloc(allocation)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)));
    EXPECT_EQ(pageFaultManager->memoryData.at(allocation).domain, PageFaultManager::AllocationDomain::Cpu);
    EXPECT_FALSE(commandStreamReceiver.isMadeResident(svmManager->getSVMAlloc(allocation2)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)));
    EXPECT_EQ(pageFaultManager->memoryData.at(allocation2).domain, PageFaultManager::AllocationDomain::Cpu);
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(svmManager->getSVMAlloc(allocation3)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)));
    EXPECT_EQ(pageFaultManager->memoryData.at(allocation3).domain, PageFaultManager::AllocationDomain::Cpu);

    commandStreamReceiver.makeResidentAllocations.clear();
    pageFaultManager->removeAllocation(allocation);
    pageFaultManager->insertAllocation(allocation, 1, pContext->getSVMAllocsManager(), pContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});
    pageFaultManager->removeAllocation(allocation2);
    pageFaultManager->insertAllocation(allocation2, 1, pContext->getSVMAllocsManager(), pContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});
    pageFaultManager->removeAllocation(allocation3);
    pageFaultManager->insertAllocation(allocation3, 1, pContext->getSVMAllocsManager(), pContext->getSpecialQueue(pDevice->getRootDeviceIndex()), {});

    mockKernel->isUnifiedMemorySyncRequired = true;
    EXPECT_EQ(mockKernel->setArgImmediate(3, sizeof(void *), &allocation3), CL_SUCCESS);
    EXPECT_EQ(commandQueue->enqueueKernel(mockKernel.get(), 1, &offset, &size, &size, 0, nullptr, nullptr), CL_SUCCESS);
    EXPECT_FALSE(commandStreamReceiver.isMadeResident(svmManager->getSVMAlloc(allocation)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)));
    EXPECT_EQ(pageFaultManager->memoryData.at(allocation).domain, PageFaultManager::AllocationDomain::Cpu);
    EXPECT_FALSE(commandStreamReceiver.isMadeResident(svmManager->getSVMAlloc(allocation2)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)));
    EXPECT_EQ(pageFaultManager->memoryData.at(allocation2).domain, PageFaultManager::AllocationDomain::Cpu);
    EXPECT_TRUE(commandStreamReceiver.isMadeResident(svmManager->getSVMAlloc(allocation3)->gpuAllocations.getGraphicsAllocation(mockRootDeviceIndex)));
    EXPECT_EQ(pageFaultManager->memoryData.at(allocation3).domain, PageFaultManager::AllocationDomain::Gpu);

    svmManager->freeSVMAlloc(allocation);
    svmManager->freeSVMAlloc(allocation2);
    svmManager->freeSVMAlloc(allocation3);
}

HWTEST_F(KernelArgSvmTest, WhenPatchingWithImplicitSurfaceThenPatchIsApplied) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    constexpr size_t rendSurfSize = sizeof(RENDER_SURFACE_STATE);

    std::vector<char> svmPtr;
    svmPtr.resize(256);

    pKernel->setCrossThreadData(nullptr, sizeof(void *));
    pKernel->setSshLocal(nullptr, rendSurfSize);
    {
        MockGraphicsAllocation svmAlloc(svmPtr.data(), svmPtr.size());

        pKernelInfo->setGlobalVariablesSurface(sizeof(void *), 0, 0);

        constexpr size_t patchOffset = 16;
        void *ptrToPatch = svmPtr.data() + patchOffset;
        ASSERT_GE(pKernel->getCrossThreadDataSize(), sizeof(void *));
        *reinterpret_cast<void **>(pKernel->getCrossThreadData()) = 0U;

        ASSERT_GE(pKernel->getSurfaceStateHeapSize(), rendSurfSize);
        RENDER_SURFACE_STATE *surfState = reinterpret_cast<RENDER_SURFACE_STATE *>(pKernel->getSurfaceStateHeap());
        memset(surfState, 0, rendSurfSize);

        pKernel->patchWithImplicitSurface(ptrToPatch, svmAlloc, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress);

        // verify cross thread data was properly patched
        EXPECT_EQ(ptrToPatch, *reinterpret_cast<void **>(pKernel->getCrossThreadData()));

        // create surface state for comparison
        RENDER_SURFACE_STATE expectedSurfaceState;
        memset(&expectedSurfaceState, 0, rendSurfSize);
        {
            void *addressToPatch = svmAlloc.getUnderlyingBuffer();
            size_t sizeToPatch = svmAlloc.getUnderlyingBufferSize();
            Buffer::setSurfaceState(pDevice, &expectedSurfaceState, false, false,
                                    sizeToPatch, addressToPatch, 0, &svmAlloc, 0, 0, false, false);
        }

        // verify ssh was properly patched
        EXPECT_EQ(0, memcmp(&expectedSurfaceState, surfState, rendSurfSize));

        // when cross thread and ssh data is not available then should not do anything
        pKernel->setCrossThreadData(nullptr, 0);
        pKernel->setSshLocal(nullptr, 0);
        pKernel->patchWithImplicitSurface(ptrToPatch, svmAlloc, pKernelInfo->kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress);
    }
}

TEST_F(KernelArgSvmTest, WhenPatchingBufferOffsetThenPatchIsApplied) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    std::vector<char> svmPtr;
    svmPtr.resize(256);

    pKernel->setCrossThreadData(nullptr, sizeof(uint32_t));
    {
        constexpr uint32_t initVal = 7U;
        constexpr uint32_t svmOffset = 13U;

        MockGraphicsAllocation svmAlloc(svmPtr.data(), 256);
        uint32_t *expectedPatchPtr = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());

        ArgDescPointer arg;
        void *returnedPtr = nullptr;

        arg.bufferOffset = undefined<NEO::CrossThreadDataOffset>;
        *expectedPatchPtr = initVal;
        returnedPtr = pKernel->patchBufferOffset(arg, svmPtr.data(), &svmAlloc);
        EXPECT_EQ(svmPtr.data(), returnedPtr);
        EXPECT_EQ(initVal, *expectedPatchPtr);

        arg.bufferOffset = undefined<NEO::CrossThreadDataOffset>;
        *expectedPatchPtr = initVal;
        returnedPtr = pKernel->patchBufferOffset(arg, svmPtr.data(), nullptr);
        EXPECT_EQ(svmPtr.data(), returnedPtr);
        EXPECT_EQ(initVal, *expectedPatchPtr);

        arg.bufferOffset = 0U;
        *expectedPatchPtr = initVal;
        returnedPtr = pKernel->patchBufferOffset(arg, svmPtr.data(), &svmAlloc);
        EXPECT_EQ(svmPtr.data(), returnedPtr);
        EXPECT_EQ(0U, *expectedPatchPtr);

        arg.bufferOffset = 0U;
        *expectedPatchPtr = initVal;
        returnedPtr = pKernel->patchBufferOffset(arg, svmPtr.data() + svmOffset, nullptr);
        void *expectedPtr = alignDown(svmPtr.data() + svmOffset, 4);
        // expecting to see DWORD alignment restriction in offset
        uint32_t expectedOffset = static_cast<uint32_t>(ptrDiff(svmPtr.data() + svmOffset, expectedPtr));
        EXPECT_EQ(expectedPtr, returnedPtr);
        EXPECT_EQ(expectedOffset, *expectedPatchPtr);

        arg.bufferOffset = 0U;
        *expectedPatchPtr = initVal;
        returnedPtr = pKernel->patchBufferOffset(arg, svmPtr.data() + svmOffset, &svmAlloc);
        EXPECT_EQ(svmPtr.data(), returnedPtr);
        EXPECT_EQ(svmOffset, *expectedPatchPtr);
    }
}

template <typename SetArgHandlerT>
class KernelArgSvmTestTyped : public KernelArgSvmTest {
};

struct SetArgHandlerSetArgSvm {
    static void setArg(Kernel &kernel, uint32_t argNum, void *ptrToPatch, size_t allocSize, GraphicsAllocation &alloc) {
        kernel.setArgSvm(argNum, allocSize, ptrToPatch, &alloc, 0u);
    }

    static constexpr bool supportsOffsets() {
        return true;
    }
};

struct SetArgHandlerSetArgSvmAlloc {
    static void setArg(Kernel &kernel, uint32_t argNum, void *ptrToPatch, size_t allocSize, GraphicsAllocation &alloc) {
        kernel.setArgSvmAlloc(argNum, ptrToPatch, &alloc, 0u);
    }

    static constexpr bool supportsOffsets() {
        return true;
    }
};

struct SetArgHandlerSetArgBuffer {
    static void setArg(Kernel &kernel, uint32_t argNum, void *ptrToPatch, size_t allocSize, GraphicsAllocation &alloc) {
        MockBuffer mb{alloc};
        cl_mem memObj = &mb;
        kernel.setArgBuffer(argNum, sizeof(cl_mem), &memObj);
    }

    static constexpr bool supportsOffsets() {
        return false;
    }
};

using SetArgHandlers = ::testing::Types<SetArgHandlerSetArgSvm, SetArgHandlerSetArgSvmAlloc, SetArgHandlerSetArgBuffer>;

TYPED_TEST_CASE(KernelArgSvmTestTyped, SetArgHandlers);
HWTEST_TYPED_TEST(KernelArgSvmTestTyped, GivenBufferKernelArgWhenBufferOffsetIsNeededThenSetArgSetsIt) {
    const ClDeviceInfo &devInfo = KernelArgSvmFixture::pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    constexpr size_t rendSurfSize = sizeof(RENDER_SURFACE_STATE);

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    uint32_t svmSize = MemoryConstants::pageSize;
    char *svmPtr = reinterpret_cast<char *>(alignedMalloc(svmSize, MemoryConstants::pageSize));

    auto &arg = this->pKernelInfo->argAsPtr(0);
    arg.bindful = 0;
    arg.stateless = 0;
    arg.pointerSize = sizeof(void *);
    arg.bufferOffset = arg.pointerSize;

    this->pKernel->setCrossThreadData(nullptr, arg.bufferOffset + sizeof(uint32_t));
    this->pKernel->setSshLocal(nullptr, rendSurfSize);
    {
        MockGraphicsAllocation svmAlloc(svmPtr, svmSize);

        constexpr size_t patchOffset = 16;
        void *ptrToPatch = svmPtr + patchOffset;
        size_t sizeToPatch = svmSize - patchOffset;
        ASSERT_GE(this->pKernel->getCrossThreadDataSize(), arg.bufferOffset + sizeof(uint32_t));

        void **expectedPointerPatchPtr = reinterpret_cast<void **>(this->pKernel->getCrossThreadData());
        uint32_t *expectedOffsetPatchPtr = reinterpret_cast<uint32_t *>(ptrOffset(this->pKernel->getCrossThreadData(), arg.bufferOffset));
        *expectedPointerPatchPtr = reinterpret_cast<void *>(0U);
        *expectedOffsetPatchPtr = 0U;

        ASSERT_GE(this->pKernel->getSurfaceStateHeapSize(), rendSurfSize);
        RENDER_SURFACE_STATE *surfState = reinterpret_cast<RENDER_SURFACE_STATE *>(this->pKernel->getSurfaceStateHeap());
        memset(surfState, 0, rendSurfSize);

        TypeParam::setArg(*this->pKernel, 0U, ptrToPatch, sizeToPatch, svmAlloc);

        // surface state for comparison
        RENDER_SURFACE_STATE expectedSurfaceState;
        memset(&expectedSurfaceState, 0, rendSurfSize);

        if (TypeParam::supportsOffsets()) {
            // setArgSvm, setArgSvmAlloc
            EXPECT_EQ(ptrToPatch, *expectedPointerPatchPtr);
            EXPECT_EQ(patchOffset, *expectedOffsetPatchPtr);
        } else {
            // setArgBuffer
            EXPECT_EQ(svmAlloc.getUnderlyingBuffer(), *expectedPointerPatchPtr);
            EXPECT_EQ(0U, *expectedOffsetPatchPtr);
        }

        Buffer::setSurfaceState(device.get(), &expectedSurfaceState, false, false, svmAlloc.getUnderlyingBufferSize(),
                                svmAlloc.getUnderlyingBuffer(), 0, &svmAlloc, 0, 0, false, false);

        // verify ssh was properly patched
        int32_t cmpResult = memcmp(&expectedSurfaceState, surfState, rendSurfSize);
        EXPECT_EQ(0, cmpResult);
    }

    alignedFree(svmPtr);
}

TEST_F(KernelArgSvmTest, givenWritableSvmAllocationWhenSettingAsArgThenDoNotExpectAllocationInCacheFlushVector) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    size_t svmSize = 4096;
    void *svmPtr = alignedMalloc(svmSize, MemoryConstants::pageSize);
    MockGraphicsAllocation svmAlloc(svmPtr, svmSize);

    svmAlloc.setMemObjectsAllocationWithWritableFlags(true);
    svmAlloc.setFlushL3Required(false);

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr, &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, pKernel->kernelArgRequiresCacheFlush[0]);

    alignedFree(svmPtr);
}

TEST_F(KernelArgSvmTest, givenCacheFlushSvmAllocationWhenSettingAsArgThenExpectAllocationInCacheFlushVector) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    size_t svmSize = 4096;
    void *svmPtr = alignedMalloc(svmSize, MemoryConstants::pageSize);
    MockGraphicsAllocation svmAlloc(svmPtr, svmSize);

    svmAlloc.setMemObjectsAllocationWithWritableFlags(false);
    svmAlloc.setFlushL3Required(true);

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr, &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(&svmAlloc, pKernel->kernelArgRequiresCacheFlush[0]);

    alignedFree(svmPtr);
}

TEST_F(KernelArgSvmTest, givenNoCacheFlushSvmAllocationWhenSettingAsArgThenNotExpectAllocationInCacheFlushVector) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    size_t svmSize = 4096;
    void *svmPtr = alignedMalloc(svmSize, MemoryConstants::pageSize);
    MockGraphicsAllocation svmAlloc(svmPtr, svmSize);

    svmAlloc.setMemObjectsAllocationWithWritableFlags(false);
    svmAlloc.setFlushL3Required(false);

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr, &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, pKernel->kernelArgRequiresCacheFlush[0]);

    alignedFree(svmPtr);
}

TEST_F(KernelArgSvmTest, givenWritableSvmAllocationWhenSettingKernelExecInfoThenDoNotExpectSvmFlushFlagTrue) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    size_t svmSize = 4096;
    void *svmPtr = alignedMalloc(svmSize, MemoryConstants::pageSize);
    MockGraphicsAllocation svmAlloc(svmPtr, svmSize);

    svmAlloc.setMemObjectsAllocationWithWritableFlags(true);
    svmAlloc.setFlushL3Required(false);

    pKernel->setSvmKernelExecInfo(&svmAlloc);
    EXPECT_FALSE(pKernel->svmAllocationsRequireCacheFlush);

    alignedFree(svmPtr);
}

TEST_F(KernelArgSvmTest, givenCacheFlushSvmAllocationWhenSettingKernelExecInfoThenExpectSvmFlushFlagTrue) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    size_t svmSize = 4096;
    void *svmPtr = alignedMalloc(svmSize, MemoryConstants::pageSize);
    MockGraphicsAllocation svmAlloc(svmPtr, svmSize);

    svmAlloc.setMemObjectsAllocationWithWritableFlags(false);
    svmAlloc.setFlushL3Required(true);

    pKernel->setSvmKernelExecInfo(&svmAlloc);
    EXPECT_TRUE(pKernel->svmAllocationsRequireCacheFlush);

    alignedFree(svmPtr);
}

TEST_F(KernelArgSvmTest, givenNoCacheFlushReadOnlySvmAllocationWhenSettingKernelExecInfoThenExpectSvmFlushFlagFalse) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    size_t svmSize = 4096;
    void *svmPtr = alignedMalloc(svmSize, MemoryConstants::pageSize);
    MockGraphicsAllocation svmAlloc(svmPtr, svmSize);

    svmAlloc.setMemObjectsAllocationWithWritableFlags(false);
    svmAlloc.setFlushL3Required(false);

    pKernel->setSvmKernelExecInfo(&svmAlloc);
    EXPECT_FALSE(pKernel->svmAllocationsRequireCacheFlush);

    alignedFree(svmPtr);
}

TEST_F(KernelArgSvmTest, givenCpuAddressIsNullWhenGpuAddressIsValidThenExpectSvmArgUseGpuAddress) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    char svmPtr[256];

    pKernelInfo->argAsPtr(0).bufferOffset = 0u;

    MockGraphicsAllocation svmAlloc(nullptr, reinterpret_cast<uint64_t>(svmPtr), 256);

    auto retVal = pKernel->setArgSvmAlloc(0, svmPtr, &svmAlloc, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pKernelArg = (void **)(pKernel->getCrossThreadData() +
                                pKernelInfo->argAsPtr(0).stateless);
    EXPECT_EQ(svmPtr, *pKernelArg);
}

TEST_F(KernelArgSvmTest, givenCpuAddressIsNullWhenGpuAddressIsValidThenPatchBufferOffsetWithGpuAddress) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    std::vector<char> svmPtr;
    svmPtr.resize(256);

    pKernel->setCrossThreadData(nullptr, sizeof(uint32_t));

    constexpr uint32_t initVal = 7U;

    MockGraphicsAllocation svmAlloc(nullptr, reinterpret_cast<uint64_t>(svmPtr.data()), 256);
    uint32_t *expectedPatchPtr = reinterpret_cast<uint32_t *>(pKernel->getCrossThreadData());

    ArgDescPointer arg;
    void *returnedPtr = nullptr;

    arg.bufferOffset = 0U;
    *expectedPatchPtr = initVal;
    returnedPtr = pKernel->patchBufferOffset(arg, svmPtr.data(), &svmAlloc);
    EXPECT_EQ(svmPtr.data(), returnedPtr);
    EXPECT_EQ(0U, *expectedPatchPtr);
}

TEST_F(KernelArgSvmTest, GivenZeroCopySvmPtrWhenSettingKernelArgThenKernelUsesSystemMemory) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    void *alloc = pContext->getSVMAllocsManager()->createSVMAlloc(
        4096,
        MemObjHelper::getSvmAllocationProperties(CL_MEM_READ_ONLY),
        pContext->getRootDeviceIndices(),
        pContext->getDeviceBitfields());

    auto svmData = pContext->getSVMAllocsManager()->getSVMAlloc(alloc);
    auto gpuAllocation = svmData->gpuAllocations.getGraphicsAllocation(*pContext->getRootDeviceIndices().begin());
    gpuAllocation->setAllocationType(NEO::AllocationType::SVM_ZERO_COPY);

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());

    auto retVal = pKernel->setArgSvmAlloc(0, alloc, gpuAllocation, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(pKernel->isAnyKernelArgumentUsingSystemMemory());

    pContext->getSVMAllocsManager()->freeSVMAlloc(alloc);
}

TEST_F(KernelArgSvmTest, GivenGpuSvmPtrWhenSettingKernelArgThenKernelNotUsesSystemMemory) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    void *alloc = pContext->getSVMAllocsManager()->createSVMAlloc(
        4096,
        MemObjHelper::getSvmAllocationProperties(CL_MEM_READ_ONLY),
        pContext->getRootDeviceIndices(),
        pContext->getDeviceBitfields());

    auto svmData = pContext->getSVMAllocsManager()->getSVMAlloc(alloc);
    auto gpuAllocation = svmData->gpuAllocations.getGraphicsAllocation(*pContext->getRootDeviceIndices().begin());
    gpuAllocation->setAllocationType(NEO::AllocationType::SVM_GPU);

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());

    auto retVal = pKernel->setArgSvmAlloc(0, alloc, gpuAllocation, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());

    pContext->getSVMAllocsManager()->freeSVMAlloc(alloc);
}

TEST_F(KernelArgSvmTest, GivenGpuSvmPtrAndKernelIsAlreadySetToUseSystemWhenSettingKernelArgThenKernelUsesSystemMemory) {
    const ClDeviceInfo &devInfo = pClDevice->getDeviceInfo();
    if (devInfo.svmCapabilities == 0) {
        GTEST_SKIP();
    }

    void *alloc = pContext->getSVMAllocsManager()->createSVMAlloc(
        4096,
        MemObjHelper::getSvmAllocationProperties(CL_MEM_READ_ONLY),
        pContext->getRootDeviceIndices(),
        pContext->getDeviceBitfields());

    auto svmData = pContext->getSVMAllocsManager()->getSVMAlloc(alloc);
    auto gpuAllocation = svmData->gpuAllocations.getGraphicsAllocation(*pContext->getRootDeviceIndices().begin());
    gpuAllocation->setAllocationType(NEO::AllocationType::SVM_GPU);

    EXPECT_FALSE(pKernel->isAnyKernelArgumentUsingSystemMemory());
    pKernel->anyKernelArgumentUsingSystemMemory = true;

    auto retVal = pKernel->setArgSvmAlloc(0, alloc, gpuAllocation, 0u);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_TRUE(pKernel->isAnyKernelArgumentUsingSystemMemory());

    pContext->getSVMAllocsManager()->freeSVMAlloc(alloc);
}
