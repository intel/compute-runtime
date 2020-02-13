/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/device/device.h"
#include "core/helpers/string.h"
#include "core/kernel/grf_config.h"
#include "runtime/device/cl_device.h"
#include "runtime/kernel/kernel.h"
#include "runtime/platform/platform.h"
#include "runtime/program/block_kernel_manager.h"
#include "runtime/scheduler/scheduler_kernel.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_program.h"

#include <cassert>

namespace NEO {

////////////////////////////////////////////////////////////////////////////////
// Kernel - Core implementation
////////////////////////////////////////////////////////////////////////////////
class MockKernel : public Kernel {
  public:
    using Kernel::addAllocationToCacheFlushVector;
    using Kernel::allBufferArgsStateful;
    using Kernel::auxTranslationRequired;
    using Kernel::containsStatelessWrites;
    using Kernel::executionType;
    using Kernel::isSchedulerKernel;
    using Kernel::kernelArgHandlers;
    using Kernel::kernelArgRequiresCacheFlush;
    using Kernel::kernelArguments;
    using Kernel::kernelSvmGfxAllocations;
    using Kernel::kernelUnifiedMemoryGfxAllocations;
    using Kernel::numberOfBindingTableStates;
    using Kernel::svmAllocationsRequireCacheFlush;
    using Kernel::threadArbitrationPolicy;
    using Kernel::unifiedMemoryControls;

    struct BlockPatchValues {
        uint64_t offset;
        uint32_t size;
        uint64_t address;
    };

    class ReflectionSurfaceHelperPublic : public Kernel::ReflectionSurfaceHelper {
      public:
        static BlockPatchValues devQueue;
        static BlockPatchValues defaultQueue;
        static BlockPatchValues eventPool;
        static BlockPatchValues printfBuffer;
        static const uint64_t undefinedOffset = (uint64_t)-1;

        static void patchBlocksCurbeMock(void *reflectionSurface, uint32_t blockID,
                                         uint64_t defaultDeviceQueueCurbeOffset, uint32_t patchSizeDefaultQueue, uint64_t defaultDeviceQueueGpuAddress,
                                         uint64_t eventPoolCurbeOffset, uint32_t patchSizeEventPool, uint64_t eventPoolGpuAddress,
                                         uint64_t deviceQueueCurbeOffset, uint32_t patchSizeDeviceQueue, uint64_t deviceQueueGpuAddress,
                                         uint64_t printfBufferOffset, uint32_t patchSizePrintfBuffer, uint64_t printfBufferGpuAddress) {
            defaultQueue.address = defaultDeviceQueueGpuAddress;
            defaultQueue.offset = defaultDeviceQueueCurbeOffset;
            defaultQueue.size = patchSizeDefaultQueue;

            devQueue.address = deviceQueueGpuAddress;
            devQueue.offset = deviceQueueCurbeOffset;
            devQueue.size = patchSizeDeviceQueue;

            eventPool.address = eventPoolGpuAddress;
            eventPool.offset = eventPoolCurbeOffset;
            eventPool.size = patchSizeEventPool;

            printfBuffer.address = printfBufferGpuAddress;
            printfBuffer.offset = printfBufferOffset;
            printfBuffer.size = patchSizePrintfBuffer;
        }

        static uint32_t getConstantBufferOffset(void *reflectionSurface, uint32_t blockID) {
            IGIL_KernelDataHeader *pKernelHeader = reinterpret_cast<IGIL_KernelDataHeader *>(reflectionSurface);
            assert(blockID < pKernelHeader->m_numberOfKernels);

            IGIL_KernelAddressData *addressData = pKernelHeader->m_data;
            assert(addressData[blockID].m_ConstantBufferOffset != 0);

            return addressData[blockID].m_ConstantBufferOffset;
        }
    };

    MockKernel(Program *programArg, const KernelInfo &kernelInfoArg, const ClDevice &deviceArg, bool scheduler = false)
        : Kernel(programArg, kernelInfoArg, deviceArg, scheduler) {
    }

    ~MockKernel() {
        // prevent double deletion
        if (Kernel::crossThreadData == mockCrossThreadData.data()) {
            Kernel::crossThreadData = nullptr;
        }

        if (kernelInfoAllocated) {
            delete kernelInfoAllocated->heapInfo.pKernelHeader;
            delete kernelInfoAllocated->patchInfo.executionEnvironment;
            delete kernelInfoAllocated->patchInfo.threadPayload;
            delete kernelInfoAllocated;
        }
    }

    template <typename KernelType = MockKernel>
    static KernelType *create(Device &device, Program *program) {
        return create<KernelType>(device, program, GrfConfig::DefaultGrfNumber);
    }

    template <typename KernelType = MockKernel>
    static KernelType *create(Device &device, Program *program, uint32_t grfNumber) {
        auto info = new KernelInfo();
        const size_t crossThreadSize = 160;
        auto pClDevice = device.getSpecializedDevice<ClDevice>();

        SKernelBinaryHeaderCommon *header = new SKernelBinaryHeaderCommon;
        header->DynamicStateHeapSize = 0;
        header->GeneralStateHeapSize = 0;
        header->KernelHeapSize = 0;
        header->KernelNameSize = 0;
        header->PatchListSize = 0;
        header->SurfaceStateHeapSize = 0;
        info->heapInfo.pKernelHeader = header;

        SPatchThreadPayload *threadPayload = new SPatchThreadPayload;
        threadPayload->LocalIDXPresent = 0;
        threadPayload->LocalIDYPresent = 0;
        threadPayload->LocalIDZPresent = 0;
        threadPayload->HeaderPresent = 0;
        threadPayload->Size = 128;

        info->patchInfo.threadPayload = threadPayload;

        SPatchExecutionEnvironment *executionEnvironment = new SPatchExecutionEnvironment;
        memset(executionEnvironment, 0, sizeof(SPatchExecutionEnvironment));
        executionEnvironment->HasDeviceEnqueue = 0;
        executionEnvironment->NumGRFRequired = grfNumber;
        executionEnvironment->CompiledSIMD32 = 1;
        info->patchInfo.executionEnvironment = executionEnvironment;

        info->crossThreadData = new char[crossThreadSize];

        auto kernel = new KernelType(program, *info, *pClDevice);
        kernel->crossThreadData = new char[crossThreadSize];
        memset(kernel->crossThreadData, 0, crossThreadSize);
        kernel->crossThreadDataSize = crossThreadSize;

        kernel->kernelInfoAllocated = info;

        return kernel;
    }

    uint32_t getPatchedArgumentsNum() const { return patchedArgumentsNum; }

    bool isPatched() const override;

    bool canTransformImages() const override;

    ////////////////////////////////////////////////////////////////////////////////
    void setCrossThreadData(const void *crossThreadDataPattern, uint32_t newCrossThreadDataSize) {
        if ((Kernel::crossThreadData != nullptr) && (Kernel::crossThreadData != mockCrossThreadData.data())) {
            delete[] Kernel::crossThreadData;
            Kernel::crossThreadData = nullptr;
            Kernel::crossThreadDataSize = 0;
        }
        if (crossThreadDataPattern && (newCrossThreadDataSize > 0)) {
            mockCrossThreadData.clear();
            mockCrossThreadData.insert(mockCrossThreadData.begin(), (char *)crossThreadDataPattern, ((char *)crossThreadDataPattern) + newCrossThreadDataSize);
        } else {
            mockCrossThreadData.resize(newCrossThreadDataSize, 0);
        }

        if (newCrossThreadDataSize == 0) {
            return;
        }
        Kernel::crossThreadData = mockCrossThreadData.data();
        Kernel::crossThreadDataSize = static_cast<uint32_t>(mockCrossThreadData.size());
    }

    void setSshLocal(const void *sshPattern, uint32_t newSshSize) {
        sshLocalSize = newSshSize;

        if (newSshSize == 0) {
            pSshLocal.reset(nullptr);
        } else {
            pSshLocal = std::make_unique<char[]>(newSshSize);
            if (sshPattern) {
                memcpy_s(pSshLocal.get(), newSshSize, sshPattern, newSshSize);
            }
        }
    }

    void setPrivateSurface(GraphicsAllocation *gfxAllocation, uint32_t size) {
        privateSurface = gfxAllocation;
        privateSurfaceSize = size;
    }

    GraphicsAllocation *getPrivateSurface() const {
        return privateSurface;
    }

    void setTotalSLMSize(uint32_t size) {
        slmTotalSize = size;
    }

    void setKernelArguments(std::vector<SimpleKernelArgInfo> kernelArguments) {
        this->kernelArguments = kernelArguments;
    }

    template <typename PatchTokenT>
    void patchWithImplicitSurface(void *ptrToPatchInCrossThreadData, GraphicsAllocation &allocation, const PatchTokenT &patch) {
        Kernel::patchWithImplicitSurface(ptrToPatchInCrossThreadData, allocation, patch);
    }

    void *patchBufferOffset(const KernelArgInfo &argInfo, void *svmPtr, GraphicsAllocation *svmAlloc) {
        return Kernel::patchBufferOffset(argInfo, svmPtr, svmAlloc);
    }

    KernelInfo *getAllocatedKernelInfo() {
        return kernelInfoAllocated;
    }

    std::vector<char> mockCrossThreadData;
    std::vector<char> mockSshLocal;

    void setUsingSharedArgs(bool usingSharedArgValue) { this->usingSharedObjArgs = usingSharedArgValue; }

    void makeResident(CommandStreamReceiver &commandStreamReceiver) override;
    void getResidency(std::vector<Surface *> &dst) override;
    void takeOwnership() const override {
        Kernel::takeOwnership();
        takeOwnershipCalls++;
    }

    void releaseOwnership() const override {
        releaseOwnershipCalls++;
        Kernel::releaseOwnership();
    }

    void setSpecialPipelineSelectMode(bool value) { specialPipelineSelectMode = value; }

    bool requiresCacheFlushCommand(const CommandQueue &commandQueue) const override;

    uint32_t makeResidentCalls = 0;
    uint32_t getResidencyCalls = 0;
    mutable uint32_t takeOwnershipCalls = 0;
    mutable uint32_t releaseOwnershipCalls = 0;

    bool canKernelTransformImages = true;
    bool isPatchedOverride = true;

  protected:
    KernelInfo *kernelInfoAllocated = nullptr;
};

//class below have enough internals to service Enqueue operation.
class MockKernelWithInternals {
  public:
    MockKernelWithInternals(const ClDevice &deviceArg, Context *context = nullptr, bool addDefaultArg = false, SPatchExecutionEnvironment newExecutionEnvironment = {}) {
        memset(&kernelHeader, 0, sizeof(SKernelBinaryHeaderCommon));
        memset(&threadPayload, 0, sizeof(SPatchThreadPayload));
        memcpy(&executionEnvironment, &newExecutionEnvironment, sizeof(SPatchExecutionEnvironment));
        memset(&executionEnvironmentBlock, 0, sizeof(SPatchExecutionEnvironment));
        memset(&dataParameterStream, 0, sizeof(SPatchDataParameterStream));
        memset(&mediaVfeState, 0, sizeof(SPatchMediaVFEState));
        memset(&mediaVfeStateSlot1, 0, sizeof(SPatchMediaVFEState));
        executionEnvironment.NumGRFRequired = GrfConfig::DefaultGrfNumber;
        executionEnvironmentBlock.NumGRFRequired = GrfConfig::DefaultGrfNumber;
        executionEnvironment.CompiledSIMD32 = 1;
        kernelHeader.SurfaceStateHeapSize = sizeof(sshLocal);
        threadPayload.LocalIDXPresent = 1;
        threadPayload.LocalIDYPresent = 1;
        threadPayload.LocalIDZPresent = 1;
        kernelInfo.heapInfo.pKernelHeap = kernelIsa;
        kernelInfo.heapInfo.pSsh = sshLocal;
        kernelInfo.heapInfo.pDsh = dshLocal;
        kernelInfo.heapInfo.pKernelHeader = &kernelHeader;
        kernelInfo.patchInfo.dataParameterStream = &dataParameterStream;
        kernelInfo.patchInfo.executionEnvironment = &executionEnvironment;
        kernelInfo.patchInfo.threadPayload = &threadPayload;
        kernelInfo.patchInfo.mediavfestate = &mediaVfeState;
        kernelInfo.patchInfo.mediaVfeStateSlot1 = &mediaVfeStateSlot1;

        if (context == nullptr) {
            mockContext = new MockContext;
            context = mockContext;
        } else {
            context->incRefInternal();
            mockContext = context;
        }

        mockProgram = new MockProgram(*deviceArg.getExecutionEnvironment(), context, false);
        mockKernel = new MockKernel(mockProgram, kernelInfo, deviceArg);
        mockKernel->setCrossThreadData(&crossThreadData, sizeof(crossThreadData));
        mockKernel->setSshLocal(&sshLocal, sizeof(sshLocal));

        if (addDefaultArg) {
            defaultKernelArguments.resize(2);
            defaultKernelArguments[0] = {};
            defaultKernelArguments[1] = {};

            kernelInfo.resizeKernelArgInfoAndRegisterParameter(2);
            kernelInfo.kernelArgInfo.resize(2);
            kernelInfo.kernelArgInfo[0].kernelArgPatchInfoVector.resize(1);
            kernelInfo.kernelArgInfo[0].kernelArgPatchInfoVector[0].crossthreadOffset = 0;
            kernelInfo.kernelArgInfo[0].kernelArgPatchInfoVector[0].size = sizeof(uintptr_t);
            kernelInfo.kernelArgInfo[0].metadata.addressQualifier = NEO::KernelArgMetadata::AddressSpaceQualifier::Global;
            kernelInfo.kernelArgInfo[0].metadata.accessQualifier = NEO::KernelArgMetadata::AccessQualifier::ReadWrite;

            kernelInfo.kernelArgInfo[1].kernelArgPatchInfoVector.resize(1);
            kernelInfo.kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset = 0;
            kernelInfo.kernelArgInfo[1].kernelArgPatchInfoVector[0].size = sizeof(uintptr_t);
            kernelInfo.kernelArgInfo[1].metadata.addressQualifier = NEO::KernelArgMetadata::AddressSpaceQualifier::Global;
            kernelInfo.kernelArgInfo[1].metadata.accessQualifier = NEO::KernelArgMetadata::AccessQualifier::ReadWrite;

            mockKernel->setKernelArguments(defaultKernelArguments);
            mockKernel->kernelArgRequiresCacheFlush.resize(2);
            mockKernel->kernelArgHandlers.resize(2);
            mockKernel->kernelArgHandlers[0] = &Kernel::setArgBuffer;
            mockKernel->kernelArgHandlers[1] = &Kernel::setArgBuffer;

            kernelInfo.kernelArgInfo[1].offsetHeap = 64;
            kernelInfo.kernelArgInfo[0].offsetHeap = 64;
        }
    }

    MockKernelWithInternals(const ClDevice &deviceArg, SPatchExecutionEnvironment newExecutionEnvironment) : MockKernelWithInternals(deviceArg, nullptr, false, newExecutionEnvironment) {
        mockKernel->initialize();
    }

    ~MockKernelWithInternals() {
        mockKernel->decRefInternal();
        mockProgram->decRefInternal();
        mockContext->decRefInternal();
    }

    operator MockKernel *() {
        return mockKernel;
    }

    MockKernel *mockKernel;
    MockProgram *mockProgram;
    Context *mockContext;
    KernelInfo kernelInfo;
    SKernelBinaryHeaderCommon kernelHeader = {};
    SPatchThreadPayload threadPayload = {};
    SPatchMediaVFEState mediaVfeState = {};
    SPatchMediaVFEState mediaVfeStateSlot1 = {};
    SPatchDataParameterStream dataParameterStream = {};
    SPatchExecutionEnvironment executionEnvironment = {};
    SPatchExecutionEnvironment executionEnvironmentBlock = {};
    uint32_t kernelIsa[32];
    char crossThreadData[256];
    char sshLocal[128];
    char dshLocal[128];
    std::vector<Kernel::SimpleKernelArgInfo> defaultKernelArguments;
};

class MockParentKernel : public Kernel {
  public:
    using Kernel::auxTranslationRequired;
    using Kernel::patchBlocksCurbeWithConstantValues;
    static MockParentKernel *create(Context &context, bool addChildSimdSize = false, bool addChildGlobalMemory = false, bool addChildConstantMemory = false, bool addPrintfForParent = true, bool addPrintfForBlock = true) {
        ClDevice &device = *context.getDevice(0);

        auto info = new KernelInfo();
        const size_t crossThreadSize = 160;
        uint32_t crossThreadOffset = 0;
        uint32_t crossThreadOffsetBlock = 0;

        SKernelBinaryHeaderCommon *header = new SKernelBinaryHeaderCommon;
        header->DynamicStateHeapSize = 0;
        header->GeneralStateHeapSize = 0;
        header->KernelHeapSize = 0;
        header->KernelNameSize = 0;
        header->PatchListSize = 0;
        header->SurfaceStateHeapSize = 0;
        info->heapInfo.pKernelHeader = header;

        SPatchThreadPayload *threadPayload = new SPatchThreadPayload;
        threadPayload->LocalIDXPresent = 0;
        threadPayload->LocalIDYPresent = 0;
        threadPayload->LocalIDZPresent = 0;
        threadPayload->HeaderPresent = 0;
        threadPayload->Size = 128;

        info->patchInfo.threadPayload = threadPayload;

        SPatchExecutionEnvironment *executionEnvironment = new SPatchExecutionEnvironment;
        *executionEnvironment = {};
        executionEnvironment->HasDeviceEnqueue = 1;
        executionEnvironment->NumGRFRequired = GrfConfig::DefaultGrfNumber;
        executionEnvironment->CompiledSIMD32 = 1;
        info->patchInfo.executionEnvironment = executionEnvironment;

        SPatchAllocateStatelessDefaultDeviceQueueSurface *allocateDeviceQueue = new SPatchAllocateStatelessDefaultDeviceQueueSurface;
        allocateDeviceQueue->DataParamOffset = crossThreadOffset;
        allocateDeviceQueue->DataParamSize = 8;
        allocateDeviceQueue->SurfaceStateHeapOffset = 0;
        allocateDeviceQueue->Size = 8;
        info->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface = allocateDeviceQueue;

        crossThreadOffset += 8;

        SPatchAllocateStatelessEventPoolSurface *eventPool = new SPatchAllocateStatelessEventPoolSurface;
        eventPool->DataParamOffset = crossThreadOffset;
        eventPool->DataParamSize = 8;
        eventPool->EventPoolSurfaceIndex = 0;
        eventPool->Size = 8;
        info->patchInfo.pAllocateStatelessEventPoolSurface = eventPool;

        crossThreadOffset += 8;
        if (addPrintfForParent) {
            SPatchAllocateStatelessPrintfSurface *printfBuffer = new SPatchAllocateStatelessPrintfSurface;
            printfBuffer->DataParamOffset = crossThreadOffset;
            printfBuffer->DataParamSize = 8;
            printfBuffer->PrintfSurfaceIndex = 0;
            printfBuffer->Size = 8;
            printfBuffer->SurfaceStateHeapOffset = 0;
            printfBuffer->Token = 0;
            info->patchInfo.pAllocateStatelessPrintfSurface = printfBuffer;

            crossThreadOffset += 8;
        }

        MockProgram *mockProgram = new MockProgram(*device.getExecutionEnvironment());
        mockProgram->setContext(&context);
        mockProgram->setDevice(&device);

        if (addChildSimdSize) {
            info->childrenKernelsIdOffset.push_back({0, crossThreadOffset});
        }

        crossThreadOffset += 8;

        assert(crossThreadSize >= crossThreadOffset);
        info->crossThreadData = new char[crossThreadSize];

        auto parent = new MockParentKernel(mockProgram, *info, device);
        parent->crossThreadData = new char[crossThreadSize];
        memset(parent->crossThreadData, 0, crossThreadSize);
        parent->crossThreadDataSize = crossThreadSize;
        parent->mockKernelInfo = info;

        auto infoBlock = new KernelInfo();
        SPatchAllocateStatelessDefaultDeviceQueueSurface *allocateDeviceQueueBlock = new SPatchAllocateStatelessDefaultDeviceQueueSurface;
        allocateDeviceQueueBlock->DataParamOffset = crossThreadOffsetBlock;
        allocateDeviceQueueBlock->DataParamSize = 8;
        allocateDeviceQueueBlock->SurfaceStateHeapOffset = 0;
        allocateDeviceQueueBlock->Size = 8;
        infoBlock->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface = allocateDeviceQueueBlock;

        crossThreadOffsetBlock += 8;

        SPatchAllocateStatelessEventPoolSurface *eventPoolBlock = new SPatchAllocateStatelessEventPoolSurface;
        eventPoolBlock->DataParamOffset = crossThreadOffsetBlock;
        eventPoolBlock->DataParamSize = 8;
        eventPoolBlock->EventPoolSurfaceIndex = 0;
        eventPoolBlock->Size = 8;
        infoBlock->patchInfo.pAllocateStatelessEventPoolSurface = eventPoolBlock;

        crossThreadOffsetBlock += 8;
        if (addPrintfForBlock) {
            SPatchAllocateStatelessPrintfSurface *printfBufferBlock = new SPatchAllocateStatelessPrintfSurface;
            printfBufferBlock->DataParamOffset = crossThreadOffsetBlock;
            printfBufferBlock->DataParamSize = 8;
            printfBufferBlock->PrintfSurfaceIndex = 0;
            printfBufferBlock->Size = 8;
            printfBufferBlock->SurfaceStateHeapOffset = 0;
            printfBufferBlock->Token = 0;
            infoBlock->patchInfo.pAllocateStatelessPrintfSurface = printfBufferBlock;

            crossThreadOffsetBlock += 8;
        }

        infoBlock->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization = nullptr;
        infoBlock->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization = nullptr;

        if (addChildGlobalMemory) {
            SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization *globalMemoryBlock = new SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization;
            globalMemoryBlock->DataParamOffset = crossThreadOffsetBlock;
            globalMemoryBlock->DataParamSize = 8;
            globalMemoryBlock->Size = 8;
            globalMemoryBlock->SurfaceStateHeapOffset = 0;
            globalMemoryBlock->Token = 0;
            infoBlock->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization = globalMemoryBlock;
            crossThreadOffsetBlock += 8;
        }

        if (addChildConstantMemory) {
            SPatchAllocateStatelessConstantMemorySurfaceWithInitialization *constantMemoryBlock = new SPatchAllocateStatelessConstantMemorySurfaceWithInitialization;
            constantMemoryBlock->DataParamOffset = crossThreadOffsetBlock;
            constantMemoryBlock->DataParamSize = 8;
            constantMemoryBlock->Size = 8;
            constantMemoryBlock->SurfaceStateHeapOffset = 0;
            constantMemoryBlock->Token = 0;
            infoBlock->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization = constantMemoryBlock;
            crossThreadOffsetBlock += 8;
        }

        SKernelBinaryHeaderCommon *headerBlock = new SKernelBinaryHeaderCommon;
        headerBlock->DynamicStateHeapSize = 0;
        headerBlock->GeneralStateHeapSize = 0;
        headerBlock->KernelHeapSize = 0;
        headerBlock->KernelNameSize = 0;
        headerBlock->PatchListSize = 0;
        headerBlock->SurfaceStateHeapSize = 0;
        infoBlock->heapInfo.pKernelHeader = headerBlock;

        SPatchThreadPayload *threadPayloadBlock = new SPatchThreadPayload;
        threadPayloadBlock->LocalIDXPresent = 0;
        threadPayloadBlock->LocalIDYPresent = 0;
        threadPayloadBlock->LocalIDZPresent = 0;
        threadPayloadBlock->HeaderPresent = 0;
        threadPayloadBlock->Size = 128;

        infoBlock->patchInfo.threadPayload = threadPayloadBlock;

        SPatchExecutionEnvironment *executionEnvironmentBlock = new SPatchExecutionEnvironment;
        executionEnvironmentBlock->HasDeviceEnqueue = 1;
        executionEnvironmentBlock->NumGRFRequired = GrfConfig::DefaultGrfNumber;
        executionEnvironmentBlock->CompiledSIMD32 = 1;
        infoBlock->patchInfo.executionEnvironment = executionEnvironmentBlock;

        SPatchDataParameterStream *streamBlock = new SPatchDataParameterStream;
        streamBlock->DataParameterStreamSize = 0;
        streamBlock->Size = 0;
        infoBlock->patchInfo.dataParameterStream = streamBlock;

        SPatchBindingTableState *bindingTable = new SPatchBindingTableState;
        bindingTable->Count = 0;
        bindingTable->Offset = 0;
        bindingTable->Size = 0;
        bindingTable->SurfaceStateOffset = 0;
        infoBlock->patchInfo.bindingTableState = bindingTable;

        SPatchInterfaceDescriptorData *idData = new SPatchInterfaceDescriptorData;
        idData->BindingTableOffset = 0;
        idData->KernelOffset = 0;
        idData->Offset = 0;
        idData->SamplerStateOffset = 0;
        idData->Size = 0;
        infoBlock->patchInfo.interfaceDescriptorData = idData;

        infoBlock->heapInfo.pDsh = (void *)new uint64_t[64];
        infoBlock->crossThreadData = new char[crossThreadOffsetBlock > crossThreadSize ? crossThreadOffsetBlock : crossThreadSize];

        mockProgram->blockKernelManager->addBlockKernelInfo(infoBlock);
        parent->mockProgram = mockProgram;

        return parent;
    }

    MockParentKernel(Program *programArg, const KernelInfo &kernelInfoArg, const ClDevice &deviceArg) : Kernel(programArg, kernelInfoArg, deviceArg) {
    }

    ~MockParentKernel() {
        delete kernelInfo.patchInfo.executionEnvironment;
        delete kernelInfo.patchInfo.pAllocateStatelessDefaultDeviceQueueSurface;
        delete kernelInfo.patchInfo.pAllocateStatelessEventPoolSurface;
        delete kernelInfo.patchInfo.pAllocateStatelessPrintfSurface;
        delete kernelInfo.patchInfo.threadPayload;
        delete kernelInfo.heapInfo.pKernelHeader;
        delete &kernelInfo;
        BlockKernelManager *blockManager = program->getBlockKernelManager();

        for (uint32_t i = 0; i < blockManager->getCount(); i++) {
            const KernelInfo *blockInfo = blockManager->getBlockKernelInfo(i);
            delete blockInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface;
            delete blockInfo->patchInfo.pAllocateStatelessEventPoolSurface;
            delete blockInfo->patchInfo.pAllocateStatelessPrintfSurface;
            delete blockInfo->heapInfo.pKernelHeader;
            delete blockInfo->patchInfo.threadPayload;
            delete blockInfo->patchInfo.executionEnvironment;
            delete blockInfo->patchInfo.dataParameterStream;
            delete blockInfo->patchInfo.bindingTableState;
            delete blockInfo->patchInfo.interfaceDescriptorData;
            delete blockInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization;
            delete blockInfo->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization;
            delete[](uint64_t *) blockInfo->heapInfo.pDsh;
        }
        if (mockProgram) {
            mockProgram->decRefInternal();
        }
    }

    Context *getContext() {
        return &mockProgram->getContext();
    }

    void setReflectionSurface(GraphicsAllocation *reflectionSurface) {
        kernelReflectionSurface = reflectionSurface;
    }

    MockProgram *mockProgram;
    KernelInfo *mockKernelInfo = nullptr;
};

class MockSchedulerKernel : public SchedulerKernel {
  public:
    MockSchedulerKernel(Program *programArg, const KernelInfo &kernelInfoArg, const ClDevice &deviceArg) : SchedulerKernel(programArg, kernelInfoArg, deviceArg){};
};

class MockDebugKernel : public MockKernel {
  public:
    MockDebugKernel(Program *program, KernelInfo &kernelInfo, const ClDevice &device) : MockKernel(program, kernelInfo, device) {
        if (!kernelInfo.patchInfo.pAllocateSystemThreadSurface) {
            SPatchAllocateSystemThreadSurface *patchToken = new SPatchAllocateSystemThreadSurface;

            patchToken->BTI = 0;
            patchToken->Offset = 0;
            patchToken->PerThreadSystemThreadSurfaceSize = MockDebugKernel::perThreadSystemThreadSurfaceSize;
            patchToken->Size = sizeof(SPatchAllocateSystemThreadSurface);
            patchToken->Token = iOpenCL::PATCH_TOKEN_ALLOCATE_SIP_SURFACE;

            kernelInfo.patchInfo.pAllocateSystemThreadSurface = patchToken;

            systemThreadSurfaceAllocated = true;
        }
    }

    ~MockDebugKernel() override {
        if (systemThreadSurfaceAllocated) {
            delete kernelInfo.patchInfo.pAllocateSystemThreadSurface;
        }
    }
    static const uint32_t perThreadSystemThreadSurfaceSize;
    bool systemThreadSurfaceAllocated = false;
};

} // namespace NEO
