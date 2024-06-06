/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/libult/create_command_stream.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/mock_wddm_residency_allocations_container.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"
#include "shared/test/common/os_interface/windows/gdi_dll_fixture.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/mocks/mock_context.h"

#include <type_traits>

using namespace NEO;

class ClWddmMemoryManagerFixture : public GdiDllFixture {
  public:
    ClWddmMemoryManagerFixture();
    ~ClWddmMemoryManagerFixture();
    void setUp();

    void tearDown() {
        GdiDllFixture::tearDown();
    }

    ExecutionEnvironment &executionEnvironment;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    WddmMock *wddm = nullptr;
    const uint32_t rootDeviceIndex = 0u;
};

using ClWddmMemoryManagerTest = Test<ClWddmMemoryManagerFixture>;

class ClMockWddmMemoryManagerFixture {
  public:
    void setUp() {
        auto osEnvironment = new OsEnvironmentWin();
        gdi = new MockGdi();
        osEnvironment->gdi.reset(gdi);
        executionEnvironment.osEnvironment.reset(osEnvironment);

        executionEnvironment.prepareRootDeviceEnvironments(2u);
        for (auto i = 0u; i < executionEnvironment.rootDeviceEnvironments.size(); i++) {
            executionEnvironment.rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(defaultHwInfo.get());
            executionEnvironment.rootDeviceEnvironments[i]->initGmm();
            auto wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[i]));
            constexpr uint64_t heap32Base = (is32bit) ? 0x1000 : 0x800000000000;
            wddm->setHeap32(heap32Base, 1000 * MemoryConstants::pageSize - 1);
            wddm->init();
        }
        rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
        wddm = static_cast<WddmMock *>(rootDeviceEnvironment->osInterface->getDriverModel()->as<Wddm>());
        rootDeviceEnvironment->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        executionEnvironment.initializeMemoryManager();

        memoryManager = std::make_unique<MockWddmMemoryManager>(executionEnvironment);
        csr.reset(createCommandStream(executionEnvironment, 0u, 1));
        auto hwInfo = rootDeviceEnvironment->getHardwareInfo();
        auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
        osContext = memoryManager->createAndRegisterOsContext(csr.get(), EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment)[0],
                                                                                                                      PreemptionHelper::getDefaultPreemptionMode(*hwInfo)));
        osContext->ensureContextInitialized(false);

        osContext->incRefInternal();
        mockTemporaryResources = reinterpret_cast<MockWddmResidentAllocationsContainer *>(wddm->getTemporaryResourcesContainer());
    }

    void tearDown() {
        osContext->decRefInternal();
    }

    MockExecutionEnvironment executionEnvironment{};
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    std::unique_ptr<CommandStreamReceiver> csr;

    WddmMock *wddm = nullptr;
    MockWddmResidentAllocationsContainer *mockTemporaryResources;
    OsContext *osContext = nullptr;
    MockGdi *gdi = nullptr;
};

class BufferWithWddmMemory : public ::testing::Test,
                             public ClWddmMemoryManagerFixture {
  public:
  protected:
    void SetUp() override {
        ClWddmMemoryManagerFixture::setUp();
        tmp = context.getMemoryManager();
        context.memoryManager = memoryManager.get();
        flags = 0;
    }

    void TearDown() override {
        context.memoryManager = tmp;
        ClWddmMemoryManagerFixture::tearDown();
    }

    MemoryManager *tmp;
    MockContext context;
    cl_mem_flags flags;
    cl_int retVal;
};
