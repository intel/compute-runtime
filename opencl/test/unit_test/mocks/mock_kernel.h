/*
 * Copyright (C) 2018-2021 Intel Corporation
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
#include "opencl/test/unit_test/mocks/mock_kernel_info.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include <cassert>

namespace NEO {
using namespace iOpenCL;

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchExecutionEnvironment &execEnv);

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
    using Kernel::executionType;
    using Kernel::getDevice;
    using Kernel::getHardwareInfo;
    using Kernel::hasDirectStatelessAccessToHostMemory;
    using Kernel::hasDirectStatelessAccessToSharedBuffer;
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
    using Kernel::maxKernelWorkGroupSize;
    using Kernel::maxWorkGroupSizeForCrossThreadData;
    using Kernel::numberOfBindingTableStates;
    using Kernel::parentEventOffset;
    using Kernel::patchBufferOffset;
    using Kernel::patchWithImplicitSurface;
    using Kernel::pImplicitArgs;
    using Kernel::preferredWkgMultipleOffset;
    using Kernel::privateSurface;
    using Kernel::singleSubdevicePreferredInCurrentEnqueue;
    using Kernel::svmAllocationsRequireCacheFlush;
    using Kernel::threadArbitrationPolicy;
    using Kernel::unifiedMemoryControls;

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
        auto info = new MockKernelInfo();
        const size_t crossThreadSize = 160;

        info->setLocalIds({0, 0, 0});
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

        kernelInfo.heapInfo.pKernelHeap = kernelIsa;
        kernelInfo.heapInfo.KernelHeapSize = sizeof(kernelIsa);
        kernelInfo.heapInfo.pSsh = sshLocal;
        kernelInfo.heapInfo.SurfaceStateHeapSize = sizeof(sshLocal);
        kernelInfo.heapInfo.pDsh = dshLocal;
        kernelInfo.heapInfo.DynamicStateHeapSize = sizeof(dshLocal);

        populateKernelDescriptor(kernelInfo.kernelDescriptor, execEnv);
        kernelInfo.kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::DefaultGrfNumber;
        kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 32;
        kernelInfo.setCrossThreadDataSize(sizeof(crossThreadData));
        kernelInfo.setLocalIds({1, 1, 1});

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

            kernelInfo.addArgBuffer(0, 0, sizeof(uintptr_t), 64);
            kernelInfo.setAddressQualifier(0, KernelArgMetadata::AddrGlobal);
            kernelInfo.setAccessQualifier(0, KernelArgMetadata::AccessReadWrite);

            kernelInfo.addArgBuffer(1, 8, sizeof(uintptr_t), 72);
            kernelInfo.setAddressQualifier(1, KernelArgMetadata::AddrGlobal);
            kernelInfo.setAccessQualifier(1, KernelArgMetadata::AccessReadWrite);

            mockKernel->setKernelArguments(defaultKernelArguments);
            mockKernel->kernelArgRequiresCacheFlush.resize(2);
            mockKernel->kernelArgHandlers.resize(2);
            mockKernel->kernelArgHandlers[0] = &Kernel::setArgBuffer;
            mockKernel->kernelArgHandlers[1] = &Kernel::setArgBuffer;
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
    MockKernelInfo kernelInfo;
    SKernelBinaryHeaderCommon kernelHeader = {};
    uint32_t kernelIsa[32];
    char crossThreadData[256];
    char sshLocal[128];
    char dshLocal[128];
    std::vector<Kernel::SimpleKernelArgInfo> defaultKernelArguments;
};

class MockParentKernel : public Kernel {
  public:
    struct CreateParams {
        bool addChildSimdSize = false;
        bool addChildGlobalMemory = false;
        bool addChildConstantMemory = false;
        bool addPrintfForParent = false;
        bool addPrintfForBlock = false;
    };
    using Kernel::auxTranslationRequired;
    using Kernel::kernelInfo;
    using Kernel::patchBlocksCurbeWithConstantValues;
    using Kernel::pImplicitArgs;
    using Kernel::pSshLocal;
    using Kernel::sshLocalSize;
    static MockParentKernel *create(Context &context) {
        CreateParams createParams{};
        return create(context, createParams);
    }
    static MockParentKernel *create(Context &context, const CreateParams &createParams) {
        auto clDevice = context.getDevice(0);

        auto info = new MockKernelInfo();
        const size_t crossThreadSize = 160;
        uint32_t crossThreadOffset = 0;
        uint32_t crossThreadOffsetBlock = 0;

        info->setLocalIds({0, 0, 0});

        info->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Stateless;
        info->kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue = true;
        info->kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::DefaultGrfNumber;
        info->kernelDescriptor.kernelAttributes.simdSize = 32;

        info->setDeviceSideEnqueueDefaultQueueSurface(8, crossThreadOffset);
        crossThreadOffset += 8;

        info->setDeviceSideEnqueueEventPoolSurface(8, crossThreadOffset);
        crossThreadOffset += 8;

        if (createParams.addPrintfForParent) {
            info->setPrintfSurface(8, crossThreadOffset);
            crossThreadOffset += 8;
        }

        ClDeviceVector deviceVector;
        deviceVector.push_back(clDevice);
        MockProgram *mockProgram = new MockProgram(&context, false, deviceVector);

        if (createParams.addChildSimdSize) {
            info->childrenKernelsIdOffset.push_back({0, crossThreadOffset});
        }

        UNRECOVERABLE_IF(crossThreadSize < crossThreadOffset + 8);
        info->crossThreadData = new char[crossThreadSize];

        auto parent = new MockParentKernel(mockProgram, *info);
        parent->crossThreadData = new char[crossThreadSize];
        memset(parent->crossThreadData, 0, crossThreadSize);
        parent->crossThreadDataSize = crossThreadSize;
        parent->mockKernelInfo = info;

        auto infoBlock = new MockKernelInfo();

        infoBlock->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::Stateless;

        infoBlock->setDeviceSideEnqueueDefaultQueueSurface(8, crossThreadOffsetBlock);
        crossThreadOffsetBlock += 8;

        infoBlock->setDeviceSideEnqueueEventPoolSurface(8, crossThreadOffset);
        crossThreadOffsetBlock += 8;

        if (createParams.addPrintfForBlock) {
            infoBlock->setPrintfSurface(8, crossThreadOffsetBlock);
            crossThreadOffsetBlock += 8;
        }

        if (createParams.addChildGlobalMemory) {
            infoBlock->setGlobalVariablesSurface(8, crossThreadOffsetBlock);
            crossThreadOffsetBlock += 8;
        }

        if (createParams.addChildConstantMemory) {
            infoBlock->setGlobalConstantsSurface(8, crossThreadOffsetBlock);
            crossThreadOffsetBlock += 8;
        }

        infoBlock->setLocalIds({0, 0, 0});

        infoBlock->kernelDescriptor.kernelAttributes.flags.usesDeviceSideEnqueue = true;
        infoBlock->kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::DefaultGrfNumber;
        infoBlock->kernelDescriptor.kernelAttributes.simdSize = 32;

        infoBlock->setDeviceSideEnqueueBlockInterfaceDescriptorOffset(0);

        infoBlock->heapInfo.pDsh = (void *)new uint64_t[64];
        infoBlock->heapInfo.DynamicStateHeapSize = 64 * sizeof(uint64_t);

        size_t crossThreadDataSize = crossThreadOffsetBlock > crossThreadSize ? crossThreadOffsetBlock : crossThreadSize;
        infoBlock->crossThreadData = new char[crossThreadDataSize];
        infoBlock->setCrossThreadDataSize(static_cast<uint16_t>(crossThreadDataSize));

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
    MockSchedulerKernel(Program *programArg, const KernelInfo &kernelInfoArg, ClDevice &clDeviceArg) : SchedulerKernel(programArg, kernelInfoArg, clDeviceArg){};
};

class MockDebugKernel : public MockKernel {
  public:
    MockDebugKernel(Program *program, const KernelInfo &kernelInfo, ClDevice &clDeviceArg) : MockKernel(program, kernelInfo, clDeviceArg) {
        if (!isValidOffset(kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful)) {
            auto &kd = const_cast<KernelDescriptor &>(kernelInfo.kernelDescriptor);
            kd.payloadMappings.implicitArgs.systemThreadSurfaceAddress.bindful = 0;
            kd.kernelAttributes.perThreadSystemThreadSurfaceSize = MockDebugKernel::perThreadSystemThreadSurfaceSize;
        }
    }

    ~MockDebugKernel() override {}
    static const uint32_t perThreadSystemThreadSurfaceSize;
};

} // namespace NEO
