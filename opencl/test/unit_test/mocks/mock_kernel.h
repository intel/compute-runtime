/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device/device.h"
#include "shared/source/helpers/string.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/kernel/kernel_objects_for_aux_translation.h"
#include "opencl/source/kernel/multi_device_kernel.h"
#include "opencl/source/platform/platform.h"
#include "opencl/source/program/block_kernel_manager.h"
#include "opencl/source/scheduler/scheduler_kernel.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include <cassert>

namespace NEO {
void populateKernelArgDescriptor(KernelDescriptor &dst, size_t argNum, const SPatchDataParameterBuffer &token);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateStatelessPrintfSurface &token);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchExecutionEnvironment &execEnv);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateStatelessEventPoolSurface &token);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateStatelessDefaultDeviceQueueSurface &token);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchString &token);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateSystemThreadSurface &token);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateStatelessConstantMemorySurfaceWithInitialization &token);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization &token);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateLocalSurface &token);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchInterfaceDescriptorData &token);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchMediaVFEState &token, uint32_t slot);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchSamplerStateArray &token);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchBindingTableState &token);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchThreadPayload &token);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchDataParameterStream &token);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateStatelessPrivateSurface &token);
void populateKernelDescriptor(KernelDescriptor &dst, const SPatchAllocateSyncBuffer &token);

struct MockKernelObjForAuxTranslation : public KernelObjForAuxTranslation {
    MockKernelObjForAuxTranslation(Type type) : KernelObjForAuxTranslation(type, nullptr) {
        if (type == KernelObjForAuxTranslation::Type::MEM_OBJ) {
            mockBuffer.reset(new MockBuffer);
            mockBuffer->getGraphicsAllocation(0)->setAllocationType(GraphicsAllocation::AllocationType::BUFFER_COMPRESSED);
            this->object = mockBuffer.get();
        } else {
            DEBUG_BREAK_IF(type != KernelObjForAuxTranslation::Type::GFX_ALLOC);
            mockGraphicsAllocation.reset(new MockGraphicsAllocation(nullptr, 0x100));
            this->object = mockGraphicsAllocation.get();
        }
    };

    MockKernelObjForAuxTranslation(Type type, size_t size) : MockKernelObjForAuxTranslation(type) {
        if (type == KernelObjForAuxTranslation::Type::MEM_OBJ) {
            mockBuffer->getGraphicsAllocation(0)->setSize(size);
        } else {
            DEBUG_BREAK_IF(type != KernelObjForAuxTranslation::Type::GFX_ALLOC);
            mockGraphicsAllocation->setSize(size);
        }
    }

    std::unique_ptr<MockBuffer> mockBuffer = nullptr;
    std::unique_ptr<MockGraphicsAllocation> mockGraphicsAllocation = nullptr;
};

class MockMultiDeviceKernel : public MultiDeviceKernel {
  public:
    static KernelVectorType toKernelVector(Kernel *pKernel) {
        KernelVectorType kernelVector;
        kernelVector.resize(pKernel->getProgram()->getMaxRootDeviceIndex() + 1);
        kernelVector[pKernel->getProgram()->getDevices()[0]->getRootDeviceIndex()] = pKernel;
        return kernelVector;
    }
    using MultiDeviceKernel::MultiDeviceKernel;
    template <typename kernel_t = Kernel>
    static MockMultiDeviceKernel *create(Program *programArg, const KernelInfoContainer &kernelInfoArg) {
        KernelVectorType kernelVector;
        kernelVector.resize(programArg->getMaxRootDeviceIndex() + 1);
        for (auto &pDevice : programArg->getDevices()) {
            auto rootDeviceIndex = pDevice->getRootDeviceIndex();
            if (kernelVector[rootDeviceIndex]) {
                continue;
            }
            kernelVector[rootDeviceIndex] = new kernel_t(programArg, *kernelInfoArg[rootDeviceIndex], *pDevice);
        }
        return new MockMultiDeviceKernel(std::move(kernelVector), kernelInfoArg);
    }
    void takeOwnership() const override {
        MultiDeviceKernel::takeOwnership();
        takeOwnershipCalls++;
    }

    void releaseOwnership() const override {
        releaseOwnershipCalls++;
        MultiDeviceKernel::releaseOwnership();
    }

    mutable uint32_t takeOwnershipCalls = 0;
    mutable uint32_t releaseOwnershipCalls = 0;
};

////////////////////////////////////////////////////////////////////////////////
// Kernel - Core implementation
////////////////////////////////////////////////////////////////////////////////
class MockKernel : public Kernel {
  public:
    using Kernel::addAllocationToCacheFlushVector;
    using Kernel::allBufferArgsStateful;
    using Kernel::auxTranslationRequired;
    using Kernel::containsStatelessWrites;
    using Kernel::dataParameterSimdSize;
    using Kernel::enqueuedLocalWorkSizeX;
    using Kernel::enqueuedLocalWorkSizeY;
    using Kernel::enqueuedLocalWorkSizeZ;
    using Kernel::executionType;
    using Kernel::getDevice;
    using Kernel::globalWorkOffsetX;
    using Kernel::globalWorkOffsetY;
    using Kernel::globalWorkOffsetZ;
    using Kernel::globalWorkSizeX;
    using Kernel::globalWorkSizeY;
    using Kernel::globalWorkSizeZ;
    using Kernel::hasDirectStatelessAccessToHostMemory;
    using Kernel::hasIndirectStatelessAccessToHostMemory;
    using Kernel::isSchedulerKernel;
    using Kernel::kernelArgHandlers;
    using Kernel::kernelArgRequiresCacheFlush;
    using Kernel::kernelArguments;
    using Kernel::KernelConfig;
    using Kernel::kernelHasIndirectAccess;
    using Kernel::kernelSubmissionMap;
    using Kernel::kernelSvmGfxAllocations;
    using Kernel::kernelUnifiedMemoryGfxAllocations;
    using Kernel::localWorkSizeX;
    using Kernel::localWorkSizeX2;
    using Kernel::localWorkSizeY;
    using Kernel::localWorkSizeY2;
    using Kernel::localWorkSizeZ;
    using Kernel::localWorkSizeZ2;
    using Kernel::maxKernelWorkGroupSize;
    using Kernel::maxWorkGroupSizeForCrossThreadData;
    using Kernel::numberOfBindingTableStates;
    using Kernel::numWorkGroupsX;
    using Kernel::numWorkGroupsY;
    using Kernel::numWorkGroupsZ;
    using Kernel::parentEventOffset;
    using Kernel::patchBufferOffset;
    using Kernel::patchWithImplicitSurface;
    using Kernel::preferredWkgMultipleOffset;
    using Kernel::privateSurface;
    using Kernel::singleSubdevicePreferedInCurrentEnqueue;
    using Kernel::svmAllocationsRequireCacheFlush;
    using Kernel::threadArbitrationPolicy;
    using Kernel::unifiedMemoryControls;
    using Kernel::workDim;

    using Kernel::slmSizes;
    using Kernel::slmTotalSize;

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

    MockKernel(Program *programArg, const KernelInfo &kernelInfoArg, ClDevice &clDeviceArg, bool scheduler = false)
        : Kernel(programArg, kernelInfoArg, clDeviceArg, scheduler) {
    }

    ~MockKernel() override {
        // prevent double deletion
        if (crossThreadData == mockCrossThreadData.data()) {
            crossThreadData = nullptr;
        }

        if (kernelInfoAllocated) {
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

        SPatchThreadPayload threadPayload = {};
        threadPayload.LocalIDXPresent = 0;
        threadPayload.LocalIDYPresent = 0;
        threadPayload.LocalIDZPresent = 0;
        threadPayload.HeaderPresent = 0;
        threadPayload.Size = 128;
        populateKernelDescriptor(info->kernelDescriptor, threadPayload);

        info->kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue = false;
        info->kernelDescriptor.kernelAttributes.numGrfRequired = grfNumber;
        info->kernelDescriptor.kernelAttributes.simdSize = 32;

        info->crossThreadData = new char[crossThreadSize];

        auto kernel = new KernelType(program, *info, *device.getSpecializedDevice<ClDevice>());
        kernel->crossThreadData = new char[crossThreadSize];
        memset(kernel->crossThreadData, 0, crossThreadSize);
        kernel->crossThreadDataSize = crossThreadSize;

        kernel->kernelInfoAllocated = info;

        return kernel;
    }

    static const KernelInfoContainer toKernelInfoContainer(const KernelInfo &kernelInfo, uint32_t rootDeviceIndex);

    uint32_t getPatchedArgumentsNum() const { return patchedArgumentsNum; }

    bool isPatched() const override;

    bool canTransformImages() const override;

    ////////////////////////////////////////////////////////////////////////////////
    void setCrossThreadData(const void *crossThreadDataPattern, uint32_t newCrossThreadDataSize) {
        if ((crossThreadData != nullptr) && (crossThreadData != mockCrossThreadData.data())) {
            delete[] crossThreadData;
            crossThreadData = nullptr;
            crossThreadDataSize = 0;
        }
        if (crossThreadDataPattern && (newCrossThreadDataSize > 0)) {
            mockCrossThreadData.clear();
            mockCrossThreadData.insert(mockCrossThreadData.begin(), (char *)crossThreadDataPattern, ((char *)crossThreadDataPattern) + newCrossThreadDataSize);
        } else {
            mockCrossThreadData.resize(newCrossThreadDataSize, 0);
        }

        if (newCrossThreadDataSize == 0) {
            crossThreadData = nullptr;
            crossThreadDataSize = 0;
            return;
        }
        crossThreadData = mockCrossThreadData.data();
        crossThreadDataSize = static_cast<uint32_t>(mockCrossThreadData.size());
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

    void setTotalSLMSize(uint32_t size) {
        slmTotalSize = size;
    }

    void setKernelArguments(std::vector<SimpleKernelArgInfo> kernelArguments) {
        this->kernelArguments = kernelArguments;
    }

    KernelInfo *getAllocatedKernelInfo() {
        return kernelInfoAllocated;
    }

    std::vector<char> mockCrossThreadData;
    std::vector<char> mockSshLocal;

    void setUsingSharedArgs(bool usingSharedArgValue) { this->usingSharedObjArgs = usingSharedArgValue; }

    void makeResident(CommandStreamReceiver &commandStreamReceiver) override;
    void getResidency(std::vector<Surface *> &dst) override;

    void setSpecialPipelineSelectMode(bool value) { specialPipelineSelectMode = value; }

    bool requiresCacheFlushCommand(const CommandQueue &commandQueue) const override;

    uint32_t makeResidentCalls = 0;
    uint32_t getResidencyCalls = 0;

    bool canKernelTransformImages = true;
    bool isPatchedOverride = true;

  protected:
    KernelInfo *kernelInfoAllocated = nullptr;
};

//class below have enough internals to service Enqueue operation.
class MockKernelWithInternals {
  public:
    MockKernelWithInternals(const ClDeviceVector &deviceVector, Context *context = nullptr, bool addDefaultArg = false, SPatchExecutionEnvironment execEnv = {}) {
        memset(&kernelHeader, 0, sizeof(SKernelBinaryHeaderCommon));
        memset(&dataParameterStream, 0, sizeof(SPatchDataParameterStream));
        memset(&mediaVfeState, 0, sizeof(SPatchMediaVFEState));
        memset(&mediaVfeStateSlot1, 0, sizeof(SPatchMediaVFEState));
        memset(&threadPayload, 0, sizeof(SPatchThreadPayload));
        threadPayload.LocalIDXPresent = 1;
        threadPayload.LocalIDYPresent = 1;
        threadPayload.LocalIDZPresent = 1;

        kernelInfo.heapInfo.pKernelHeap = kernelIsa;
        kernelInfo.heapInfo.pSsh = sshLocal;
        kernelInfo.heapInfo.pDsh = dshLocal;
        kernelInfo.heapInfo.SurfaceStateHeapSize = sizeof(sshLocal);
        populateKernelDescriptor(kernelInfo.kernelDescriptor, dataParameterStream);

        populateKernelDescriptor(kernelInfo.kernelDescriptor, execEnv);
        kernelInfo.kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::DefaultGrfNumber;
        kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 32;

        populateKernelDescriptor(kernelInfo.kernelDescriptor, threadPayload);
        populateKernelDescriptor(kernelInfo.kernelDescriptor, mediaVfeState, 0);
        populateKernelDescriptor(kernelInfo.kernelDescriptor, mediaVfeStateSlot1, 1);

        if (context == nullptr) {
            mockContext = new MockContext(deviceVector);
            context = mockContext;
        } else {
            context->incRefInternal();
            mockContext = context;
        }
        auto maxRootDeviceIndex = 0u;

        for (const auto &pClDevice : deviceVector) {
            if (pClDevice->getRootDeviceIndex() > maxRootDeviceIndex) {
                maxRootDeviceIndex = pClDevice->getRootDeviceIndex();
            }
        }

        kernelInfos.resize(maxRootDeviceIndex + 1);

        for (const auto &pClDevice : deviceVector) {
            kernelInfos[pClDevice->getRootDeviceIndex()] = &kernelInfo;
        }

        mockProgram = new MockProgram(context, false, deviceVector);
        mockKernel = new MockKernel(mockProgram, kernelInfo, *deviceVector[0]);
        mockKernel->setCrossThreadData(&crossThreadData, sizeof(crossThreadData));
        KernelVectorType mockKernels;
        mockKernels.resize(mockProgram->getMaxRootDeviceIndex() + 1);
        for (const auto &pClDevice : deviceVector) {
            auto rootDeviceIndex = pClDevice->getRootDeviceIndex();
            if (mockKernels[rootDeviceIndex] == nullptr) {
                mockKernels[rootDeviceIndex] = mockKernel;
            }
        }
        mockMultiDeviceKernel = new MockMultiDeviceKernel(std::move(mockKernels), kernelInfos);

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
            kernelInfo.kernelArgInfo[0].metadata.addressQualifier = NEO::KernelArgMetadata::AddrGlobal;
            kernelInfo.kernelArgInfo[0].metadata.accessQualifier = NEO::KernelArgMetadata::AccessReadWrite;

            kernelInfo.kernelArgInfo[1].kernelArgPatchInfoVector.resize(1);
            kernelInfo.kernelArgInfo[1].kernelArgPatchInfoVector[0].crossthreadOffset = 0;
            kernelInfo.kernelArgInfo[1].kernelArgPatchInfoVector[0].size = sizeof(uintptr_t);
            kernelInfo.kernelArgInfo[1].metadata.addressQualifier = NEO::KernelArgMetadata::AddrGlobal;
            kernelInfo.kernelArgInfo[1].metadata.accessQualifier = NEO::KernelArgMetadata::AccessReadWrite;

            mockKernel->setKernelArguments(defaultKernelArguments);
            mockKernel->kernelArgRequiresCacheFlush.resize(2);
            mockKernel->kernelArgHandlers.resize(2);
            mockKernel->kernelArgHandlers[0] = &Kernel::setArgBuffer;
            mockKernel->kernelArgHandlers[1] = &Kernel::setArgBuffer;

            kernelInfo.kernelArgInfo[1].offsetHeap = 64;
            kernelInfo.kernelArgInfo[0].offsetHeap = 64;
        }
    }

    MockKernelWithInternals(ClDevice &deviceArg, Context *context = nullptr, bool addDefaultArg = false, SPatchExecutionEnvironment execEnv = {}) : MockKernelWithInternals(toClDeviceVector(deviceArg), context, addDefaultArg, execEnv) {
    }
    MockKernelWithInternals(ClDevice &deviceArg, SPatchExecutionEnvironment execEnv) : MockKernelWithInternals(deviceArg, nullptr, false, execEnv) {
        mockKernel->initialize();
    }

    ~MockKernelWithInternals() {
        mockMultiDeviceKernel->decRefInternal();
        mockProgram->decRefInternal();
        mockContext->decRefInternal();
    }

    operator MockKernel *() {
        return mockKernel;
    }

    MockMultiDeviceKernel *mockMultiDeviceKernel = nullptr;
    MockKernel *mockKernel;
    MockProgram *mockProgram;
    Context *mockContext;
    KernelInfoContainer kernelInfos;
    KernelInfo kernelInfo;
    SKernelBinaryHeaderCommon kernelHeader = {};
    SPatchThreadPayload threadPayload = {};
    SPatchMediaVFEState mediaVfeState = {};
    SPatchMediaVFEState mediaVfeStateSlot1 = {};
    SPatchDataParameterStream dataParameterStream = {};
    uint32_t kernelIsa[32];
    char crossThreadData[256];
    char sshLocal[128];
    char dshLocal[128];
    std::vector<Kernel::SimpleKernelArgInfo> defaultKernelArguments;
};

class MockParentKernel : public Kernel {
  public:
    using Kernel::auxTranslationRequired;
    using Kernel::kernelInfo;
    using Kernel::patchBlocksCurbeWithConstantValues;
    using Kernel::pSshLocal;
    using Kernel::sshLocalSize;

    static MockParentKernel *create(Context &context, bool addChildSimdSize = false, bool addChildGlobalMemory = false, bool addChildConstantMemory = false, bool addPrintfForParent = true, bool addPrintfForBlock = true) {
        auto clDevice = context.getDevice(0);

        auto info = new KernelInfo();
        const size_t crossThreadSize = 160;
        uint32_t crossThreadOffset = 0;
        uint32_t crossThreadOffsetBlock = 0;

        SPatchThreadPayload threadPayload = {};
        threadPayload.LocalIDXPresent = 0;
        threadPayload.LocalIDYPresent = 0;
        threadPayload.LocalIDZPresent = 0;
        threadPayload.HeaderPresent = 0;
        threadPayload.Size = 128;
        populateKernelDescriptor(info->kernelDescriptor, threadPayload);

        info->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Stateless;
        info->kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue = true;
        info->kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::DefaultGrfNumber;
        info->kernelDescriptor.kernelAttributes.simdSize = 32;

        SPatchAllocateStatelessDefaultDeviceQueueSurface allocateDeviceQueueSurface = {};
        allocateDeviceQueueSurface.DataParamOffset = crossThreadOffset;
        allocateDeviceQueueSurface.DataParamSize = 8;
        allocateDeviceQueueSurface.Size = 8;
        populateKernelDescriptor(info->kernelDescriptor, allocateDeviceQueueSurface);

        crossThreadOffset += 8;

        SPatchAllocateStatelessEventPoolSurface allocateEventPoolSurface = {};
        allocateEventPoolSurface.DataParamOffset = crossThreadOffset;
        allocateEventPoolSurface.DataParamSize = 8;
        allocateEventPoolSurface.EventPoolSurfaceIndex = 0;
        allocateEventPoolSurface.Size = 8;
        populateKernelDescriptor(info->kernelDescriptor, allocateEventPoolSurface);

        crossThreadOffset += 8;
        if (addPrintfForParent) {
            SPatchAllocateStatelessPrintfSurface printfBuffer = {};
            printfBuffer.DataParamOffset = crossThreadOffset;
            printfBuffer.DataParamSize = 8;
            printfBuffer.PrintfSurfaceIndex = 0;
            printfBuffer.Size = 8;
            printfBuffer.Token = 0;
            populateKernelDescriptor(info->kernelDescriptor, printfBuffer);

            crossThreadOffset += 8;
        }

        ClDeviceVector deviceVector;
        deviceVector.push_back(clDevice);
        MockProgram *mockProgram = new MockProgram(&context, false, deviceVector);

        if (addChildSimdSize) {
            info->childrenKernelsIdOffset.push_back({0, crossThreadOffset});
        }

        UNRECOVERABLE_IF(crossThreadSize < crossThreadOffset + 8);
        info->crossThreadData = new char[crossThreadSize];

        auto parent = new MockParentKernel(mockProgram, *info);
        parent->crossThreadData = new char[crossThreadSize];
        memset(parent->crossThreadData, 0, crossThreadSize);
        parent->crossThreadDataSize = crossThreadSize;
        parent->mockKernelInfo = info;

        auto infoBlock = new KernelInfo();

        infoBlock->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Stateless;

        SPatchAllocateStatelessDefaultDeviceQueueSurface allocateDeviceQueueSurfaceBlock = {};
        allocateDeviceQueueSurfaceBlock.DataParamOffset = crossThreadOffsetBlock;
        allocateDeviceQueueSurfaceBlock.DataParamSize = 8;
        allocateDeviceQueueSurfaceBlock.Size = 8;
        populateKernelDescriptor(infoBlock->kernelDescriptor, allocateDeviceQueueSurfaceBlock);

        crossThreadOffsetBlock += 8;

        SPatchAllocateStatelessEventPoolSurface allocateEventPoolSurfaceBlock = {};
        allocateEventPoolSurfaceBlock.DataParamOffset = crossThreadOffsetBlock;
        allocateEventPoolSurfaceBlock.DataParamSize = 8;
        allocateEventPoolSurfaceBlock.EventPoolSurfaceIndex = 0;
        allocateEventPoolSurfaceBlock.Size = 8;
        populateKernelDescriptor(infoBlock->kernelDescriptor, allocateEventPoolSurfaceBlock);

        crossThreadOffsetBlock += 8;
        if (addPrintfForBlock) {
            SPatchAllocateStatelessPrintfSurface printfBufferBlock = {};
            printfBufferBlock.DataParamOffset = crossThreadOffsetBlock;
            printfBufferBlock.DataParamSize = 8;
            printfBufferBlock.PrintfSurfaceIndex = 0;
            printfBufferBlock.Size = 8;
            printfBufferBlock.Token = 0;
            populateKernelDescriptor(infoBlock->kernelDescriptor, printfBufferBlock);

            crossThreadOffsetBlock += 8;
        }

        if (addChildGlobalMemory) {
            SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization globalMemoryBlock = {};
            globalMemoryBlock.DataParamOffset = crossThreadOffsetBlock;
            globalMemoryBlock.DataParamSize = 8;
            globalMemoryBlock.Size = 8;
            globalMemoryBlock.SurfaceStateHeapOffset = 0;
            populateKernelDescriptor(infoBlock->kernelDescriptor, globalMemoryBlock);
            crossThreadOffsetBlock += 8;
        }

        if (addChildConstantMemory) {
            SPatchAllocateStatelessConstantMemorySurfaceWithInitialization constantMemoryBlock = {};
            constantMemoryBlock.DataParamOffset = crossThreadOffsetBlock;
            constantMemoryBlock.DataParamSize = 8;
            constantMemoryBlock.Size = 8;
            constantMemoryBlock.SurfaceStateHeapOffset = 0;
            populateKernelDescriptor(infoBlock->kernelDescriptor, constantMemoryBlock);
            crossThreadOffsetBlock += 8;
        }

        SPatchThreadPayload threadPayloadBlock = {};
        threadPayloadBlock.LocalIDXPresent = 0;
        threadPayloadBlock.LocalIDYPresent = 0;
        threadPayloadBlock.LocalIDZPresent = 0;
        threadPayloadBlock.HeaderPresent = 0;
        threadPayloadBlock.Size = 128;
        populateKernelDescriptor(infoBlock->kernelDescriptor, threadPayloadBlock);

        infoBlock->kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue = true;
        infoBlock->kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::DefaultGrfNumber;
        infoBlock->kernelDescriptor.kernelAttributes.simdSize = 32;

        SPatchDataParameterStream streamBlock = {};
        streamBlock.DataParameterStreamSize = 0;
        streamBlock.Size = 0;
        populateKernelDescriptor(infoBlock->kernelDescriptor, streamBlock);

        SPatchBindingTableState bindingTable = {};
        bindingTable.Count = 0;
        bindingTable.Offset = 0;
        bindingTable.Size = 0;
        bindingTable.SurfaceStateOffset = 0;
        populateKernelDescriptor(infoBlock->kernelDescriptor, bindingTable);

        SPatchInterfaceDescriptorData idData = {};
        idData.BindingTableOffset = 0;
        idData.KernelOffset = 0;
        idData.Offset = 0;
        idData.SamplerStateOffset = 0;
        idData.Size = 0;
        populateKernelDescriptor(infoBlock->kernelDescriptor, idData);

        infoBlock->heapInfo.pDsh = (void *)new uint64_t[64];
        infoBlock->crossThreadData = new char[crossThreadOffsetBlock > crossThreadSize ? crossThreadOffsetBlock : crossThreadSize];

        mockProgram->blockKernelManager->addBlockKernelInfo(infoBlock);
        parent->mockProgram = mockProgram;

        return parent;
    }

    MockParentKernel(Program *programArg, const KernelInfo &kernelInfoArg) : Kernel(programArg, kernelInfoArg, *programArg->getDevices()[0], false) {
    }

    ~MockParentKernel() override {
        delete &kernelInfo;
        BlockKernelManager *blockManager = program->getBlockKernelManager();

        for (uint32_t i = 0; i < blockManager->getCount(); i++) {
            const KernelInfo *blockInfo = blockManager->getBlockKernelInfo(i);
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
    using Kernel::enqueuedLocalWorkSizeX;
    using Kernel::enqueuedLocalWorkSizeY;
    using Kernel::enqueuedLocalWorkSizeZ;
    using Kernel::globalWorkOffsetX;
    using Kernel::globalWorkOffsetY;
    using Kernel::globalWorkOffsetZ;
    using Kernel::globalWorkSizeX;
    using Kernel::globalWorkSizeY;
    using Kernel::globalWorkSizeZ;
    using Kernel::localWorkSizeX;
    using Kernel::localWorkSizeX2;
    using Kernel::localWorkSizeY;
    using Kernel::localWorkSizeY2;
    using Kernel::localWorkSizeZ;
    using Kernel::localWorkSizeZ2;
    using Kernel::numWorkGroupsX;
    using Kernel::numWorkGroupsY;
    using Kernel::numWorkGroupsZ;
    MockSchedulerKernel(Program *programArg, const KernelInfo &kernelInfoArg, ClDevice &clDeviceArg) : SchedulerKernel(programArg, kernelInfoArg, clDeviceArg){};
};

class MockDebugKernel : public MockKernel {
  public:
    MockDebugKernel(Program *program, const KernelInfo &kernelInfo, ClDevice &clDeviceArg) : MockKernel(program, kernelInfo, clDeviceArg) {
        if (!isValidOffset(kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful)) {
            SPatchAllocateSystemThreadSurface allocateSystemThreadSurface = {};
            allocateSystemThreadSurface.Offset = 0;
            allocateSystemThreadSurface.PerThreadSystemThreadSurfaceSize = MockDebugKernel::perThreadSystemThreadSurfaceSize;
            populateKernelDescriptor(const_cast<KernelDescriptor &>(kernelInfo.kernelDescriptor), allocateSystemThreadSurface);
        }
    }

    ~MockDebugKernel() override {}
    static const uint32_t perThreadSystemThreadSurfaceSize;
};

} // namespace NEO
