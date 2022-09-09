/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/program/kernel_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/mocks/mock_compilers.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"

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
            auto ptr = reinterpret_cast<void *>(0x1234);
            auto gmmHelper = std::make_unique<GmmHelper>(nullptr, defaultHwInfo.get());
            auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
            isaGraphicsAllocation.reset(new NEO::MockGraphicsAllocation(0,
                                                                        NEO::AllocationType::KERNEL_ISA,
                                                                        ptr,
                                                                        0x1000,
                                                                        0u,
                                                                        MemoryPool::System4KBPages,
                                                                        MemoryManager::maxOsContextCount,
                                                                        canonizedGpuAddress));
            kernelInfo->kernelAllocation = isaGraphicsAllocation.get();
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

        ~MockModule() override {
        }

        const KernelImmutableData *getKernelImmutableData(const char *kernelName) const override {
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

    void setUp() {
        auto executionEnvironment = MockDevice::prepareExecutionEnvironment(NEO::defaultHwInfo.get(), 0u);
        memoryManager = new MockImmutableMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        DeviceFixture::setupWithExecutionEnvironment(*executionEnvironment);
    }

    void createModuleFromMockBinary(uint32_t perHwThreadPrivateMemorySize, bool isInternal, MockImmutableData *mockKernelImmData,
                                    std::initializer_list<ZebinTestData::appendElfAdditionalSection> additionalSections = {}) {
        zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo(), additionalSections);
        const auto &src = zebinData->storage;

        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
        moduleDesc.inputSize = src.size();

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

    void tearDown() {
        module.reset(nullptr);
        DeviceFixture::tearDown();
    }

    const std::string kernelName = "test";
    const uint32_t numKernelArguments = 6;
    std::unique_ptr<MockModule> module;
    std::unique_ptr<ZebinTestData::ZebinWithL0TestCommonModule> zebinData;
    MockImmutableMemoryManager *memoryManager;
};

struct ModuleFixture : public DeviceFixture {
    void setUp() {

        DeviceFixture::setUp();
        createModuleFromMockBinary();
    }

    void createModuleFromMockBinary(ModuleType type = ModuleType::User) {
        zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
        const auto &src = zebinData->storage;

        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
        moduleDesc.inputSize = src.size();

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

    void tearDown() {
        kernel.reset(nullptr);
        module.reset(nullptr);
        DeviceFixture::tearDown();
    }

    const std::string kernelName = "test";
    const uint32_t numKernelArguments = 6;
    std::unique_ptr<L0::Module> module;
    std::unique_ptr<WhiteBox<::L0::Kernel>> kernel;
    std::unique_ptr<ZebinTestData::ZebinWithL0TestCommonModule> zebinData;
};

struct MultiDeviceModuleFixture : public MultiDeviceFixture {
    void setUp() {
        MultiDeviceFixture::setUp();
        modules.resize(numRootDevices);
    }

    void createModuleFromMockBinary(uint32_t rootDeviceIndex) {
        auto device = driverHandle->devices[rootDeviceIndex];
        zebinData = std::make_unique<ZebinTestData::ZebinWithL0TestCommonModule>(device->getHwInfo());
        const auto &src = zebinData->storage;

        ze_module_desc_t moduleDesc = {};
        moduleDesc.format = ZE_MODULE_FORMAT_NATIVE;
        moduleDesc.pInputModule = reinterpret_cast<const uint8_t *>(src.data());
        moduleDesc.inputSize = src.size();

        ModuleBuildLog *moduleBuildLog = nullptr;

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

    void tearDown() {
        kernel.reset(nullptr);
        for (auto &module : modules) {
            module.reset(nullptr);
        }
        MultiDeviceFixture::tearDown();
    }

    const std::string kernelName = "test";
    const uint32_t numKernelArguments = 6;
    std::vector<std::unique_ptr<L0::Module>> modules;
    std::unique_ptr<WhiteBox<::L0::Kernel>> kernel;
    std::unique_ptr<ZebinTestData::ZebinWithL0TestCommonModule> zebinData;
};

struct ModuleWithZebinFixture : public DeviceFixture {
    struct MockImmutableData : public KernelImmutableData {
        using KernelImmutableData::device;
        using KernelImmutableData::isaGraphicsAllocation;
        using KernelImmutableData::kernelDescriptor;
        MockImmutableData(L0::Device *device) {

            auto mockKernelDescriptor = new NEO::KernelDescriptor;
            mockKernelDescriptor->kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram::kernelName;
            kernelDescriptor = mockKernelDescriptor;
            this->device = device;
            auto ptr = reinterpret_cast<void *>(0x1234);
            auto gmmHelper = device->getNEODevice()->getGmmHelper();
            auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
            isaGraphicsAllocation.reset(new NEO::MockGraphicsAllocation(0,
                                                                        NEO::AllocationType::KERNEL_ISA,
                                                                        ptr,
                                                                        0x1000,
                                                                        0u,
                                                                        MemoryPool::System4KBPages,
                                                                        MemoryManager::maxOsContextCount,
                                                                        canonizedGpuAddress));
        }

        ~MockImmutableData() override {
            delete kernelDescriptor;
        }
    };

    struct MockModuleWithZebin : public L0::ModuleImp {
        using ModuleImp::getDebugInfo;
        using ModuleImp::getZebinSegments;
        using ModuleImp::isZebinBinary;
        using ModuleImp::kernelImmDatas;
        using ModuleImp::passDebugData;
        using ModuleImp::translationUnit;
        MockModuleWithZebin(L0::Device *device) : ModuleImp(device, nullptr, ModuleType::User) {
            isZebinBinary = true;
        }

        void addSegments() {
            kernelImmDatas.push_back(std::make_unique<MockImmutableData>(device));
            auto ptr = reinterpret_cast<void *>(0x1234);
            auto gmmHelper = device->getNEODevice()->getGmmHelper();
            auto canonizedGpuAddress = gmmHelper->canonize(castToUint64(ptr));
            translationUnit->globalVarBuffer = new NEO::MockGraphicsAllocation(0,
                                                                               NEO::AllocationType::GLOBAL_SURFACE,
                                                                               ptr,
                                                                               0x1000,
                                                                               0u,
                                                                               MemoryPool::System4KBPages,
                                                                               MemoryManager::maxOsContextCount,
                                                                               canonizedGpuAddress);
            translationUnit->globalConstBuffer = new NEO::MockGraphicsAllocation(0,
                                                                                 NEO::AllocationType::GLOBAL_SURFACE,
                                                                                 ptr,
                                                                                 0x1000,
                                                                                 0u,
                                                                                 MemoryPool::System4KBPages,
                                                                                 MemoryManager::maxOsContextCount,
                                                                                 canonizedGpuAddress);

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

        ~MockModuleWithZebin() override {
        }

        const char strings[12] = "Hello olleH";
    };
    void setUp() {

        DeviceFixture::setUp();
        module = std::make_unique<MockModuleWithZebin>(device);
    }

    void tearDown() {
        module.reset(nullptr);
        DeviceFixture::tearDown();
    }
    std::unique_ptr<MockModuleWithZebin> module;
};

struct ImportHostPointerModuleFixture : public ModuleFixture {
    void setUp() {
        DebugManager.flags.EnableHostPointerImport.set(1);
        ModuleFixture::setUp();

        hostPointer = driverHandle->getMemoryManager()->allocateSystemMemory(MemoryConstants::pageSize, MemoryConstants::pageSize);
    }

    void tearDown() {
        driverHandle->getMemoryManager()->freeSystemMemory(hostPointer);
        ModuleFixture::tearDown();
    }

    DebugManagerStateRestore debugRestore;
    void *hostPointer = nullptr;
};

struct MultiTileModuleFixture : public MultiDeviceModuleFixture {
    void setUp() {
        DebugManager.flags.EnableImplicitScaling.set(1);
        MultiDeviceFixture::numRootDevices = 1u;
        MultiDeviceFixture::numSubDevices = 2u;

        MultiDeviceModuleFixture::setUp();
        createModuleFromMockBinary(0);

        device = driverHandle->devices[0];
    }

    void tearDown() {
        MultiDeviceModuleFixture::tearDown();
    }

    DebugManagerStateRestore debugRestore;
    VariableBackup<bool> backup{&NEO::ImplicitScaling::apiSupport, true};
    L0::Device *device = nullptr;
};

} // namespace ult
} // namespace L0
