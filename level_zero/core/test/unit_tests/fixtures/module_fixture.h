/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"

#include "level_zero/core/source/module/module_imp.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

struct ModuleImmutableDataFixture : public DeviceFixture {
    struct MockImmutableMemoryManager : public NEO::MockMemoryManager {
        MockImmutableMemoryManager(NEO::ExecutionEnvironment &executionEnvironment);
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
        MockImmutableData(uint32_t perHwThreadPrivateMemorySize);
        MockImmutableData(uint32_t perHwThreadPrivateMemorySize, uint32_t perThreadScratchSize, uint32_t perThreaddPrivateScratchSize);
        void setDevice(L0::Device *inDevice) {
            device = inDevice;
        }

        ~MockImmutableData() override;
        void resizeExplicitArgs(size_t size);

        NEO::KernelDescriptor *mockKernelDescriptor = nullptr;
        char kernelHeap[MemoryConstants::pageSize] = {};
        NEO::KernelInfo *mockKernelInfo = nullptr;
    };

    struct MockModule : public L0::ModuleImp {
        using ModuleImp::allocatePrivateMemoryPerDispatch;
        using ModuleImp::defaultMaxGroupSize;
        using ModuleImp::getKernelImmutableDataVector;
        using ModuleImp::kernelImmDatas;
        using ModuleImp::translationUnit;
        using ModuleImp::type;

        MockModule(L0::Device *device,
                   L0::ModuleBuildLog *moduleBuildLog,
                   L0::ModuleType type,
                   uint32_t perHwThreadPrivateMemorySize,
                   MockImmutableData *inMockKernelImmData);

        ~MockModule() override = default;

        const KernelImmutableData *getKernelImmutableData(const char *kernelName) const override {
            return mockKernelImmData;
        }

        void checkIfPrivateMemoryPerDispatchIsNeeded() override;

        MockImmutableData *mockKernelImmData = nullptr;
    };

    class MockKernel : public WhiteBox<L0::KernelImp> {
      public:
        using KernelImp::crossThreadData;
        using KernelImp::crossThreadDataSize;
        using KernelImp::dynamicStateHeapData;
        using KernelImp::dynamicStateHeapDataSize;
        using KernelImp::kernelArgHandlers;
        using KernelImp::kernelHasIndirectAccess;
        using KernelImp::kernelImmData;
        using KernelImp::kernelRequiresGenerationOfLocalIdsByRuntime;
        using KernelImp::kernelRequiresUncachedMocsCount;
        using KernelImp::printfBuffer;
        using KernelImp::privateMemoryGraphicsAllocation;
        using KernelImp::requiredWorkgroupOrder;
        using KernelImp::surfaceStateHeapData;
        using KernelImp::surfaceStateHeapDataSize;

        MockKernel(MockModule *mockModule) : WhiteBox<L0::KernelImp>(mockModule) {
        }
        void setBufferSurfaceState(uint32_t argIndex, void *address, NEO::GraphicsAllocation *alloc) override {
        }
        void evaluateIfRequiresGenerationOfLocalIdsByRuntime(const NEO::KernelDescriptor &kernelDescriptor) override {
        }
        void setCrossThreadData(uint32_t dataSize);
    };

    void setUp();

    void createModuleFromMockBinary(uint32_t perHwThreadPrivateMemorySize, bool isInternal, MockImmutableData *mockKernelImmData,
                                    std::initializer_list<ZebinTestData::appendElfAdditionalSection> additionalSections = {});

    void createKernel(MockKernel *kernel);

    void tearDown();

    const std::string kernelName = "test";
    const uint32_t numKernelArguments = 6;
    std::unique_ptr<MockModule> module;
    std::unique_ptr<ZebinTestData::ZebinWithL0TestCommonModule> zebinData;
    MockImmutableMemoryManager *memoryManager;
};

struct ModuleFixture : public DeviceFixture {

    struct ProxyModuleImp : public ModuleImp {
        using ModuleImp::ModuleImp;

        std::vector<std::unique_ptr<KernelImmutableData>> &getKernelImmDatas() {
            return kernelImmDatas;
        }

        static L0::Module *create(L0::Device *device, const ze_module_desc_t *desc,
                                  ModuleBuildLog *moduleBuildLog, ModuleType type, ze_result_t *result);
    };

    void setUp();

    void createModuleFromMockBinary(ModuleType type = ModuleType::User);

    void createKernel();

    std::unique_ptr<WhiteBox<::L0::Kernel>> createKernelWithName(std::string name);

    void tearDown();

    const std::string kernelName = "test";
    const uint32_t numKernelArguments = 6;
    std::unique_ptr<L0::Module> module;
    std::unique_ptr<WhiteBox<::L0::Kernel>> kernel;
    std::unique_ptr<ZebinTestData::ZebinWithL0TestCommonModule> zebinData;
};

struct MultiDeviceModuleFixture : public MultiDeviceFixture {
    void setUp();

    void createModuleFromMockBinary(uint32_t rootDeviceIndex);

    void createKernel(uint32_t rootDeviceIndex);

    void tearDown();

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
        MockImmutableData(L0::Device *device);

        ~MockImmutableData() override;
    };

    struct MockModuleWithZebin : public L0::ModuleImp {
        using ModuleImp::getDebugInfo;
        using ModuleImp::getZebinSegments;
        using ModuleImp::isZebinBinary;
        using ModuleImp::kernelImmDatas;
        using ModuleImp::passDebugData;
        using ModuleImp::translationUnit;
        MockModuleWithZebin(L0::Device *device);

        void addSegments();

        void addEmptyZebin();

        ~MockModuleWithZebin() override = default;

        const char strings[12] = "Hello olleH";
    };
    void setUp();

    void tearDown();
    std::unique_ptr<MockModuleWithZebin> module;
};

struct ImportHostPointerModuleFixture : public ModuleFixture {
    void setUp();

    void tearDown();

    DebugManagerStateRestore debugRestore;
    void *hostPointer = nullptr;
};

struct MultiTileModuleFixture : public MultiDeviceModuleFixture {
    MultiTileModuleFixture();
    void setUp();

    void tearDown();

    DebugManagerStateRestore debugRestore;
    VariableBackup<bool> backup;
    L0::Device *device = nullptr;
};

} // namespace ult
} // namespace L0
