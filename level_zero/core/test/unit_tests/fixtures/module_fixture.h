/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/file_io.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/program/kernel_info.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"

#include "level_zero/core/source/module/module.h"
#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_context.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

struct ModuleImmutableDataFixture : public DeviceFixture {
    struct MockImmutableMemoryManager : public NEO::MockMemoryManager {
        MockImmutableMemoryManager(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MockMemoryManager(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
        bool copyMemoryToAllocation(NEO::GraphicsAllocation *graphicsAllocation,
                                    size_t destinationOffset,
                                    const void *memoryToCopy,
                                    size_t sizeToCopy) override {
            copyMemoryToAllocationCalledTimes++;
            return true;
        }
        uint32_t copyMemoryToAllocationCalledTimes = 0;
    };

    struct MockImmutableData : KernelImmutableData {
        using KernelImmutableData::kernelDescriptor;
        using KernelImmutableData::kernelInfo;
        MockImmutableData(uint32_t perHwThreadPrivateMemorySize) {
            mockKernelDescriptor = new NEO::KernelDescriptor;
            mockKernelDescriptor->kernelAttributes.perHwThreadPrivateMemorySize = perHwThreadPrivateMemorySize;
            kernelDescriptor = mockKernelDescriptor;

            mockKernelInfo = new NEO::KernelInfo;
            mockKernelInfo->heapInfo.pKernelHeap = kernelHeap;
            mockKernelInfo->heapInfo.KernelHeapSize = MemoryConstants::pageSize;
            kernelInfo = mockKernelInfo;

            if (getIsaGraphicsAllocation() != nullptr) {
                device->getNEODevice()->getMemoryManager()->freeGraphicsMemory(&*isaGraphicsAllocation);
                isaGraphicsAllocation.release();
            }
            isaGraphicsAllocation.reset(new NEO::MockGraphicsAllocation(0,
                                                                        NEO::GraphicsAllocation::AllocationType::KERNEL_ISA,
                                                                        reinterpret_cast<void *>(0x1234),
                                                                        0x1000,
                                                                        0,
                                                                        sizeof(uint32_t),
                                                                        MemoryPool::System4KBPages));
        }

        void setDevice(L0::Device *inDevice) {
            device = inDevice;
        }

        ~MockImmutableData() override {
            delete mockKernelInfo;
            delete mockKernelDescriptor;
        }
        void resizeExplicitArgs(size_t size) {
            kernelDescriptor->payloadMappings.explicitArgs.resize(size);
        }
        NEO::KernelDescriptor *mockKernelDescriptor = nullptr;
        char kernelHeap[MemoryConstants::pageSize] = {};
        NEO::KernelInfo *mockKernelInfo = nullptr;
    };

    struct MockModule : public L0::ModuleImp {
        using ModuleImp::getKernelImmutableDataVector;
        using ModuleImp::maxGroupSize;
        using ModuleImp::translationUnit;
        using ModuleImp::type;
        MockModule(L0::Device *device,
                   L0::ModuleBuildLog *moduleBuildLog,
                   L0::ModuleType type,
                   uint32_t perHwThreadPrivateMemorySize,
                   MockImmutableData *inMockKernelImmData) : ModuleImp(device, moduleBuildLog, type), mockKernelImmData(inMockKernelImmData) {
            mockKernelImmData->setDevice(device);
        }

        ~MockModule() {
        }

        const KernelImmutableData *getKernelImmutableData(const char *functionName) const override {
            return mockKernelImmData;
        }

        void checkIfPrivateMemoryPerDispatchIsNeeded() override {
            const_cast<KernelDescriptor &>(kernelImmDatas[0]->getDescriptor()).kernelAttributes.perHwThreadPrivateMemorySize = mockKernelImmData->getDescriptor().kernelAttributes.perHwThreadPrivateMemorySize;
            ModuleImp::checkIfPrivateMemoryPerDispatchIsNeeded();
        }

        MockImmutableData *mockKernelImmData = nullptr;
    };

    class MockKernel : public WhiteBox<L0::KernelImp> {
      public:
        using KernelImp::kernelArgHandlers;
        using KernelImp::kernelHasIndirectAccess;
        using L0::KernelImp::privateMemoryGraphicsAllocation;

        MockKernel(MockModule *mockModule) : WhiteBox<L0::KernelImp>(mockModule) {
        }
        void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {
            return;
        }
        void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {
            return;
        }
        ~MockKernel() override {
        }
        std::unique_ptr<Kernel> clone() const override { return nullptr; }
    };

    void SetUp() override {
        DeviceFixture::SetUp();
        memoryManager = new MockImmutableMemoryManager(*neoDevice->executionEnvironment);
        neoDevice->executionEnvironment->memoryManager.reset(memoryManager);
    }

    void createModuleFromBinary(uint32_t perHwThreadPrivateMemorySize, bool isInternal, MockImmutableData *mockKernelImmData) {
        std::string testFile;
        retrieveBinaryKernelFilenameNoRevision(testFile, binaryFilename + "_", ".bin");

        size_t size = 0;
        auto src = loadDataFromFile(
            testFile.c_str(),
            size);

        ASSERT_NE(0u, size);
        ASSERT_NE(nullptr, src);

        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
        moduleDesc.inputSize = size;

        ModuleBuildLog *moduleBuildLog = nullptr;

        module = std::make_unique<MockModule>(device,
                                              moduleBuildLog,
                                              ModuleType::User,
                                              perHwThreadPrivateMemorySize,
                                              mockKernelImmData);

        module->type = isInternal ? ModuleType::Builtin : ModuleType::User;
        bool result = module->initialize(&moduleDesc, device->getNEODevice());
        EXPECT_TRUE(result);
    }

    void createKernel(MockKernel *kernel) {
        ze_kernel_desc_t desc = {};
        desc.pKernelName = kernelName.c_str();
        kernel->initialize(&desc);
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    const std::string binaryFilename = "test_kernel";
    const std::string kernelName = "test";
    const uint32_t numKernelArguments = 6;
    std::unique_ptr<MockModule> module;
    MockImmutableMemoryManager *memoryManager;
};

struct ModuleFixture : public DeviceFixture {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        DeviceFixture::SetUp();
        createModuleFromBinary();
    }

    void createModuleFromBinary(ModuleType type = ModuleType::User) {
        std::string testFile;
        retrieveBinaryKernelFilenameNoRevision(testFile, binaryFilename + "_", ".bin");

        size_t size = 0;
        auto src = loadDataFromFile(
            testFile.c_str(),
            size);

        ASSERT_NE(0u, size);
        ASSERT_NE(nullptr, src);

        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
        moduleDesc.inputSize = size;

        ModuleBuildLog *moduleBuildLog = nullptr;

        module.reset(Module::create(device, &moduleDesc, moduleBuildLog, type));
    }

    void createKernel() {
        ze_kernel_desc_t desc = {};
        desc.pKernelName = kernelName.c_str();

        kernel = std::make_unique<WhiteBox<::L0::Kernel>>();
        kernel->module = module.get();
        kernel->initialize(&desc);
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    const std::string binaryFilename = "test_kernel";
    const std::string kernelName = "test";
    const uint32_t numKernelArguments = 6;
    std::unique_ptr<L0::Module> module;
    std::unique_ptr<WhiteBox<::L0::Kernel>> kernel;
};

struct MultiDeviceModuleFixture : public MultiDeviceFixture {
    void SetUp() override {
        MultiDeviceFixture::SetUp();
        modules.resize(numRootDevices);
    }

    void createModuleFromBinary(uint32_t rootDeviceIndex) {
        std::string testFile;
        retrieveBinaryKernelFilenameNoRevision(testFile, binaryFilename + "_", ".bin");

        size_t size = 0;
        auto src = loadDataFromFile(testFile.c_str(), size);

        ASSERT_NE(0u, size);
        ASSERT_NE(nullptr, src);

        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.get());
        moduleDesc.inputSize = size;

        ModuleBuildLog *moduleBuildLog = nullptr;

        auto device = driverHandle->devices[rootDeviceIndex];
        modules[rootDeviceIndex].reset(Module::create(device,
                                                      &moduleDesc,
                                                      moduleBuildLog, ModuleType::User));
    }

    void createKernel(uint32_t rootDeviceIndex) {
        ze_kernel_desc_t desc = {};
        desc.pKernelName = kernelName.c_str();

        kernel = std::make_unique<WhiteBox<::L0::Kernel>>();
        kernel->module = modules[rootDeviceIndex].get();
        kernel->initialize(&desc);
    }

    void TearDown() override {
        MultiDeviceFixture::TearDown();
    }

    const std::string binaryFilename = "test_kernel";
    const std::string kernelName = "test";
    const uint32_t numKernelArguments = 6;
    std::vector<std::unique_ptr<L0::Module>> modules;
    std::unique_ptr<WhiteBox<::L0::Kernel>> kernel;
};

struct ImportHostPointerModuleFixture : public ModuleFixture {
    void SetUp() override {
        DebugManager.flags.EnableHostPointerImport.set(1);
        ModuleFixture::SetUp();

        hostPointer = driverHandle->getMemoryManager()->allocateSystemMemory(MemoryConstants::pageSize, MemoryConstants::pageSize);
    }

    void TearDown() override {
        driverHandle->getMemoryManager()->freeSystemMemory(hostPointer);
        ModuleFixture::TearDown();
    }

    DebugManagerStateRestore debugRestore;
    void *hostPointer = nullptr;
};

} // namespace ult
} // namespace L0
