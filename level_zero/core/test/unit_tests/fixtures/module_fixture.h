/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/unit_test/device_binary_format/zebin_tests.h"

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
        using KernelImmutableData::crossThreadDataSize;
        using KernelImmutableData::crossThreadDataTemplate;
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
                                                                        NEO::AllocationType::KERNEL_ISA,
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
        using ModuleImp::kernelImmDatas;
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
        using KernelImp::crossThreadData;
        using KernelImp::crossThreadDataSize;
        using KernelImp::kernelArgHandlers;
        using KernelImp::kernelHasIndirectAccess;
        using KernelImp::kernelRequiresGenerationOfLocalIdsByRuntime;
        using KernelImp::privateMemoryGraphicsAllocation;
        using KernelImp::requiredWorkgroupOrder;

        MockKernel(MockModule *mockModule) : WhiteBox<L0::KernelImp>(mockModule) {
        }
        void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {
            return;
        }
        void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {
            return;
        }
        void setCrossThreadData(uint32_t dataSize) {
            crossThreadData.reset(new uint8_t[dataSize]);
            crossThreadDataSize = dataSize;
            memset(crossThreadData.get(), 0x00, crossThreadDataSize);
        }
        ~MockKernel() override {
        }
    };

    void SetUp() {
        auto executionEnvironment = MockDevice::prepareExecutionEnvironment(NEO::defaultHwInfo.get(), 0u);
        memoryManager = new MockImmutableMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        DeviceFixture::setupWithExecutionEnvironment(*executionEnvironment);
    }

    void createModuleFromBinary(uint32_t perHwThreadPrivateMemorySize, bool isInternal, MockImmutableData *mockKernelImmData) {
        std::string testFile;
        retrieveBinaryKernelFilenameApiSpecific(testFile, binaryFilename + "_", ".bin");

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

    void TearDown() {
        DeviceFixture::TearDown();
    }

    const std::string binaryFilename = "test_kernel";
    const std::string kernelName = "test";
    const uint32_t numKernelArguments = 6;
    std::unique_ptr<MockModule> module;
    MockImmutableMemoryManager *memoryManager;
};

struct ModuleFixture : public DeviceFixture {
    void SetUp() {
        NEO::MockCompilerEnableGuard mock(true);
        DeviceFixture::SetUp();
        createModuleFromBinary();
    }

    void createModuleFromBinary(ModuleType type = ModuleType::User) {
        std::string testFile;
        retrieveBinaryKernelFilenameApiSpecific(testFile, binaryFilename + "_", ".bin");

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

    void TearDown() {
        DeviceFixture::TearDown();
    }

    const std::string binaryFilename = "test_kernel";
    const std::string kernelName = "test";
    const uint32_t numKernelArguments = 6;
    std::unique_ptr<L0::Module> module;
    std::unique_ptr<WhiteBox<::L0::Kernel>> kernel;
};

struct MultiDeviceModuleFixture : public MultiDeviceFixture {
    void SetUp() {
        MultiDeviceFixture::SetUp();
        modules.resize(numRootDevices);
    }

    void createModuleFromBinary(uint32_t rootDeviceIndex) {
        std::string testFile;
        retrieveBinaryKernelFilenameApiSpecific(testFile, binaryFilename + "_", ".bin");

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

    void TearDown() {
        MultiDeviceFixture::TearDown();
    }

    const std::string binaryFilename = "test_kernel";
    const std::string kernelName = "test";
    const uint32_t numKernelArguments = 6;
    std::vector<std::unique_ptr<L0::Module>> modules;
    std::unique_ptr<WhiteBox<::L0::Kernel>> kernel;
};

struct ModuleWithZebinFixture : public DeviceFixture {
    struct MockImmutableData : public KernelImmutableData {
        using KernelImmutableData::device;
        using KernelImmutableData::isaGraphicsAllocation;
        using KernelImmutableData::kernelDescriptor;
        MockImmutableData(L0::Device *device) {

            auto mockKernelDescriptor = new NEO::KernelDescriptor;
            mockKernelDescriptor->kernelMetadata.kernelName = "kernel";
            kernelDescriptor = mockKernelDescriptor;
            this->device = device;
            isaGraphicsAllocation.reset(new NEO::MockGraphicsAllocation(0,
                                                                        NEO::AllocationType::KERNEL_ISA,
                                                                        reinterpret_cast<void *>(0x1234),
                                                                        0x1000,
                                                                        0,
                                                                        sizeof(uint32_t),
                                                                        MemoryPool::System4KBPages));
        }

        ~MockImmutableData() {
            delete kernelDescriptor;
        }
    };

    struct MockModuleWithZebin : public L0::ModuleImp {
        using ModuleImp::getDebugInfo;
        using ModuleImp::getZebinSegments;
        using ModuleImp::kernelImmDatas;
        using ModuleImp::passDebugData;
        using ModuleImp::translationUnit;
        MockModuleWithZebin(L0::Device *device) : ModuleImp(device, nullptr, ModuleType::User) {}

        void addSegments() {
            kernelImmDatas.push_back(std::make_unique<MockImmutableData>(device));
            translationUnit->globalVarBuffer = new NEO::MockGraphicsAllocation(0,
                                                                               NEO::AllocationType::GLOBAL_SURFACE,
                                                                               reinterpret_cast<void *>(0x1234),
                                                                               0x1000,
                                                                               0,
                                                                               sizeof(uint32_t),
                                                                               MemoryPool::System4KBPages);
            translationUnit->globalConstBuffer = new NEO::MockGraphicsAllocation(0,
                                                                                 NEO::AllocationType::GLOBAL_SURFACE,
                                                                                 reinterpret_cast<void *>(0x1234),
                                                                                 0x1000,
                                                                                 0,
                                                                                 sizeof(uint32_t),
                                                                                 MemoryPool::System4KBPages);

            translationUnit->programInfo.globalStrings.initData = &strings;
            translationUnit->programInfo.globalStrings.size = sizeof(strings);
        }

        void addKernelSegment() {
        }

        void addEmptyZebin() {
            auto zebin = ZebinTestData::ValidEmptyProgram();

            translationUnit->unpackedDeviceBinarySize = zebin.storage.size();
            translationUnit->unpackedDeviceBinary.reset(new char[zebin.storage.size()]);
            memcpy_s(translationUnit->unpackedDeviceBinary.get(), translationUnit->unpackedDeviceBinarySize,
                     zebin.storage.data(), zebin.storage.size());
        }

        ~MockModuleWithZebin() {
        }

        const char strings[12] = "Hello olleH";
    };
    void SetUp() {
        NEO::MockCompilerEnableGuard mock(true);
        DeviceFixture::SetUp();
        module = std::make_unique<MockModuleWithZebin>(device);
    }

    void TearDown() {
        DeviceFixture::TearDown();
    }
    std::unique_ptr<MockModuleWithZebin> module;
};

struct ImportHostPointerModuleFixture : public ModuleFixture {
    void SetUp() {
        DebugManager.flags.EnableHostPointerImport.set(1);
        ModuleFixture::SetUp();

        hostPointer = driverHandle->getMemoryManager()->allocateSystemMemory(MemoryConstants::pageSize, MemoryConstants::pageSize);
    }

    void TearDown() {
        driverHandle->getMemoryManager()->freeSystemMemory(hostPointer);
        ModuleFixture::TearDown();
    }

    DebugManagerStateRestore debugRestore;
    void *hostPointer = nullptr;
};

struct MultiTileModuleFixture : public MultiDeviceModuleFixture {
    void SetUp() {
        DebugManager.flags.EnableImplicitScaling.set(1);
        MultiDeviceFixture::numRootDevices = 1u;
        MultiDeviceFixture::numSubDevices = 2u;

        MultiDeviceModuleFixture::SetUp();
        createModuleFromBinary(0);

        device = driverHandle->devices[0];
    }

    void TearDown() {
        MultiDeviceModuleFixture::TearDown();
    }

    DebugManagerStateRestore debugRestore;
    VariableBackup<bool> backup{&NEO::ImplicitScaling::apiSupport, true};
    L0::Device *device = nullptr;
};

} // namespace ult
} // namespace L0
