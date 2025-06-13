/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device/device.h"
#include "shared/source/helpers/string.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_kernel_info.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/kernel/kernel_objects_for_aux_translation.h"
#include "opencl/source/kernel/multi_device_kernel.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

#include <cassert>

namespace NEO {
using namespace iOpenCL;

void populateKernelDescriptor(KernelDescriptor &dst, const SPatchExecutionEnvironment &execEnv);

struct MockKernelObjForAuxTranslation : public KernelObjForAuxTranslation {
    MockKernelObjForAuxTranslation(Type type) : KernelObjForAuxTranslation(type, nullptr) {
        if (type == KernelObjForAuxTranslation::Type::memObj) {
            mockBuffer.reset(new MockBuffer);
            this->object = mockBuffer.get();
        } else {
            DEBUG_BREAK_IF(type != KernelObjForAuxTranslation::Type::gfxAlloc);
            mockGraphicsAllocation.reset(new MockGraphicsAllocation(nullptr, 0x100));
            this->object = mockGraphicsAllocation.get();
        }
    };

    MockKernelObjForAuxTranslation(Type type, size_t size) : MockKernelObjForAuxTranslation(type) {
        if (type == KernelObjForAuxTranslation::Type::memObj) {
            mockBuffer->getGraphicsAllocation(0)->setSize(size);
        } else {
            DEBUG_BREAK_IF(type != KernelObjForAuxTranslation::Type::gfxAlloc);
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
    template <typename KernelType = Kernel>
    static MockMultiDeviceKernel *create(Program *programArg, const KernelInfoContainer &kernelInfoArg) {
        KernelVectorType kernelVector;
        kernelVector.resize(programArg->getMaxRootDeviceIndex() + 1);
        for (auto &pDevice : programArg->getDevices()) {
            auto rootDeviceIndex = pDevice->getRootDeviceIndex();
            if (kernelVector[rootDeviceIndex]) {
                continue;
            }
            kernelVector[rootDeviceIndex] = new KernelType(programArg, *kernelInfoArg[rootDeviceIndex], *pDevice);
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
    using Kernel::allBufferArgsStateful;
    using Kernel::anyKernelArgumentUsingSystemMemory;
    using Kernel::auxTranslationRequired;
    using Kernel::containsStatelessWrites;
    using Kernel::crossThreadData;
    using Kernel::crossThreadDataSize;
    using Kernel::dataParameterSimdSize;
    using Kernel::executionType;
    using Kernel::getDevice;
    using Kernel::getHardwareInfo;
    using Kernel::graphicsAllocationTypeUseSystemMemory;
    using Kernel::hasDirectStatelessAccessToHostMemory;
    using Kernel::hasDirectStatelessAccessToSharedBuffer;
    using Kernel::hasIndirectStatelessAccessToHostMemory;
    using Kernel::implicitArgsVersion;
    using Kernel::isBuiltIn;
    using Kernel::isUnifiedMemorySyncRequired;
    using Kernel::kernelArgHandlers;
    using Kernel::kernelArguments;
    using Kernel::kernelHasIndirectAccess;
    using Kernel::kernelSvmGfxAllocations;
    using Kernel::kernelUnifiedMemoryGfxAllocations;
    using Kernel::localBindingTableOffset;
    using Kernel::localIdsCache;
    using Kernel::maxKernelWorkGroupSize;
    using Kernel::maxWorkGroupSizeForCrossThreadData;
    using Kernel::numberOfBindingTableStates;
    using Kernel::parentEventOffset;
    using Kernel::patchBufferOffset;
    using Kernel::patchPrivateSurface;
    using Kernel::patchWithImplicitSurface;
    using Kernel::pImplicitArgs;
    using Kernel::preferredWkgMultipleOffset;
    using Kernel::privateSurface;
    using Kernel::setInlineSamplers;
    using Kernel::slmSizes;
    using Kernel::slmTotalSize;
    using Kernel::unifiedMemoryControls;
    using Kernel::usingImages;

    MockKernel(Program *programArg, const KernelInfo &kernelInfoArg, ClDevice &clDeviceArg)
        : Kernel(programArg, kernelInfoArg, clDeviceArg) {
        initializeLocalIdsCache();
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
        return create<KernelType>(device, program, GrfConfig::defaultGrfNumber);
    }

    template <typename KernelType = MockKernel>
    static KernelType *create(Device &device, Program *program, uint32_t grfNumber) {
        auto info = new MockKernelInfo();
        const size_t crossThreadSize = 160;

        info->setLocalIds({0, 0, 0});
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

    ////////////////////////////////////////////////////////////////////////////////
    void setCrossThreadData(const void *crossThreadDataPattern, uint32_t newCrossThreadDataSize) {
        if ((crossThreadData != nullptr) && (crossThreadData != mockCrossThreadData.data())) {
            delete[] crossThreadData;
            crossThreadData = nullptr;
            crossThreadDataSize = 0;
        }
        if (crossThreadDataPattern && (newCrossThreadDataSize > 0)) {
            mockCrossThreadData.assign((char *)crossThreadDataPattern, ((char *)crossThreadDataPattern) + newCrossThreadDataSize);
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

    void setSystolicPipelineSelectMode(bool value) { systolicPipelineSelectMode = value; }

    cl_int setArgSvmAlloc(uint32_t argIndex, void *svmPtr, GraphicsAllocation *svmAlloc, uint32_t allocId) override;

    uint32_t makeResidentCalls = 0;
    uint32_t getResidencyCalls = 0;
    uint32_t setArgSvmAllocCalls = 0;
    uint32_t moveArgsToGpuDomainCalls = 0;

    bool canKernelTransformImages = true;
    bool isPatchedOverride = true;

  protected:
    KernelInfo *kernelInfoAllocated = nullptr;
};

// class below have enough internals to service Enqueue operation.
class MockKernelWithInternals {
  public:
    MockKernelWithInternals(const ClDeviceVector &deviceVector, Context *context = nullptr, bool addDefaultArg = false, SPatchExecutionEnvironment execEnv = {}) {
        memset(&kernelHeader, 0, sizeof(SKernelBinaryHeaderCommon));

        kernelInfo.heapInfo.pKernelHeap = kernelIsa;
        kernelInfo.heapInfo.kernelHeapSize = sizeof(kernelIsa);
        kernelInfo.heapInfo.pSsh = sshLocal;
        kernelInfo.heapInfo.surfaceStateHeapSize = sizeof(sshLocal);
        kernelInfo.heapInfo.pDsh = dshLocal;
        kernelInfo.heapInfo.dynamicStateHeapSize = sizeof(dshLocal);

        populateKernelDescriptor(kernelInfo.kernelDescriptor, execEnv);
        kernelInfo.kernelDescriptor.kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;
        kernelInfo.kernelDescriptor.kernelAttributes.simdSize = 32;
        kernelInfo.setCrossThreadDataSize(sizeof(crossThreadData));
        kernelInfo.setLocalIds({1, 1, 1});
        kernelInfo.kernelDescriptor.kernelAttributes.numLocalIdChannels = 3;

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

            kernelInfo.addArgBuffer(1, 8, sizeof(uintptr_t), 0);
            kernelInfo.setAddressQualifier(1, KernelArgMetadata::AddrGlobal);
            kernelInfo.setAccessQualifier(1, KernelArgMetadata::AccessReadWrite);

            mockKernel->setKernelArguments(defaultKernelArguments);
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
    alignas(64) char sshLocal[128];
    alignas(64) char dshLocal[128];
    char crossThreadData[256];
    uint32_t kernelIsa[32];
    MockKernelInfo kernelInfo;

    MockMultiDeviceKernel *mockMultiDeviceKernel = nullptr;
    MockKernel *mockKernel;
    MockProgram *mockProgram;
    Context *mockContext;
    KernelInfoContainer kernelInfos;
    SKernelBinaryHeaderCommon kernelHeader = {};
    std::vector<Kernel::SimpleKernelArgInfo> defaultKernelArguments;
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
