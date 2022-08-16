/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/device_binary_format/patchtokens_decoder.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hash.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/device_binary_format/patchtokens_tests.h"
#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/helpers/test_files.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/page_fault_manager/mock_cpu_page_fault_manager.h"

#include "opencl/source/api/api.h"
#include "opencl/source/context/context.h"
#include "opencl/source/gtpin/gtpin_defs.h"
#include "opencl/source/gtpin/gtpin_helpers.h"
#include "opencl/source/gtpin/gtpin_hw_helper.h"
#include "opencl/source/gtpin/gtpin_init.h"
#include "opencl/source/gtpin/gtpin_notify.h"
#include "opencl/source/helpers/cl_validators.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/program/create.inl"
#include "opencl/test/unit_test/fixtures/context_fixture.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/program/program_tests.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include "gtest/gtest.h"

#include <deque>
#include <vector>

using namespace NEO;
using namespace gtpin;

namespace NEO {
extern std::deque<gtpinkexec_t> kernelExecQueue;
extern GTPinHwHelper *gtpinHwHelperFactory[IGFX_MAX_CORE];
} // namespace NEO

namespace ULT {

int ContextCreateCallbackCount = 0;
int ContextDestroyCallbackCount = 0;
int KernelCreateCallbackCount = 0;
int KernelSubmitCallbackCount = 0;
int CommandBufferCreateCallbackCount = 0;
int CommandBufferCompleteCallbackCount = 0;
uint32_t kernelOffset = 0;
bool returnNullResource = false;

context_handle_t currContext = nullptr;

std::deque<resource_handle_t> kernelResources;
platform_info_t platformInfo;

void onContextCreate(context_handle_t context, platform_info_t *platformInfo, igc_init_t **igcInit) {
    ULT::platformInfo.gen_version = platformInfo->gen_version;
    currContext = context;
    kernelResources.clear();
    ContextCreateCallbackCount++;
    *igcInit = reinterpret_cast<igc_init_t *>(0x1234);
}

void onContextDestroy(context_handle_t context) {
    currContext = nullptr;
    EXPECT_EQ(0u, kernelResources.size());
    kernelResources.clear();
    ContextDestroyCallbackCount++;
}

void onKernelCreate(context_handle_t context, const instrument_params_in_t *paramsIn, instrument_params_out_t *paramsOut) {
    paramsOut->inst_kernel_binary = const_cast<uint8_t *>(paramsIn->orig_kernel_binary);
    paramsOut->inst_kernel_size = paramsIn->orig_kernel_size;
    paramsOut->kernel_id = paramsIn->igc_hash_id;
    KernelCreateCallbackCount++;
}

void onKernelSubmit(command_buffer_handle_t cb, uint64_t kernelId, uint32_t *entryOffset, resource_handle_t *resource) {
    resource_handle_t currResource = nullptr;
    ASSERT_NE(nullptr, currContext);
    if (!returnNullResource) {
        GTPIN_DI_STATUS st = gtpinCreateBuffer(currContext, (uint32_t)256, &currResource);
        EXPECT_EQ(GTPIN_DI_SUCCESS, st);
        EXPECT_NE(nullptr, currResource);

        uint8_t *bufAddress = nullptr;
        st = gtpinMapBuffer(currContext, currResource, &bufAddress);
        EXPECT_EQ(GTPIN_DI_SUCCESS, st);
        EXPECT_NE(nullptr, bufAddress);
    }
    *entryOffset = kernelOffset;
    *resource = currResource;
    kernelResources.push_back(currResource);

    KernelSubmitCallbackCount++;
}

void onCommandBufferCreate(context_handle_t context, command_buffer_handle_t cb) {
    CommandBufferCreateCallbackCount++;
}

void onCommandBufferComplete(command_buffer_handle_t cb) {
    ASSERT_NE(nullptr, currContext);
    resource_handle_t currResource = kernelResources[0];
    EXPECT_NE(nullptr, currResource);
    GTPIN_DI_STATUS st = gtpinUnmapBuffer(currContext, currResource);
    EXPECT_EQ(GTPIN_DI_SUCCESS, st);

    st = gtpinFreeBuffer(currContext, currResource);
    EXPECT_EQ(GTPIN_DI_SUCCESS, st);
    kernelResources.pop_front();

    CommandBufferCompleteCallbackCount++;
}

class MockMemoryManagerWithFailures : public OsAgnosticMemoryManager {
  public:
    using OsAgnosticMemoryManager::pageFaultManager;
    MockMemoryManagerWithFailures(ExecutionEnvironment &executionEnvironment) : OsAgnosticMemoryManager(executionEnvironment){};

    GraphicsAllocation *allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) override {
        if (failAllAllocationsInDevicePool) {
            failAllAllocationsInDevicePool = false;
            return nullptr;
        }
        return OsAgnosticMemoryManager::allocateGraphicsMemoryInDevicePool(allocationData, status);
    }
    bool failAllAllocationsInDevicePool = false;
};
struct MockResidentTestsPageFaultManager : public MockPageFaultManager {
    void moveAllocationToGpuDomain(void *ptr) override {
        moveAllocationToGpuDomainCalledTimes++;
        migratedAddress = ptr;
    }
    uint32_t moveAllocationToGpuDomainCalledTimes = 0;
    void *migratedAddress = nullptr;
};

class GTPinFixture : public ContextFixture, public MemoryManagementFixture {
    using ContextFixture::setUp;

  public:
    void setUp() {
        DebugManager.flags.GTPinAllocateBufferInSharedMemory.set(false);
        setUpImpl();
    }

    void setUpImpl() {
        platformsImpl->clear();
        MemoryManagementFixture::setUp();
        constructPlatform();
        pPlatform = platform();
        auto executionEnvironment = pPlatform->peekExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();
        memoryManager = new MockMemoryManagerWithFailures(*executionEnvironment);
        memoryManager->pageFaultManager.reset(new MockResidentTestsPageFaultManager());
        executionEnvironment->memoryManager.reset(memoryManager);
        initPlatform();
        pDevice = pPlatform->getClDevice(0);
        rootDeviceIndex = pDevice->getRootDeviceIndex();
        cl_device_id device = pDevice;
        ContextFixture::setUp(1, &device);

        driverServices.bufferAllocate = nullptr;
        driverServices.bufferDeallocate = nullptr;
        driverServices.bufferMap = nullptr;
        driverServices.bufferUnMap = nullptr;

        gtpinCallbacks.onContextCreate = nullptr;
        gtpinCallbacks.onContextDestroy = nullptr;
        gtpinCallbacks.onKernelCreate = nullptr;
        gtpinCallbacks.onKernelSubmit = nullptr;
        gtpinCallbacks.onCommandBufferCreate = nullptr;
        gtpinCallbacks.onCommandBufferComplete = nullptr;

        NEO::isGTPinInitialized = false;
        kernelOffset = 0;
    }

    void tearDown() {
        ContextFixture::tearDown();
        platformsImpl->clear();
        MemoryManagementFixture::tearDown();
        NEO::isGTPinInitialized = false;
    }

    Platform *pPlatform = nullptr;
    ClDevice *pDevice = nullptr;
    cl_int retVal = CL_SUCCESS;
    GTPIN_DI_STATUS retFromGtPin = GTPIN_DI_SUCCESS;
    driver_services_t driverServices;
    gtpin::ocl::gtpin_events_t gtpinCallbacks;
    MockMemoryManagerWithFailures *memoryManager = nullptr;
    uint32_t rootDeviceIndex = std::numeric_limits<uint32_t>::max();
    DebugManagerStateRestore restore;
};

typedef Test<GTPinFixture> GTPinTests;

TEST_F(GTPinTests, givenInvalidArgumentsThenGTPinInitFails) {
    bool isInitialized = false;

    retFromGtPin = GTPin_Init(nullptr, nullptr, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);
    isInitialized = gtpinIsGTPinInitialized();
    EXPECT_FALSE(isInitialized);

    retFromGtPin = GTPin_Init(&gtpinCallbacks, nullptr, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);
    isInitialized = gtpinIsGTPinInitialized();
    EXPECT_FALSE(isInitialized);

    retFromGtPin = GTPin_Init(nullptr, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);
    isInitialized = gtpinIsGTPinInitialized();
    EXPECT_FALSE(isInitialized);
}

TEST_F(GTPinTests, givenIncompleteArgumentsThenGTPinInitFails) {
    interface_version_t ver;
    ver.common = 0;
    ver.specific = 0;

    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, &ver);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    gtpinCallbacks.onContextCreate = onContextCreate;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    gtpinCallbacks.onContextDestroy = onContextDestroy;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    gtpinCallbacks.onKernelCreate = onKernelCreate;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);
}

TEST_F(GTPinTests, givenInvalidArgumentsWhenVersionArgumentIsProvidedThenGTPinInitReturnsDriverVersion) {
    interface_version_t ver;
    ver.common = 0;
    ver.specific = 0;

    retFromGtPin = GTPin_Init(nullptr, nullptr, &ver);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_EQ(gtpin::ocl::GTPIN_OCL_INTERFACE_VERSION, ver.specific);
    EXPECT_EQ(gtpin::GTPIN_COMMON_INTERFACE_VERSION, ver.common);

    retFromGtPin = GTPin_Init(&gtpinCallbacks, nullptr, &ver);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_EQ(gtpin::ocl::GTPIN_OCL_INTERFACE_VERSION, ver.specific);
    EXPECT_EQ(gtpin::GTPIN_COMMON_INTERFACE_VERSION, ver.common);

    retFromGtPin = GTPin_Init(nullptr, &driverServices, &ver);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_EQ(gtpin::ocl::GTPIN_OCL_INTERFACE_VERSION, ver.specific);
    EXPECT_EQ(gtpin::GTPIN_COMMON_INTERFACE_VERSION, ver.common);
}

TEST_F(GTPinTests, givenValidAndCompleteArgumentsThenGTPinInitSucceeds) {
    bool isInitialized = false;

    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_EQ(&NEO::gtpinCreateBuffer, driverServices.bufferAllocate);
    EXPECT_EQ(&NEO::gtpinFreeBuffer, driverServices.bufferDeallocate);
    EXPECT_EQ(&NEO::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&NEO::gtpinUnmapBuffer, driverServices.bufferUnMap);
    isInitialized = gtpinIsGTPinInitialized();
    EXPECT_TRUE(isInitialized);
}

TEST_F(GTPinTests, givenValidAndCompleteArgumentsWhenGTPinIsAlreadyInitializedThenGTPinInitFails) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_EQ(&NEO::gtpinCreateBuffer, driverServices.bufferAllocate);
    EXPECT_EQ(&NEO::gtpinFreeBuffer, driverServices.bufferDeallocate);
    EXPECT_EQ(&NEO::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&NEO::gtpinUnmapBuffer, driverServices.bufferUnMap);

    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INSTANCE_ALREADY_CREATED, retFromGtPin);
}

TEST_F(GTPinTests, givenInvalidArgumentsThenBufferAllocateFails) {
    resource_handle_t res;
    uint32_t buffSize = 400u;

    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_EQ(&NEO::gtpinCreateBuffer, driverServices.bufferAllocate);
    EXPECT_EQ(&NEO::gtpinFreeBuffer, driverServices.bufferDeallocate);
    EXPECT_EQ(&NEO::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&NEO::gtpinUnmapBuffer, driverServices.bufferUnMap);

    retFromGtPin = (*driverServices.bufferAllocate)(nullptr, buffSize, &res);
    EXPECT_NE(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_context ctxt = (cl_context)((Context *)pContext);
    retFromGtPin = (*driverServices.bufferAllocate)((gtpin::context_handle_t)ctxt, buffSize, nullptr);
    EXPECT_NE(GTPIN_DI_SUCCESS, retFromGtPin);
}

TEST_F(GTPinTests, givenInvalidArgumentsThenBufferDeallocateFails) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_EQ(&NEO::gtpinCreateBuffer, driverServices.bufferAllocate);
    ASSERT_EQ(&NEO::gtpinFreeBuffer, driverServices.bufferDeallocate);
    EXPECT_EQ(&NEO::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&NEO::gtpinUnmapBuffer, driverServices.bufferUnMap);

    retFromGtPin = (*driverServices.bufferDeallocate)(nullptr, nullptr);
    EXPECT_NE(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_context ctxt = (cl_context)((Context *)pContext);
    retFromGtPin = (*driverServices.bufferDeallocate)((gtpin::context_handle_t)ctxt, nullptr);
    EXPECT_NE(GTPIN_DI_SUCCESS, retFromGtPin);

    retFromGtPin = (*driverServices.bufferDeallocate)((gtpin::context_handle_t)ctxt, (gtpin::resource_handle_t)ctxt);
    EXPECT_NE(GTPIN_DI_SUCCESS, retFromGtPin);
}

TEST_F(GTPinTests, givenInvalidArgumentsThenBufferMapFails) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_EQ(&NEO::gtpinCreateBuffer, driverServices.bufferAllocate);
    EXPECT_EQ(&NEO::gtpinFreeBuffer, driverServices.bufferDeallocate);
    ASSERT_EQ(&NEO::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&NEO::gtpinUnmapBuffer, driverServices.bufferUnMap);

    uint8_t *mappedPtr;
    retFromGtPin = (*driverServices.bufferMap)(nullptr, nullptr, &mappedPtr);
    EXPECT_NE(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_context ctxt = (cl_context)((Context *)pContext);
    retFromGtPin = (*driverServices.bufferMap)((gtpin::context_handle_t)ctxt, nullptr, &mappedPtr);
    EXPECT_NE(GTPIN_DI_SUCCESS, retFromGtPin);

    retFromGtPin = (*driverServices.bufferMap)((gtpin::context_handle_t)ctxt, (gtpin::resource_handle_t)ctxt, &mappedPtr);
    EXPECT_NE(GTPIN_DI_SUCCESS, retFromGtPin);
}

TEST_F(GTPinTests, givenInvalidArgumentsThenBufferUnMapFails) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_EQ(&NEO::gtpinCreateBuffer, driverServices.bufferAllocate);
    EXPECT_EQ(&NEO::gtpinFreeBuffer, driverServices.bufferDeallocate);
    EXPECT_EQ(&NEO::gtpinMapBuffer, driverServices.bufferMap);
    ASSERT_EQ(&NEO::gtpinUnmapBuffer, driverServices.bufferUnMap);

    retFromGtPin = (*driverServices.bufferUnMap)(nullptr, nullptr);
    EXPECT_NE(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_context ctxt = (cl_context)((Context *)pContext);
    retFromGtPin = (*driverServices.bufferUnMap)((gtpin::context_handle_t)ctxt, nullptr);
    EXPECT_NE(GTPIN_DI_SUCCESS, retFromGtPin);

    retFromGtPin = (*driverServices.bufferUnMap)((gtpin::context_handle_t)ctxt, (gtpin::resource_handle_t)ctxt);
    EXPECT_NE(GTPIN_DI_SUCCESS, retFromGtPin);
}

TEST_F(GTPinTests, givenValidRequestForHugeMemoryAllocationThenBufferAllocateFails) {

    InjectedFunction allocBufferFunc = [this](size_t failureIndex) {
        resource_handle_t res;
        cl_context ctxt = (cl_context)((Context *)pContext);
        uint32_t hugeSize = 400u; // Will be handled as huge memory allocation
        retFromGtPin = (*driverServices.bufferAllocate)((gtpin::context_handle_t)ctxt, hugeSize, &res);
        if (MemoryManagement::nonfailingAllocation != failureIndex) {
            EXPECT_EQ(GTPIN_DI_ERROR_ALLOCATION_FAILED, retFromGtPin);
        } else {
            EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
            EXPECT_NE(nullptr, res);
            retFromGtPin = (*driverServices.bufferDeallocate)((gtpin::context_handle_t)ctxt, res);
            EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
        }
    };

    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_EQ(&NEO::gtpinCreateBuffer, driverServices.bufferAllocate);
    ASSERT_EQ(&NEO::gtpinFreeBuffer, driverServices.bufferDeallocate);
    EXPECT_EQ(&NEO::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&NEO::gtpinUnmapBuffer, driverServices.bufferUnMap);

    injectFailures(allocBufferFunc);
}

TEST_F(GTPinTests, givenValidRequestForMemoryAllocationThenBufferAllocateAndDeallocateSucceeds) {
    resource_handle_t res;
    uint32_t buffSize = 400u;

    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_EQ(&NEO::gtpinCreateBuffer, driverServices.bufferAllocate);
    ASSERT_EQ(&NEO::gtpinFreeBuffer, driverServices.bufferDeallocate);
    EXPECT_EQ(&NEO::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&NEO::gtpinUnmapBuffer, driverServices.bufferUnMap);

    cl_context ctxt = (cl_context)((Context *)pContext);
    retFromGtPin = (*driverServices.bufferAllocate)((gtpin::context_handle_t)ctxt, buffSize, &res);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_NE(nullptr, res);

    retFromGtPin = (*driverServices.bufferDeallocate)((gtpin::context_handle_t)ctxt, res);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
}

TEST_F(GTPinTests, givenValidArgumentsForBufferMapWhenCallSequenceIsCorrectThenBufferMapSucceeds) {
    resource_handle_t res;
    uint32_t buffSize = 400u;

    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_EQ(&NEO::gtpinCreateBuffer, driverServices.bufferAllocate);
    ASSERT_EQ(&NEO::gtpinFreeBuffer, driverServices.bufferDeallocate);
    ASSERT_EQ(&NEO::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&NEO::gtpinUnmapBuffer, driverServices.bufferUnMap);

    cl_context ctxt = (cl_context)((Context *)pContext);
    retFromGtPin = (*driverServices.bufferAllocate)((gtpin::context_handle_t)ctxt, buffSize, &res);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_NE(nullptr, res);

    uint8_t *mappedPtr = nullptr;
    retFromGtPin = (*driverServices.bufferMap)((gtpin::context_handle_t)ctxt, res, &mappedPtr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_NE(nullptr, mappedPtr);

    retFromGtPin = (*driverServices.bufferDeallocate)((gtpin::context_handle_t)ctxt, res);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
}

TEST_F(GTPinTests, givenMissingReturnArgumentForBufferMapWhenCallSequenceIsCorrectThenBufferMapFails) {
    resource_handle_t res;
    uint32_t buffSize = 400u;

    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_EQ(&NEO::gtpinCreateBuffer, driverServices.bufferAllocate);
    ASSERT_EQ(&NEO::gtpinFreeBuffer, driverServices.bufferDeallocate);
    ASSERT_EQ(&NEO::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&NEO::gtpinUnmapBuffer, driverServices.bufferUnMap);

    cl_context ctxt = (cl_context)((Context *)pContext);
    retFromGtPin = (*driverServices.bufferAllocate)((gtpin::context_handle_t)ctxt, buffSize, &res);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_NE(nullptr, res);

    retFromGtPin = (*driverServices.bufferMap)((gtpin::context_handle_t)ctxt, res, nullptr);
    EXPECT_NE(GTPIN_DI_SUCCESS, retFromGtPin);

    retFromGtPin = (*driverServices.bufferDeallocate)((gtpin::context_handle_t)ctxt, res);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
}

TEST_F(GTPinTests, givenValidArgumentsForBufferUnMapWhenCallSequenceIsCorrectThenBufferUnMapSucceeds) {
    resource_handle_t res;
    uint32_t buffSize = 400u;

    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_EQ(&NEO::gtpinCreateBuffer, driverServices.bufferAllocate);
    ASSERT_EQ(&NEO::gtpinFreeBuffer, driverServices.bufferDeallocate);
    ASSERT_EQ(&NEO::gtpinMapBuffer, driverServices.bufferMap);
    ASSERT_EQ(&NEO::gtpinUnmapBuffer, driverServices.bufferUnMap);

    cl_context ctxt = (cl_context)((Context *)pContext);
    retFromGtPin = (*driverServices.bufferAllocate)((gtpin::context_handle_t)ctxt, buffSize, &res);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_NE(nullptr, res);

    uint8_t *mappedPtr = nullptr;
    retFromGtPin = (*driverServices.bufferMap)((gtpin::context_handle_t)ctxt, res, &mappedPtr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_NE(nullptr, mappedPtr);

    retFromGtPin = (*driverServices.bufferUnMap)((gtpin::context_handle_t)ctxt, res);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    retFromGtPin = (*driverServices.bufferDeallocate)((gtpin::context_handle_t)ctxt, res);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
}

TEST_F(GTPinTests, givenUninitializedGTPinInterfaceThenGTPinContextCallbackIsNotCalled) {
    int prevCount = ContextCreateCallbackCount;
    cl_device_id device = (cl_device_id)pDevice;
    auto context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(ContextCreateCallbackCount, prevCount);

    prevCount = ContextDestroyCallbackCount;
    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(ContextDestroyCallbackCount, prevCount);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenContextCreationArgumentsAreInvalidThenGTPinContextCallbackIsNotCalled) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    int prevCount = ContextCreateCallbackCount;
    cl_device_id device = (cl_device_id)pDevice;
    cl_context_properties invalidProperties[3] = {CL_CONTEXT_PLATFORM, (cl_context_properties) nullptr, 0};
    auto context = clCreateContext(invalidProperties, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
    EXPECT_EQ(nullptr, context);
    EXPECT_EQ(ContextCreateCallbackCount, prevCount);

    context = clCreateContextFromType(invalidProperties, CL_DEVICE_TYPE_GPU, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
    EXPECT_EQ(nullptr, context);
    EXPECT_EQ(ContextCreateCallbackCount, prevCount);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceThenGTPinContextCallbackIsCalled) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    int prevCount = ContextCreateCallbackCount;
    cl_device_id device = (cl_device_id)pDevice;
    auto context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(ContextCreateCallbackCount, prevCount + 1);

    prevCount = ContextDestroyCallbackCount;
    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(ContextDestroyCallbackCount, prevCount + 1);

    prevCount = ContextCreateCallbackCount;
    context = clCreateContextFromType(nullptr, CL_DEVICE_TYPE_GPU, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(ContextCreateCallbackCount, prevCount + 1);

    prevCount = ContextDestroyCallbackCount;
    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(ContextDestroyCallbackCount, prevCount + 1);
}

TEST_F(GTPinTests, givenUninitializedGTPinInterfaceThenGTPinKernelCreateCallbackIsNotCalled) {
    cl_kernel kernel = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    size_t sourceSize = 0;
    std::string testFile;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        (cl_context)((Context *)pContext),
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, pProgram);

    retVal = clBuildProgram(
        pProgram,
        1,
        &device,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    int prevCount = KernelCreateCallbackCount;
    kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    EXPECT_NE(nullptr, kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount, KernelCreateCallbackCount);

    // Cleanup
    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenContextIsCreatedThenCorrectVersionIsSet) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_device_id device = static_cast<cl_device_id>(pDevice);
    cl_context context = nullptr;

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);
    GFXCORE_FAMILY genFamily = pDevice->getHardwareInfo().platform.eRenderCoreFamily;
    GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(genFamily);
    EXPECT_EQ(ULT::platformInfo.gen_version, static_cast<gtpin::GTPIN_GEN_VERSION>(gtpinHelper.getGenVersion()));

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenKernelIsExecutedThenGTPinCallbacksAreCalled) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_kernel kernel1 = nullptr;
    cl_kernel kernel2 = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        context,
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, pProgram);

    retVal = clBuildProgram(
        pProgram,
        1,
        &device,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Create and submit first instance of "CopyBuffer" kernel
    int prevCount11 = KernelCreateCallbackCount;
    kernel1 = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    EXPECT_NE(nullptr, kernel1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount11 + 1, KernelCreateCallbackCount);

    MultiDeviceKernel *pMultiDeviceKernel1 = static_cast<MultiDeviceKernel *>(kernel1);
    Kernel *pKernel1 = pMultiDeviceKernel1->getKernel(rootDeviceIndex);
    const KernelInfo &kInfo1 = pKernel1->getKernelInfo();
    uint64_t gtpinKernelId1 = pKernel1->getKernelId();
    EXPECT_EQ(kInfo1.shaderHashCode, gtpinKernelId1);

    constexpr size_t n = 256;
    auto buff10 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);
    auto buff11 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);

    retVal = clSetKernelArg(pMultiDeviceKernel1, 0, sizeof(cl_mem), &buff10);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pMultiDeviceKernel1, 1, sizeof(cl_mem), &buff11);
    EXPECT_EQ(CL_SUCCESS, retVal);

    int prevCount12 = KernelSubmitCallbackCount;
    int prevCount13 = CommandBufferCreateCallbackCount;
    int prevCount14 = CommandBufferCompleteCallbackCount;
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {n, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    retVal = clEnqueueNDRangeKernel(cmdQ, pMultiDeviceKernel1, workDim, globalWorkOffset, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount12 + 1, KernelSubmitCallbackCount);
    EXPECT_EQ(prevCount13 + 1, CommandBufferCreateCallbackCount);

    // Create and submit second instance of "CopyBuffer" kernel
    int prevCount21 = KernelCreateCallbackCount;
    kernel2 = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    EXPECT_NE(nullptr, kernel2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    // Verify that GT-Pin Kernel Create callback is not called multiple times for the same kernel
    EXPECT_EQ(prevCount21, KernelCreateCallbackCount);

    MultiDeviceKernel *pMultiDeviceKernel2 = static_cast<MultiDeviceKernel *>(kernel2);
    Kernel *pKernel2 = pMultiDeviceKernel2->getKernel(rootDeviceIndex);
    const KernelInfo &kInfo2 = pKernel2->getKernelInfo();
    uint64_t gtpinKernelId2 = pKernel2->getKernelId();
    EXPECT_EQ(kInfo2.shaderHashCode, gtpinKernelId2);

    auto buff20 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);
    auto buff21 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);

    retVal = clSetKernelArg(pMultiDeviceKernel2, 0, sizeof(cl_mem), &buff20);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pMultiDeviceKernel2, 1, sizeof(cl_mem), &buff21);
    EXPECT_EQ(CL_SUCCESS, retVal);

    int prevCount22 = KernelSubmitCallbackCount;
    int prevCount23 = CommandBufferCreateCallbackCount;
    int prevCount24 = CommandBufferCompleteCallbackCount;
    retVal = clEnqueueNDRangeKernel(cmdQ, pMultiDeviceKernel2, workDim, globalWorkOffset, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount22 + 1, KernelSubmitCallbackCount);
    EXPECT_EQ(prevCount23 + 1, CommandBufferCreateCallbackCount);

    retVal = clFinish(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount14 + 2, CommandBufferCompleteCallbackCount);
    EXPECT_EQ(prevCount24 + 2, CommandBufferCompleteCallbackCount);

    // Cleanup
    retVal = clReleaseKernel(kernel1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseKernel(kernel2);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pSource.reset();

    retVal = clReleaseMemObject(buff10);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buff11);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buff20);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buff21);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenKernelINTELIsExecutedThenGTPinCallbacksAreCalled) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_kernel kernel1 = nullptr;
    cl_kernel kernel2 = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        context,
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, pProgram);

    retVal = clBuildProgram(
        pProgram,
        1,
        &device,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Create and submit first instance of "CopyBuffer" kernel
    int prevCount11 = KernelCreateCallbackCount;
    kernel1 = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    EXPECT_NE(nullptr, kernel1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount11 + 1, KernelCreateCallbackCount);

    MultiDeviceKernel *pMultiDeviceKernel1 = static_cast<MultiDeviceKernel *>(kernel1);
    Kernel *pKernel1 = pMultiDeviceKernel1->getKernel(rootDeviceIndex);
    const KernelInfo &kInfo1 = pKernel1->getKernelInfo();
    uint64_t gtpinKernelId1 = pKernel1->getKernelId();
    EXPECT_EQ(kInfo1.shaderHashCode, gtpinKernelId1);

    cl_uint workDim = 1;
    size_t localWorkSize[3] = {1, 1, 1};
    CommandQueue *commandQueue = nullptr;
    withCastToInternal(cmdQ, &commandQueue);
    size_t n = 100;
    auto buff10 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);
    auto buff11 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);

    retVal = clSetKernelArg(pMultiDeviceKernel1, 0, sizeof(cl_mem), &buff10);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pMultiDeviceKernel1, 1, sizeof(cl_mem), &buff11);
    EXPECT_EQ(CL_SUCCESS, retVal);

    int prevCount12 = KernelSubmitCallbackCount;
    int prevCount13 = CommandBufferCreateCallbackCount;
    int prevCount14 = CommandBufferCompleteCallbackCount;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t workgroupCount[3] = {n, 1, 1};
    retVal = clEnqueueNDCountKernelINTEL(cmdQ, pMultiDeviceKernel1, workDim, globalWorkOffset, workgroupCount, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount12 + 1, KernelSubmitCallbackCount);
    EXPECT_EQ(prevCount13 + 1, CommandBufferCreateCallbackCount);

    // Create and submit second instance of "CopyBuffer" kernel
    int prevCount21 = KernelCreateCallbackCount;
    kernel2 = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    EXPECT_NE(nullptr, kernel2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    // Verify that GT-Pin Kernel Create callback is not called multiple times for the same kernel
    EXPECT_EQ(prevCount21, KernelCreateCallbackCount);

    MultiDeviceKernel *pMultiDeviceKernel2 = static_cast<MultiDeviceKernel *>(kernel2);
    Kernel *pKernel2 = pMultiDeviceKernel2->getKernel(rootDeviceIndex);
    const KernelInfo &kInfo2 = pKernel2->getKernelInfo();
    uint64_t gtpinKernelId2 = pKernel2->getKernelId();
    EXPECT_EQ(kInfo2.shaderHashCode, gtpinKernelId2);

    auto buff20 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);
    auto buff21 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);

    retVal = clSetKernelArg(pMultiDeviceKernel2, 0, sizeof(cl_mem), &buff20);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pMultiDeviceKernel2, 1, sizeof(cl_mem), &buff21);
    EXPECT_EQ(CL_SUCCESS, retVal);

    int prevCount22 = KernelSubmitCallbackCount;
    int prevCount23 = CommandBufferCreateCallbackCount;
    int prevCount24 = CommandBufferCompleteCallbackCount;
    retVal = clEnqueueNDCountKernelINTEL(cmdQ, pMultiDeviceKernel2, workDim, globalWorkOffset, workgroupCount, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount22 + 1, KernelSubmitCallbackCount);
    EXPECT_EQ(prevCount23 + 1, CommandBufferCreateCallbackCount);

    retVal = clFinish(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount14 + 2, CommandBufferCompleteCallbackCount);
    EXPECT_EQ(prevCount24 + 2, CommandBufferCompleteCallbackCount);

    // Cleanup
    retVal = clReleaseKernel(kernel1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseKernel(kernel2);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pSource.reset();

    retVal = clReleaseMemObject(buff10);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buff11);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buff20);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buff21);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenKernelWithoutSSHIsUsedThenKernelCreateCallbacksIsNotCalled) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_device_id device = (cl_device_id)pDevice;
    cl_context context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);
    auto pContext = castToObject<Context>(context);
    auto rootDeviceIndex = pDevice->getRootDeviceIndex();

    char binary[1024] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t binSize = 10;
    MockProgram *pProgram = Program::createBuiltInFromGenBinary<MockProgram>(pContext, pContext->getDevices(), &binary[0], binSize, &retVal);
    ASSERT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    PatchTokensTestData::ValidProgramWithKernel programTokens;

    pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy(reinterpret_cast<char *>(programTokens.storage.data()), programTokens.storage.size());
    pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = programTokens.storage.size();
    retVal = pProgram->processGenBinary(*pContext->getDevice(0));
    EXPECT_EQ(CL_SUCCESS, retVal);

    int prevCount = KernelCreateCallbackCount;
    cl_kernel kernel = clCreateKernel(pProgram, std::string(programTokens.kernels[0].name.begin(), programTokens.kernels[0].name.size()).c_str(), &retVal);
    EXPECT_NE(nullptr, kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount, KernelCreateCallbackCount);

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenKernelWithoutSSHIsUsedThenGTPinSubmitKernelCallbackIsNotCalled) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_kernel kernel = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        context,
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, pProgram);

    retVal = clBuildProgram(
        pProgram,
        1,
        &device,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    int prevCount1 = KernelCreateCallbackCount;
    kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    EXPECT_NE(nullptr, kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount1 + 1, KernelCreateCallbackCount);

    MultiDeviceKernel *pMultiDeviceKernel = static_cast<MultiDeviceKernel *>(kernel);
    Kernel *pKernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);
    const KernelInfo &kInfo = pKernel->getKernelInfo();
    uint64_t gtpinKernelId = pKernel->getKernelId();
    EXPECT_EQ(kInfo.shaderHashCode, gtpinKernelId);

    constexpr size_t n = 256;
    auto buff0 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);
    auto buff1 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(cl_mem), &buff0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pMultiDeviceKernel, 1, sizeof(cl_mem), &buff1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Verify that when SSH is removed then during kernel execution
    // GT-Pin Kernel Submit, Command Buffer Create and Command Buffer Complete callbacks are not called.
    pKernel->resizeSurfaceStateHeap(nullptr, 0, 0, 0);

    int prevCount2 = KernelSubmitCallbackCount;
    int prevCount3 = CommandBufferCreateCallbackCount;
    int prevCount4 = CommandBufferCompleteCallbackCount;
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {n, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    retVal = clEnqueueNDRangeKernel(cmdQ, pMultiDeviceKernel, workDim, globalWorkOffset, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount2, KernelSubmitCallbackCount);
    EXPECT_EQ(prevCount3, CommandBufferCreateCallbackCount);

    retVal = clFinish(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount4, CommandBufferCompleteCallbackCount);

    // Cleanup
    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pSource.reset();

    retVal = clReleaseMemObject(buff0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buff1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenBlockedKernelWithoutSSHIsUsedThenGTPinSubmitKernelCallbackIsNotCalled) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_kernel kernel = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        context,
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, pProgram);

    retVal = clBuildProgram(
        pProgram,
        1,
        &device,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    int prevCount1 = KernelCreateCallbackCount;
    kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    EXPECT_NE(nullptr, kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount1 + 1, KernelCreateCallbackCount);

    MultiDeviceKernel *pMultiDeviceKernel = static_cast<MultiDeviceKernel *>(kernel);
    Kernel *pKernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);
    const KernelInfo &kInfo = pKernel->getKernelInfo();
    uint64_t gtpinKernelId = pKernel->getKernelId();
    EXPECT_EQ(kInfo.shaderHashCode, gtpinKernelId);

    constexpr size_t n = 256;
    auto buff0 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);
    auto buff1 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);

    retVal = clSetKernelArg(pMultiDeviceKernel, 0, sizeof(cl_mem), &buff0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pMultiDeviceKernel, 1, sizeof(cl_mem), &buff1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Verify that when SSH is removed then during kernel execution
    // GT-Pin Kernel Submit, Command Buffer Create and Command Buffer Complete callbacks are not called.
    pKernel->resizeSurfaceStateHeap(nullptr, 0, 0, 0);

    cl_event userEvent = clCreateUserEvent(context, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    int prevCount2 = KernelSubmitCallbackCount;
    int prevCount3 = CommandBufferCreateCallbackCount;
    int prevCount4 = CommandBufferCompleteCallbackCount;
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {n, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    retVal = clEnqueueNDRangeKernel(cmdQ, pMultiDeviceKernel, workDim, globalWorkOffset, globalWorkSize, localWorkSize, 1, &userEvent, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount2, KernelSubmitCallbackCount);
    EXPECT_EQ(prevCount3, CommandBufferCreateCallbackCount);

    retVal = clSetUserEventStatus(userEvent, CL_COMPLETE);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clFinish(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount4, CommandBufferCompleteCallbackCount);

    // Cleanup
    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pSource.reset();

    retVal = clReleaseMemObject(buff0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buff1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenTheSameKerneIsExecutedTwiceThenGTPinCreateKernelCallbackIsCalledOnce) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_kernel kernel1 = nullptr;
    cl_kernel kernel2 = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        context,
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, pProgram);

    retVal = clBuildProgram(
        pProgram,
        1,
        &device,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Kernel "CopyBuffer" - called for the first time
    int prevCount11 = KernelCreateCallbackCount;
    kernel1 = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    EXPECT_NE(nullptr, kernel1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount11 + 1, KernelCreateCallbackCount);

    MultiDeviceKernel *pMultiDeviceKernel1 = static_cast<MultiDeviceKernel *>(kernel1);
    Kernel *pKernel1 = pMultiDeviceKernel1->getKernel(rootDeviceIndex);
    const KernelInfo &kInfo1 = pKernel1->getKernelInfo();
    uint64_t gtpinKernelId1 = pKernel1->getKernelId();
    EXPECT_EQ(kInfo1.shaderHashCode, gtpinKernelId1);

    constexpr size_t n = 256;
    auto buff10 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);
    auto buff11 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);

    retVal = clSetKernelArg(pMultiDeviceKernel1, 0, sizeof(cl_mem), &buff10);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pMultiDeviceKernel1, 1, sizeof(cl_mem), &buff11);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_event userEvent = clCreateUserEvent(context, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    int prevCount12 = KernelSubmitCallbackCount;
    int prevCount13 = CommandBufferCreateCallbackCount;
    int prevCount14 = CommandBufferCompleteCallbackCount;
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {n, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    retVal = clEnqueueNDRangeKernel(cmdQ, pMultiDeviceKernel1, workDim, globalWorkOffset, globalWorkSize, localWorkSize, 1, &userEvent, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount12 + 1, KernelSubmitCallbackCount);
    EXPECT_EQ(prevCount13 + 1, CommandBufferCreateCallbackCount);
    EXPECT_EQ(prevCount14, CommandBufferCompleteCallbackCount);

    // The same kernel "CopyBuffer" - called second time
    int prevCount21 = KernelCreateCallbackCount;
    kernel2 = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    EXPECT_NE(nullptr, kernel2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    // Verify that Kernel Create callback was not called now
    EXPECT_EQ(prevCount21, KernelCreateCallbackCount);

    MultiDeviceKernel *pMultiDeviceKernel2 = static_cast<MultiDeviceKernel *>(kernel2);
    Kernel *pKernel2 = pMultiDeviceKernel2->getKernel(rootDeviceIndex);
    const KernelInfo &kInfo2 = pKernel2->getKernelInfo();
    uint64_t gtpinKernelId2 = pKernel2->getKernelId();
    EXPECT_EQ(kInfo2.shaderHashCode, gtpinKernelId2);

    auto buff20 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);
    auto buff21 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);

    retVal = clSetKernelArg(pMultiDeviceKernel2, 0, sizeof(cl_mem), &buff20);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pMultiDeviceKernel2, 1, sizeof(cl_mem), &buff21);
    EXPECT_EQ(CL_SUCCESS, retVal);

    int prevCount22 = KernelSubmitCallbackCount;
    int prevCount23 = CommandBufferCreateCallbackCount;
    int prevCount24 = CommandBufferCompleteCallbackCount;
    EXPECT_EQ(prevCount14, prevCount24);
    retVal = clEnqueueNDRangeKernel(cmdQ, pMultiDeviceKernel2, workDim, globalWorkOffset, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount22 + 1, KernelSubmitCallbackCount);
    EXPECT_EQ(prevCount23 + 1, CommandBufferCreateCallbackCount);
    EXPECT_EQ(prevCount14, CommandBufferCompleteCallbackCount);
    EXPECT_EQ(prevCount24, CommandBufferCompleteCallbackCount);
    EXPECT_EQ(prevCount14, prevCount24);

    clSetUserEventStatus(userEvent, CL_COMPLETE);

    retVal = clFinish(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);
    // Verify that both kernel instances were completed
    EXPECT_EQ(prevCount14 + 2, CommandBufferCompleteCallbackCount);
    EXPECT_EQ(prevCount24 + 2, CommandBufferCompleteCallbackCount);

    // Cleanup
    retVal = clReleaseKernel(kernel1);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseKernel(kernel2);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pSource.reset();

    retVal = clReleaseMemObject(buff10);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buff11);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buff20);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(buff21);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseEvent(userEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenMultipleKernelSubmissionsWhenOneOfGtpinSurfacesIsNullThenOnlyNonNullSurfacesAreMadeResident) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_kernel kernel1 = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        context,
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, pProgram);

    retVal = clBuildProgram(
        pProgram,
        1,
        &device,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    kernel1 = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    EXPECT_NE(nullptr, kernel1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    MultiDeviceKernel *pMultiDeviceKernel1 = static_cast<MultiDeviceKernel *>(kernel1);
    Kernel *pKernel1 = pMultiDeviceKernel1->getKernel(rootDeviceIndex);
    returnNullResource = true;

    auto pCmdQueue = castToObject<CommandQueue>(cmdQ);

    gtpinNotifyKernelSubmit(pMultiDeviceKernel1, pCmdQueue);
    EXPECT_EQ(nullptr, kernelExecQueue[0].gtpinResource);

    CommandStreamReceiver &csr = pCmdQueue->getGpgpuCommandStreamReceiver();
    gtpinNotifyMakeResident(pKernel1, &csr);
    EXPECT_FALSE(kernelExecQueue[0].isResourceResident);

    std::vector<Surface *> residencyVector;
    gtpinNotifyUpdateResidencyList(pKernel1, &residencyVector);
    EXPECT_EQ(0u, residencyVector.size());

    returnNullResource = false;

    gtpinNotifyKernelSubmit(pMultiDeviceKernel1, pCmdQueue);
    EXPECT_NE(nullptr, kernelExecQueue[1].gtpinResource);
    gtpinNotifyMakeResident(pKernel1, &csr);
    EXPECT_TRUE(kernelExecQueue[1].isResourceResident);
    cl_mem gtpinBuffer1 = kernelExecQueue[1].gtpinResource;

    gtpinNotifyKernelSubmit(pMultiDeviceKernel1, pCmdQueue);
    EXPECT_NE(nullptr, kernelExecQueue[2].gtpinResource);
    gtpinNotifyUpdateResidencyList(pKernel1, &residencyVector);
    EXPECT_EQ(1u, residencyVector.size());
    EXPECT_TRUE(kernelExecQueue[2].isResourceResident);
    EXPECT_FALSE(kernelExecQueue[0].isResourceResident);

    GeneralSurface *pSurf = static_cast<GeneralSurface *>(residencyVector[0]);
    delete pSurf;
    residencyVector.clear();

    cl_mem gtpinBuffer2 = kernelExecQueue[2].gtpinResource;

    gtpinUnmapBuffer(reinterpret_cast<context_handle_t>(context), reinterpret_cast<resource_handle_t>(gtpinBuffer1));
    gtpinFreeBuffer(reinterpret_cast<context_handle_t>(context), reinterpret_cast<resource_handle_t>(gtpinBuffer1));

    gtpinUnmapBuffer(reinterpret_cast<context_handle_t>(context), reinterpret_cast<resource_handle_t>(gtpinBuffer2));
    gtpinFreeBuffer(reinterpret_cast<context_handle_t>(context), reinterpret_cast<resource_handle_t>(gtpinBuffer2));

    retVal = clFinish(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Cleanup
    returnNullResource = false;
    kernelResources.clear();
    retVal = clReleaseKernel(kernel1);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pSource.reset();

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenKernelIsCreatedThenAllKernelSubmitRelatedNotificationsAreCalled) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    kernelExecQueue.clear();

    cl_kernel kernel = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        context,
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, pProgram);

    retVal = clBuildProgram(
        pProgram,
        1,
        &device,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Create kernel
    int prevCount1 = KernelCreateCallbackCount;
    kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    ASSERT_NE(nullptr, kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount1 + 1, KernelCreateCallbackCount);

    // Simulate that created kernel was sent for execution
    auto pMultiDeviceKernel = castToObject<MultiDeviceKernel>(kernel);
    auto pKernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);
    auto pCmdQueue = castToObject<CommandQueue>(cmdQ);
    ASSERT_NE(nullptr, pKernel);
    EXPECT_EQ(0u, kernelExecQueue.size());
    EXPECT_EQ(0u, kernelResources.size());
    int prevCount2 = CommandBufferCreateCallbackCount;
    int prevCount3 = KernelSubmitCallbackCount;
    gtpinNotifyKernelSubmit(kernel, pCmdQueue);
    EXPECT_EQ(prevCount2 + 1, CommandBufferCreateCallbackCount);
    EXPECT_EQ(prevCount3 + 1, KernelSubmitCallbackCount);
    EXPECT_EQ(1u, kernelExecQueue.size());
    EXPECT_EQ(1u, kernelResources.size());
    EXPECT_EQ(pKernel, kernelExecQueue[0].pKernel);
    EXPECT_EQ(kernelResources[0], (resource_handle_t)kernelExecQueue[0].gtpinResource);
    EXPECT_EQ(pCmdQueue, kernelExecQueue[0].pCommandQueue);
    EXPECT_FALSE(kernelExecQueue[0].isTaskCountValid);
    EXPECT_FALSE(kernelExecQueue[0].isResourceResident);

    // Verify that if kernel unknown to GT-Pin is about to be flushed
    // then its residency vector does not obtain GT-Pin resource
    std::vector<Surface *> residencyVector;
    EXPECT_EQ(0u, residencyVector.size());
    gtpinNotifyUpdateResidencyList(nullptr, &residencyVector);
    EXPECT_EQ(0u, residencyVector.size());
    EXPECT_FALSE(kernelExecQueue[0].isResourceResident);

    // Verify that if kernel known to GT-Pin is about to be flushed
    // then its residency vector obtains GT-Pin resource
    EXPECT_EQ(0u, residencyVector.size());
    gtpinNotifyUpdateResidencyList(pKernel, &residencyVector);
    EXPECT_EQ(1u, residencyVector.size());
    GeneralSurface *pSurf = (GeneralSurface *)residencyVector[0];
    delete pSurf;
    residencyVector.clear();
    EXPECT_TRUE(kernelExecQueue[0].isResourceResident);
    kernelExecQueue[0].isResourceResident = false;

    // Create second kernel ...
    cl_kernel kernel2 = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    ASSERT_NE(nullptr, kernel2);
    EXPECT_EQ(CL_SUCCESS, retVal);
    // ... and simulate that it was sent for execution
    auto pMultiDeviceKernel2 = castToObject<MultiDeviceKernel>(kernel2);
    auto pKernel2 = pMultiDeviceKernel2->getKernel(rootDeviceIndex);
    ASSERT_NE(nullptr, pKernel2);
    EXPECT_EQ(1u, kernelExecQueue.size());
    EXPECT_EQ(1u, kernelResources.size());
    int prevCount22 = CommandBufferCreateCallbackCount;
    int prevCount23 = KernelSubmitCallbackCount;
    gtpinNotifyKernelSubmit(kernel2, pCmdQueue);
    EXPECT_EQ(prevCount22 + 1, CommandBufferCreateCallbackCount);
    EXPECT_EQ(prevCount23 + 1, KernelSubmitCallbackCount);
    EXPECT_EQ(2u, kernelExecQueue.size());
    EXPECT_EQ(2u, kernelResources.size());
    EXPECT_EQ(pKernel2, kernelExecQueue[1].pKernel);
    EXPECT_EQ(kernelResources[1], (resource_handle_t)kernelExecQueue[1].gtpinResource);
    EXPECT_EQ(pCmdQueue, kernelExecQueue[1].pCommandQueue);
    EXPECT_FALSE(kernelExecQueue[1].isTaskCountValid);
    EXPECT_FALSE(kernelExecQueue[1].isResourceResident);

    // Verify that correct GT-Pin resource is made resident
    cl_mem gtpinBuffer0 = kernelExecQueue[0].gtpinResource;
    auto pBuffer0 = castToObject<Buffer>(gtpinBuffer0);
    GraphicsAllocation *pGfxAlloc0 = pBuffer0->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    cl_mem gtpinBuffer1 = kernelExecQueue[1].gtpinResource;
    auto pBuffer1 = castToObject<Buffer>(gtpinBuffer1);
    GraphicsAllocation *pGfxAlloc1 = pBuffer1->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    CommandStreamReceiver &csr = pCmdQueue->getGpgpuCommandStreamReceiver();
    EXPECT_FALSE(pGfxAlloc0->isResident(csr.getOsContext().getContextId()));
    EXPECT_FALSE(pGfxAlloc1->isResident(csr.getOsContext().getContextId()));
    gtpinNotifyMakeResident(pKernel, &csr);
    EXPECT_TRUE(pGfxAlloc0->isResident(csr.getOsContext().getContextId()));
    EXPECT_FALSE(pGfxAlloc1->isResident(csr.getOsContext().getContextId()));

    // Cancel information about second submitted kernel
    kernelExecQueue.pop_back();
    EXPECT_EQ(1u, kernelExecQueue.size());
    kernelResources.pop_back();
    EXPECT_EQ(1u, kernelResources.size());
    gtpinUnmapBuffer((context_handle_t)context, (resource_handle_t)gtpinBuffer1);
    gtpinFreeBuffer((context_handle_t)context, (resource_handle_t)gtpinBuffer1);
    retVal = clReleaseKernel(kernel2);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Verify that if flush occurs on another queue then our kernel is not flushed to CSR
    uint32_t taskCount = 11;
    gtpinNotifyPreFlushTask(nullptr);
    EXPECT_EQ(1u, kernelExecQueue.size());
    EXPECT_FALSE(kernelExecQueue[0].isTaskCountValid);
    gtpinNotifyFlushTask(taskCount);
    EXPECT_EQ(1u, kernelExecQueue.size());
    EXPECT_FALSE(kernelExecQueue[0].isTaskCountValid);

    // Verify that if flush occurs on current queue then our kernel is flushed to CSR
    gtpinNotifyPreFlushTask(pCmdQueue);
    EXPECT_EQ(1u, kernelExecQueue.size());
    EXPECT_FALSE(kernelExecQueue[0].isTaskCountValid);
    gtpinNotifyFlushTask(taskCount);
    EXPECT_EQ(1u, kernelExecQueue.size());
    EXPECT_TRUE(kernelExecQueue[0].isTaskCountValid);
    EXPECT_EQ(taskCount, kernelExecQueue[0].taskCount);

    // Verify that if previous task was completed then it does not affect our kernel
    uint32_t taskCompleted = taskCount - 1;
    int prevCount4 = CommandBufferCompleteCallbackCount;
    gtpinNotifyTaskCompletion(taskCompleted);
    EXPECT_EQ(1u, kernelExecQueue.size());
    EXPECT_EQ(1u, kernelResources.size());
    EXPECT_EQ(prevCount4, CommandBufferCompleteCallbackCount);

    // Verify that if current task was completed then it is our kernel
    gtpinNotifyTaskCompletion(taskCompleted + 1);
    EXPECT_EQ(0u, kernelExecQueue.size());
    EXPECT_EQ(0u, kernelResources.size());
    EXPECT_EQ(prevCount4 + 1, CommandBufferCompleteCallbackCount);

    // Cleanup
    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pSource.reset();

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenOneKernelIsSubmittedSeveralTimesThenCorrectBuffersAreMadeResident) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    kernelExecQueue.clear();

    cl_kernel kernel = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        context,
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, pProgram);

    retVal = clBuildProgram(
        pProgram,
        1,
        &device,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Create kernel
    int prevCount1 = KernelCreateCallbackCount;
    kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    ASSERT_NE(nullptr, kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount1 + 1, KernelCreateCallbackCount);

    // Simulate that created kernel was sent for execution two times in a row
    auto pMultiDeviceKernel = castToObject<MultiDeviceKernel>(kernel);
    auto pKernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);
    auto pCmdQueue = castToObject<CommandQueue>(cmdQ);
    ASSERT_NE(nullptr, pKernel);
    EXPECT_EQ(0u, kernelExecQueue.size());
    EXPECT_EQ(0u, kernelResources.size());
    int prevCount2 = CommandBufferCreateCallbackCount;
    int prevCount3 = KernelSubmitCallbackCount;
    // First kernel submission
    gtpinNotifyKernelSubmit(kernel, pCmdQueue);
    EXPECT_EQ(prevCount2 + 1, CommandBufferCreateCallbackCount);
    EXPECT_EQ(prevCount3 + 1, KernelSubmitCallbackCount);
    EXPECT_EQ(1u, kernelExecQueue.size());
    EXPECT_EQ(1u, kernelResources.size());
    EXPECT_EQ(pKernel, kernelExecQueue[0].pKernel);
    EXPECT_EQ(kernelResources[0], (resource_handle_t)kernelExecQueue[0].gtpinResource);
    EXPECT_EQ(pCmdQueue, kernelExecQueue[0].pCommandQueue);
    EXPECT_FALSE(kernelExecQueue[0].isTaskCountValid);
    EXPECT_FALSE(kernelExecQueue[0].isResourceResident);
    // Second kernel submission
    gtpinNotifyKernelSubmit(kernel, pCmdQueue);
    EXPECT_EQ(prevCount2 + 2, CommandBufferCreateCallbackCount);
    EXPECT_EQ(prevCount3 + 2, KernelSubmitCallbackCount);
    EXPECT_EQ(2u, kernelExecQueue.size());
    EXPECT_EQ(2u, kernelResources.size());
    EXPECT_EQ(pKernel, kernelExecQueue[0].pKernel);
    EXPECT_EQ(kernelResources[0], (resource_handle_t)kernelExecQueue[0].gtpinResource);
    EXPECT_EQ(pCmdQueue, kernelExecQueue[0].pCommandQueue);
    EXPECT_FALSE(kernelExecQueue[0].isTaskCountValid);
    EXPECT_FALSE(kernelExecQueue[0].isResourceResident);
    EXPECT_EQ(pKernel, kernelExecQueue[1].pKernel);
    EXPECT_EQ(kernelResources[1], (resource_handle_t)kernelExecQueue[1].gtpinResource);
    EXPECT_EQ(pCmdQueue, kernelExecQueue[1].pCommandQueue);
    EXPECT_FALSE(kernelExecQueue[1].isTaskCountValid);
    EXPECT_FALSE(kernelExecQueue[1].isResourceResident);

    // Verify that correct GT-Pin resource is made resident.
    // This simulates enqueuing non-blocked kernels
    cl_mem gtpinBuffer0 = kernelExecQueue[0].gtpinResource;
    auto pBuffer0 = castToObject<Buffer>(gtpinBuffer0);
    GraphicsAllocation *pGfxAlloc0 = pBuffer0->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    cl_mem gtpinBuffer1 = kernelExecQueue[1].gtpinResource;
    auto pBuffer1 = castToObject<Buffer>(gtpinBuffer1);
    GraphicsAllocation *pGfxAlloc1 = pBuffer1->getGraphicsAllocation(pDevice->getRootDeviceIndex());
    CommandStreamReceiver &csr = pCmdQueue->getGpgpuCommandStreamReceiver();
    // Make resident resource of first submitted kernel
    EXPECT_FALSE(pGfxAlloc0->isResident(csr.getOsContext().getContextId()));
    EXPECT_FALSE(pGfxAlloc1->isResident(csr.getOsContext().getContextId()));
    gtpinNotifyMakeResident(pKernel, &csr);
    EXPECT_TRUE(pGfxAlloc0->isResident(csr.getOsContext().getContextId()));
    EXPECT_FALSE(pGfxAlloc1->isResident(csr.getOsContext().getContextId()));
    // Make resident resource of second submitted kernel
    gtpinNotifyMakeResident(pKernel, &csr);
    EXPECT_TRUE(pGfxAlloc0->isResident(csr.getOsContext().getContextId()));
    EXPECT_TRUE(pGfxAlloc1->isResident(csr.getOsContext().getContextId()));

    // Verify that correct GT-Pin resource is added to residency list.
    // This simulates enqueuing blocked kernels
    kernelExecQueue[0].isResourceResident = false;
    kernelExecQueue[1].isResourceResident = false;
    pGfxAlloc0->releaseResidencyInOsContext(csr.getOsContext().getContextId());
    pGfxAlloc1->releaseResidencyInOsContext(csr.getOsContext().getContextId());
    EXPECT_FALSE(pGfxAlloc0->isResident(csr.getOsContext().getContextId()));
    EXPECT_FALSE(pGfxAlloc1->isResident(csr.getOsContext().getContextId()));
    std::vector<Surface *> residencyVector;
    EXPECT_EQ(0u, residencyVector.size());
    // Add to residency list resource of first submitted kernel
    gtpinNotifyUpdateResidencyList(pKernel, &residencyVector);
    EXPECT_EQ(1u, residencyVector.size());
    // Make resident first resource on residency list
    GeneralSurface *pSurf1 = (GeneralSurface *)residencyVector[0];
    pSurf1->makeResident(csr);
    EXPECT_TRUE(pGfxAlloc0->isResident(csr.getOsContext().getContextId()));
    EXPECT_FALSE(pGfxAlloc1->isResident(csr.getOsContext().getContextId()));
    // Add to residency list resource of second submitted kernel
    gtpinNotifyUpdateResidencyList(pKernel, &residencyVector);
    EXPECT_EQ(2u, residencyVector.size());
    // Make resident second resource on residency list
    GeneralSurface *pSurf2 = (GeneralSurface *)residencyVector[1];
    pSurf2->makeResident(csr);
    EXPECT_TRUE(pGfxAlloc0->isResident(csr.getOsContext().getContextId()));
    EXPECT_TRUE(pGfxAlloc1->isResident(csr.getOsContext().getContextId()));

    // Cleanup
    delete pSurf1;
    delete pSurf2;
    residencyVector.clear();

    kernelExecQueue.pop_back();
    EXPECT_EQ(1u, kernelExecQueue.size());
    kernelResources.pop_back();
    EXPECT_EQ(1u, kernelResources.size());
    gtpinUnmapBuffer((context_handle_t)context, (resource_handle_t)gtpinBuffer1);
    gtpinFreeBuffer((context_handle_t)context, (resource_handle_t)gtpinBuffer1);

    kernelExecQueue.pop_back();
    EXPECT_EQ(0u, kernelExecQueue.size());
    kernelResources.pop_back();
    EXPECT_EQ(0u, kernelResources.size());
    gtpinUnmapBuffer((context_handle_t)context, (resource_handle_t)gtpinBuffer0);
    gtpinFreeBuffer((context_handle_t)context, (resource_handle_t)gtpinBuffer0);

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pSource.reset();

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenLowMemoryConditionOccursThenKernelCreationFails) {

    InjectedFunction allocBufferFunc = [this](size_t failureIndex) {
        cl_device_id device = (cl_device_id)pDevice;
        cl_context context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, context);
        auto pContext = castToObject<Context>(context);

        char binary[1024] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
        size_t binSize = 10;
        MockProgram *pProgram = Program::createBuiltInFromGenBinary<MockProgram>(pContext, pContext->getDevices(), &binary[0], binSize, &retVal);
        ASSERT_NE(nullptr, pProgram);
        EXPECT_EQ(CL_SUCCESS, retVal);

        PatchTokensTestData::ValidProgramWithKernel programTokens;

        auto rootDeviceIndex = pDevice->getRootDeviceIndex();

        pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinary = makeCopy(programTokens.storage.data(), programTokens.storage.size());
        pProgram->buildInfos[rootDeviceIndex].unpackedDeviceBinarySize = programTokens.storage.size();
        retVal = pProgram->processGenBinary(*pDevice);
        if (retVal == CL_OUT_OF_HOST_MEMORY) {
            auto nonFailingAlloc = MemoryManagement::nonfailingAllocation;
            EXPECT_NE(nonFailingAlloc, failureIndex);
        } else {
            EXPECT_EQ(CL_SUCCESS, retVal);
            // Create kernels from program
            cl_kernel kernels[2] = {0};
            cl_uint numCreatedKernels = 0;

            if (MemoryManagement::nonfailingAllocation != failureIndex) {
                memoryManager->failAllAllocationsInDevicePool = true;
            }
            retVal = clCreateKernelsInProgram(pProgram, 2, kernels, &numCreatedKernels);

            if (MemoryManagement::nonfailingAllocation != failureIndex) {
                if (retVal != CL_SUCCESS) {
                    EXPECT_EQ(nullptr, kernels[0]);
                    EXPECT_EQ(1u, numCreatedKernels);
                }
                clReleaseKernel(kernels[0]);
            } else {
                EXPECT_NE(nullptr, kernels[0]);
                EXPECT_EQ(1u, numCreatedKernels);
                clReleaseKernel(kernels[0]);
            }
        }

        clReleaseProgram(pProgram);
        clReleaseContext(context);
    };

    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_EQ(&NEO::gtpinCreateBuffer, driverServices.bufferAllocate);
    ASSERT_EQ(&NEO::gtpinFreeBuffer, driverServices.bufferDeallocate);
    EXPECT_EQ(&NEO::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&NEO::gtpinUnmapBuffer, driverServices.bufferUnMap);

    injectFailures(allocBufferFunc);
}

TEST_F(GTPinTests, givenKernelWithSSHThenVerifyThatSSHResizeWorksWell) {
    cl_kernel kernel = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    size_t sourceSize = 0;
    std::string testFile;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        context,
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, pProgram);

    retVal = clBuildProgram(
        pProgram,
        1,
        &device,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Create kernel
    kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    ASSERT_NE(nullptr, kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto pMultiDeviceKernel = castToObject<MultiDeviceKernel>(kernel);
    auto pKernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);
    ASSERT_NE(nullptr, pKernel);

    size_t numBTS1 = pKernel->getNumberOfBindingTableStates();
    EXPECT_LE(2u, numBTS1);
    size_t sizeSurfaceStates1 = pKernel->getSurfaceStateHeapSize();
    EXPECT_NE(0u, sizeSurfaceStates1);
    size_t offsetBTS1 = pKernel->getBindingTableOffset();
    EXPECT_NE(0u, offsetBTS1);

    GFXCORE_FAMILY genFamily = pDevice->getHardwareInfo().platform.eRenderCoreFamily;
    GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(genFamily);
    void *pSS1 = gtpinHelper.getSurfaceState(pKernel, 0);
    EXPECT_NE(nullptr, pSS1);

    // Enlarge SSH by one SURFACE STATE element
    bool surfaceAdded = gtpinHelper.addSurfaceState(pKernel);
    EXPECT_TRUE(surfaceAdded);

    size_t numBTS2 = pKernel->getNumberOfBindingTableStates();
    EXPECT_EQ(numBTS1 + 1, numBTS2);
    size_t sizeSurfaceStates2 = pKernel->getSurfaceStateHeapSize();
    EXPECT_GT(sizeSurfaceStates2, sizeSurfaceStates1);
    size_t offsetBTS2 = pKernel->getBindingTableOffset();
    EXPECT_GT(offsetBTS2, offsetBTS1);

    void *pSS2 = gtpinHelper.getSurfaceState(pKernel, 0);
    EXPECT_NE(pSS2, pSS1);

    pSS2 = gtpinHelper.getSurfaceState(pKernel, numBTS2);
    EXPECT_EQ(nullptr, pSS2);

    // Remove kernel's SSH
    pKernel->resizeSurfaceStateHeap(nullptr, 0, 0, 0);

    // Try to enlarge SSH once again, this time the operation must fail
    surfaceAdded = gtpinHelper.addSurfaceState(pKernel);
    EXPECT_FALSE(surfaceAdded);

    size_t numBTS3 = pKernel->getNumberOfBindingTableStates();
    EXPECT_EQ(0u, numBTS3);
    size_t sizeSurfaceStates3 = pKernel->getSurfaceStateHeapSize();
    EXPECT_EQ(0u, sizeSurfaceStates3);
    size_t offsetBTS3 = pKernel->getBindingTableOffset();
    EXPECT_EQ(0u, offsetBTS3);
    void *pSS3 = gtpinHelper.getSurfaceState(pKernel, 0);
    EXPECT_EQ(nullptr, pSS3);

    // Cleanup
    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenKernelThenVerifyThatKernelCodeSubstitutionWorksWell) {
    cl_kernel kernel = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    size_t sourceSize = 0;
    std::string testFile;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        context,
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, pProgram);

    retVal = clBuildProgram(
        pProgram,
        1,
        &device,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Create kernel
    kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    ASSERT_NE(nullptr, kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto pMultiDeviceKernel = castToObject<MultiDeviceKernel>(kernel);
    auto pKernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);
    ASSERT_NE(nullptr, pKernel);

    bool isKernelCodeSubstituted = pKernel->isKernelHeapSubstituted();
    EXPECT_FALSE(isKernelCodeSubstituted);

    // Substitute new kernel code
    constexpr size_t newCodeSize = 64;
    uint8_t newCode[newCodeSize] = {0x0, 0x1, 0x2, 0x3, 0x4};
    pKernel->substituteKernelHeap(&newCode[0], newCodeSize);

    // Verify that substitution went properly
    isKernelCodeSubstituted = pKernel->isKernelHeapSubstituted();
    EXPECT_TRUE(isKernelCodeSubstituted);
    uint8_t *pBin2 = reinterpret_cast<uint8_t *>(const_cast<void *>(pKernel->getKernelHeap()));
    EXPECT_EQ(pBin2, &newCode[0]);

    auto kernelIsa = pKernel->getKernelInfo().kernelAllocation->getUnderlyingBuffer();

    EXPECT_EQ(0, memcmp(kernelIsa, newCode, newCodeSize));

    // Cleanup
    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, WhenGettingGtPinHwHelperThenValidPointerIsReturned) {
    GFXCORE_FAMILY genFamily = pDevice->getHardwareInfo().platform.eRenderCoreFamily;
    GTPinHwHelper *pGTPinHelper = &GTPinHwHelper::get(genFamily);
    EXPECT_NE(nullptr, pGTPinHelper);
}

TEST(GTPinOfflineTests, givenGtPinInDisabledStateWhenCallbacksFromEnqueuePathAreCalledThenNothingHappens) {
    ASSERT_FALSE(gtpinIsGTPinInitialized());
    auto dummyKernel = reinterpret_cast<cl_kernel>(0x1000);
    auto dummyQueue = reinterpret_cast<void *>(0x1000);
    uint32_t dummyCompletedTask = 0u;

    //now call gtpin function with dummy data, this must not crash
    gtpinNotifyKernelSubmit(dummyKernel, dummyQueue);
    gtpinNotifyPreFlushTask(dummyQueue);
    gtpinNotifyTaskCompletion(dummyCompletedTask);
    gtpinNotifyFlushTask(dummyCompletedTask);
    EXPECT_FALSE(gtpinIsGTPinInitialized());
}
TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenOnKernelSubitIsCalledThenCorrectOffsetisSetInKernel) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    VariableBackup<bool> returnNullResourceBckp(&returnNullResource);
    VariableBackup<uint32_t> kernelOffsetBckp(&kernelOffset);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    char surfaceStateHeap[0x80];
    std::unique_ptr<MockContext> context(new MockContext(pDevice));

    EXPECT_EQ(CL_SUCCESS, retVal);
    auto pKernelInfo = std::make_unique<KernelInfo>();
    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

    auto pProgramm = std::make_unique<MockProgram>(context.get(), false, toClDeviceVector(*pDevice));
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(context.get(), pDevice, nullptr, false));
    std::unique_ptr<MultiDeviceKernel> pMultiDeviceKernel(MockMultiDeviceKernel::create<MockKernel>(pProgramm.get(), MockKernel::toKernelInfoContainer(*pKernelInfo, rootDeviceIndex)));
    auto pKernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(rootDeviceIndex));

    pKernel->setSshLocal(nullptr, sizeof(surfaceStateHeap));

    kernelOffset = 0x1234;
    EXPECT_NE(pKernel->getStartOffset(), kernelOffset);
    returnNullResource = true;
    cl_context ctxt = context.get();
    currContext = (gtpin::context_handle_t)ctxt;
    gtpinNotifyKernelSubmit(pMultiDeviceKernel.get(), cmdQ.get());
    EXPECT_EQ(pKernel->getStartOffset(), kernelOffset);

    EXPECT_EQ(CL_SUCCESS, retVal);

    kernelResources.clear();
}
TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenOnContextCreateIsCalledThenGtpinInitIsSet) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    auto context = std::make_unique<MockContext>();
    gtpinNotifyContextCreate(context.get());
    EXPECT_NE(gtpinGetIgcInit(), nullptr);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenOnKernelCreateIsCalledWithNullptrThenCallIsIgnored) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    auto prevCreateCount = KernelCreateCallbackCount;
    gtpinNotifyKernelCreate(nullptr);
    EXPECT_EQ(prevCreateCount, KernelCreateCallbackCount);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenKernelDoesNotHaveDebugDataThenPassNullPtrToOnKernelCreate) {
    static void *debugDataPtr = nullptr;
    static size_t debugDataSize = 0;
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = [](context_handle_t context, const instrument_params_in_t *paramsIn, instrument_params_out_t *paramsOut) {
        paramsOut->inst_kernel_binary = const_cast<uint8_t *>(paramsIn->orig_kernel_binary);
        paramsOut->inst_kernel_size = paramsIn->orig_kernel_size;
        paramsOut->kernel_id = paramsIn->igc_hash_id;
        debugDataPtr = const_cast<void *>(paramsIn->debug_data);
        debugDataSize = paramsIn->debug_data_size;
    };
    gtpinCallbacks.onKernelSubmit = [](command_buffer_handle_t cb, uint64_t kernelId, uint32_t *entryOffset, resource_handle_t *resource) {};
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    MockKernelWithInternals mockKernel(*pDevice);
    mockKernel.kernelInfo.kernelDescriptor.external.debugData.reset();
    mockKernel.kernelInfo.createKernelAllocation(pDevice->getDevice(), false);
    gtpinNotifyKernelCreate(static_cast<cl_kernel>(mockKernel.mockKernel->getMultiDeviceKernel()));
    EXPECT_EQ(debugDataPtr, nullptr);
    EXPECT_EQ(debugDataSize, 0u);
    pDevice->getMemoryManager()->freeGraphicsMemory(mockKernel.kernelInfo.kernelAllocation);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenKernelHasDebugDataThenCorrectDebugDataIsSet) {
    static void *debugDataPtr = nullptr;
    static size_t debugDataSize = 0;
    void *dummyDebugData = reinterpret_cast<void *>(0x123456);
    size_t dummyDebugDataSize = 0x2245;
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = [](context_handle_t context, const instrument_params_in_t *paramsIn, instrument_params_out_t *paramsOut) {
        paramsOut->inst_kernel_binary = const_cast<uint8_t *>(paramsIn->orig_kernel_binary);
        paramsOut->inst_kernel_size = paramsIn->orig_kernel_size;
        paramsOut->kernel_id = paramsIn->igc_hash_id;
        debugDataPtr = const_cast<void *>(paramsIn->debug_data);
        debugDataSize = paramsIn->debug_data_size;
    };
    gtpinCallbacks.onKernelSubmit = [](command_buffer_handle_t cb, uint64_t kernelId, uint32_t *entryOffset, resource_handle_t *resource) {};
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    MockKernelWithInternals mockKernel(*pDevice);
    mockKernel.kernelInfo.kernelDescriptor.external.debugData.reset(new DebugData());
    mockKernel.kernelInfo.debugData.vIsa = reinterpret_cast<char *>(dummyDebugData);
    mockKernel.kernelInfo.debugData.vIsaSize = static_cast<uint32_t>(dummyDebugDataSize);
    mockKernel.kernelInfo.createKernelAllocation(pDevice->getDevice(), false);
    gtpinNotifyKernelCreate(static_cast<cl_kernel>(mockKernel.mockKernel->getMultiDeviceKernel()));
    EXPECT_EQ(debugDataPtr, dummyDebugData);
    EXPECT_EQ(debugDataSize, dummyDebugDataSize);
    pDevice->getMemoryManager()->freeGraphicsMemory(mockKernel.kernelInfo.kernelAllocation);
}

HWTEST_F(GTPinTests, givenGtPinInitializedWhenSubmittingKernelCommandThenFlushedTaskCountIsNotified) {
    auto mockCmdQ = std::make_unique<MockCommandQueueHw<FamilyType>>(pContext, pDevice, nullptr);

    auto onKernelSubmitFnc = [](command_buffer_handle_t cb, uint64_t kernelId, uint32_t *entryOffset, resource_handle_t *resource) { return; };

    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmitFnc;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    IndirectHeap *ih1 = nullptr, *ih2 = nullptr, *ih3 = nullptr;
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::DYNAMIC_STATE, 128, ih1);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::INDIRECT_OBJECT, 128, ih2);
    mockCmdQ->allocateHeapMemory(IndirectHeap::Type::SURFACE_STATE, 128, ih3);

    PreemptionMode preemptionMode = pDevice->getPreemptionMode();
    auto cmdStream = new LinearStream(pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({pDevice->getRootDeviceIndex(), 128, AllocationType::COMMAND_BUFFER, pDevice->getDeviceBitfield()}));

    std::vector<Surface *> surfaces;
    auto kernelOperation = std::make_unique<KernelOperation>(cmdStream, *mockCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    MockKernelWithInternals kernel(*pDevice);
    kernelOperation->setHeaps(ih1, ih2, ih3);

    bool flushDC = false;
    bool slmUsed = false;
    bool ndRangeKernel = false;

    gtpinNotifyKernelSubmit(kernel.mockMultiDeviceKernel, mockCmdQ.get());

    std::unique_ptr<Command> command(new CommandComputeKernel(*mockCmdQ, kernelOperation, surfaces, flushDC, slmUsed, ndRangeKernel, nullptr, preemptionMode, kernel, 1));
    CompletionStamp stamp = command->submit(20, false);

    ASSERT_EQ(1u, kernelExecQueue.size());

    EXPECT_TRUE(kernelExecQueue[0].isTaskCountValid);
    EXPECT_EQ(kernelExecQueue[0].taskCount, stamp.taskCount);
}

class GTPinFixtureWithLocalMemory : public GTPinFixture {
  public:
    void setUp() {
        DebugManager.flags.EnableLocalMemory.set(true);
        DebugManager.flags.GTPinAllocateBufferInSharedMemory.set(true);
        GTPinFixture::setUpImpl();
    }
    void tearDown() {
        GTPinFixture::tearDown();
    }
    DebugManagerStateRestore restore;
};

using GTPinTestsWithLocalMemory = Test<GTPinFixtureWithLocalMemory>;

TEST_F(GTPinTestsWithLocalMemory, whenPlatformHasNoSvmSupportThenGtPinBufferCantBeAllocatedInSharedMemory) {
    DebugManager.flags.GTPinAllocateBufferInSharedMemory.set(-1);
    GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    auto canUseSharedAllocation = gtpinHelper.canUseSharedAllocation(pDevice->getHardwareInfo());
    if (!pDevice->getHardwareInfo().capabilityTable.ftrSvm) {
        EXPECT_FALSE(canUseSharedAllocation);
    }
}

HWTEST_F(GTPinTestsWithLocalMemory, givenGtPinWithSupportForSharedAllocationWhenGtPinHelperFunctionsAreCalledThenCheckIfSharedAllocationCabBeUsed) {
    GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    if (!gtpinHelper.canUseSharedAllocation(pDevice->getHardwareInfo())) {
        GTEST_SKIP();
    }

    class MockGTPinHwHelperHw : public GTPinHwHelperHw<FamilyType> {
      public:
        bool canUseSharedAllocation(const HardwareInfo &hwInfo) const override {
            canUseSharedAllocationCalled = true;
            return true;
        }
        mutable bool canUseSharedAllocationCalled = false;
    };

    const auto family = pDevice->getHardwareInfo().platform.eRenderCoreFamily;
    MockGTPinHwHelperHw mockGTPinHwHelperHw;
    VariableBackup<GTPinHwHelper *> gtpinHwHelperBackup{&gtpinHwHelperFactory[family], &mockGTPinHwHelperHw};

    resource_handle_t resource = nullptr;
    cl_context ctxt = (cl_context)((Context *)pContext);

    mockGTPinHwHelperHw.canUseSharedAllocationCalled = false;
    gtpinCreateBuffer((gtpin::context_handle_t)ctxt, 256, &resource);
    EXPECT_TRUE(mockGTPinHwHelperHw.canUseSharedAllocationCalled);

    mockGTPinHwHelperHw.canUseSharedAllocationCalled = false;
    uint8_t *address = nullptr;
    gtpinMapBuffer((gtpin::context_handle_t)ctxt, resource, &address);
    EXPECT_TRUE(mockGTPinHwHelperHw.canUseSharedAllocationCalled);

    mockGTPinHwHelperHw.canUseSharedAllocationCalled = false;
    gtpinUnmapBuffer((gtpin::context_handle_t)ctxt, resource);
    EXPECT_TRUE(mockGTPinHwHelperHw.canUseSharedAllocationCalled);

    mockGTPinHwHelperHw.canUseSharedAllocationCalled = false;
    gtpinFreeBuffer((gtpin::context_handle_t)ctxt, resource);
    EXPECT_TRUE(mockGTPinHwHelperHw.canUseSharedAllocationCalled);
}

HWTEST_F(GTPinTestsWithLocalMemory, givenGtPinCanUseSharedAllocationWhenGtPinBufferIsCreatedThenAllocateBufferInSharedMemory) {
    GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    if (!gtpinHelper.canUseSharedAllocation(pDevice->getHardwareInfo())) {
        GTEST_SKIP();
    }

    resource_handle_t resource = nullptr;
    cl_context ctxt = (cl_context)((Context *)pContext);
    GTPIN_DI_STATUS status = GTPIN_DI_SUCCESS;

    status = gtpinCreateBuffer((gtpin::context_handle_t)ctxt, 256, &resource);
    EXPECT_EQ(GTPIN_DI_SUCCESS, status);
    EXPECT_NE(nullptr, resource);

    auto allocData = reinterpret_cast<SvmAllocationData *>(resource);

    auto cpuAllocation = allocData->cpuAllocation;
    ASSERT_NE(nullptr, cpuAllocation);
    EXPECT_NE(AllocationType::UNIFIED_SHARED_MEMORY, cpuAllocation->getAllocationType());

    auto gpuAllocation = allocData->gpuAllocations.getGraphicsAllocation(pDevice->getRootDeviceIndex());
    ASSERT_NE(nullptr, gpuAllocation);
    EXPECT_NE(AllocationType::UNIFIED_SHARED_MEMORY, gpuAllocation->getAllocationType());

    uint8_t *address = nullptr;
    status = gtpinMapBuffer((gtpin::context_handle_t)ctxt, resource, &address);
    EXPECT_EQ(GTPIN_DI_SUCCESS, status);
    EXPECT_EQ(allocData->cpuAllocation->getUnderlyingBuffer(), address);

    status = gtpinUnmapBuffer((gtpin::context_handle_t)ctxt, resource);
    EXPECT_EQ(GTPIN_DI_SUCCESS, status);

    status = gtpinFreeBuffer((gtpin::context_handle_t)ctxt, resource);
    EXPECT_EQ(GTPIN_DI_SUCCESS, status);
}

HWTEST_F(GTPinTestsWithLocalMemory, givenGtPinCanUseSharedAllocationWhenGtPinBufferIsAllocatedInSharedMemoryThenSetSurfaceStateForTheBufferAndMakeItResident) {
    GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    if (!gtpinHelper.canUseSharedAllocation(pDevice->getHardwareInfo())) {
        GTEST_SKIP();
    }

    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;

    GTPIN_DI_STATUS status = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, status);

    cl_kernel kernel = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd16", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd16.cl");
    auto pSource = loadDataFromFile(testFile.c_str(), sourceSize);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    const char *sources[1] = {pSource.get()};
    pProgram = clCreateProgramWithSource(
        context,
        1,
        sources,
        &sourceSize,
        &retVal);
    ASSERT_NE(nullptr, pProgram);

    retVal = clBuildProgram(
        pProgram,
        1,
        &device,
        nullptr,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    kernel = clCreateKernel(pProgram, "CopyBuffer", &retVal);
    EXPECT_NE(nullptr, kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto pMultiDeviceKernel = static_cast<MultiDeviceKernel *>(kernel);
    auto pKernel = pMultiDeviceKernel->getKernel(rootDeviceIndex);
    auto pCmdQueue = castToObject<CommandQueue>(cmdQ);
    auto &csr = pCmdQueue->getGpgpuCommandStreamReceiver();

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    constexpr size_t renderSurfaceSize = sizeof(RENDER_SURFACE_STATE);

    size_t gtpinBTI = pKernel->getNumberOfBindingTableStates() - 1;
    void *pSurfaceState = gtpinHelper.getSurfaceState(pKernel, gtpinBTI);
    EXPECT_NE(nullptr, pSurfaceState);

    RENDER_SURFACE_STATE *surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(pSurfaceState);
    memset(pSurfaceState, 0, renderSurfaceSize);
    gtpinNotifyKernelSubmit(kernel, pCmdQueue);

    auto allocData = reinterpret_cast<SvmAllocationData *>(kernelExecQueue[0].gtpinResource);
    EXPECT_NE(nullptr, allocData);
    auto gpuAllocation = allocData->gpuAllocations.getGraphicsAllocation(rootDeviceIndex);
    EXPECT_NE(nullptr, gpuAllocation);

    RENDER_SURFACE_STATE expectedSurfaceState;
    memset(&expectedSurfaceState, 0, renderSurfaceSize);
    {
        void *addressToPatch = gpuAllocation->getUnderlyingBuffer();
        size_t sizeToPatch = gpuAllocation->getUnderlyingBufferSize();
        Buffer::setSurfaceState(&pDevice->getDevice(), &expectedSurfaceState, false, false,
                                sizeToPatch, addressToPatch, 0, gpuAllocation, 0, 0,
                                pKernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.useGlobalAtomics, pContext->getNumDevices());
    }
    EXPECT_EQ(0, memcmp(&expectedSurfaceState, surfaceState, renderSurfaceSize));

    EXPECT_FALSE(gpuAllocation->isResident(csr.getOsContext().getContextId()));
    gtpinNotifyMakeResident(pKernel, &csr);
    EXPECT_TRUE(gpuAllocation->isResident(csr.getOsContext().getContextId()));

    kernelExecQueue[0].isTaskCountValid = true;
    gtpinNotifyTaskCompletion(kernelExecQueue[0].taskCount);

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}
HWTEST_F(GTPinTestsWithLocalMemory, givenGtPinCanUseSharedAllocationWhenGtpinNotifyKernelSubmitThenMoveToAllocationDomainCalled) {
    GTPinHwHelper &gtpinHelper = GTPinHwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    if (!gtpinHelper.canUseSharedAllocation(pDevice->getHardwareInfo())) {
        GTEST_SKIP();
    }
    class MockGTPinHwHelperHw : public GTPinHwHelperHw<FamilyType> {
      public:
        void *getSurfaceState(Kernel *pKernel, size_t bti) override {
            return data;
        }
        uint8_t data[128];
    };

    struct MockResidentTestsPageFaultManager : public MockPageFaultManager {
        void moveAllocationToGpuDomain(void *ptr) override {
            moveAllocationToGpuDomainCalledTimes++;
            migratedAddress = ptr;
        }
        uint32_t moveAllocationToGpuDomainCalledTimes = 0;
        void *migratedAddress = nullptr;
    };
    static std::unique_ptr<SvmAllocationData> allocDataHandle;
    static std::unique_ptr<MockGraphicsAllocation> mockGAHandle;
    const auto family = pDevice->getHardwareInfo().platform.eRenderCoreFamily;
    MockGTPinHwHelperHw mockGTPinHwHelperHw;
    VariableBackup<GTPinHwHelper *> gtpinHwHelperBackup{&gtpinHwHelperFactory[family], &mockGTPinHwHelperHw};
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = [](command_buffer_handle_t cb, uint64_t kernelId, uint32_t *entryOffset, resource_handle_t *resource) {
        auto allocData = std::make_unique<SvmAllocationData>(0);
        auto mockGA = std::make_unique<MockGraphicsAllocation>();
        allocData->gpuAllocations.addAllocation(mockGA.get());
        *resource = reinterpret_cast<resource_handle_t>(allocData.get());
        allocDataHandle = std::move(allocData);
        mockGAHandle = std::move(mockGA);
    };
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;

    GTPIN_DI_STATUS status = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, status);

    MockKernelWithInternals mockkernel(*pDevice);
    MockCommandQueue mockCmdQueue;
    cl_context ctxt = (cl_context)((Context *)pContext);
    currContext = (gtpin::context_handle_t)(ctxt);
    mockCmdQueue.device = pDevice;

    gtpinNotifyKernelSubmit(mockkernel.mockMultiDeviceKernel, &mockCmdQueue);
    EXPECT_EQ(reinterpret_cast<MockResidentTestsPageFaultManager *>(pDevice->getExecutionEnvironment()->memoryManager->getPageFaultManager())->moveAllocationToGpuDomainCalledTimes, 1u);

    mockCmdQueue.device = nullptr;
    mockGAHandle.reset();
    allocDataHandle.reset();
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenGtpinRemoveCommandQueueIsCalledThenAllKernelsFromCmdQueueAreRemoved) {
    gtpinCallbacks.onContextCreate = onContextCreate;
    gtpinCallbacks.onContextDestroy = onContextDestroy;
    gtpinCallbacks.onKernelCreate = onKernelCreate;
    gtpinCallbacks.onKernelSubmit = onKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = onCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = onCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    kernelExecQueue.clear();

    CommandQueue *cmdQ1 = reinterpret_cast<CommandQueue *>(1);
    CommandQueue *cmdQ2 = reinterpret_cast<CommandQueue *>(2);
    Kernel *kernel1 = reinterpret_cast<Kernel *>(1);
    Kernel *kernel2 = reinterpret_cast<Kernel *>(2);
    Kernel *kernel3 = reinterpret_cast<Kernel *>(3);
    Kernel *kernel4 = reinterpret_cast<Kernel *>(4);

    gtpinkexec_t kExec;
    kExec.pKernel = kernel1;
    kExec.pCommandQueue = cmdQ1;
    kernelExecQueue.push_back(kExec);

    kExec.pKernel = kernel2;
    kExec.pCommandQueue = cmdQ1;
    kernelExecQueue.push_back(kExec);

    kExec.pKernel = kernel3;
    kExec.pCommandQueue = cmdQ2;
    kernelExecQueue.push_back(kExec);

    kExec.pKernel = kernel4;
    kExec.pCommandQueue = cmdQ2;
    kernelExecQueue.push_back(kExec);
    EXPECT_EQ(4u, kernelExecQueue.size());

    gtpinRemoveCommandQueue(cmdQ1);
    EXPECT_EQ(2u, kernelExecQueue.size());

    gtpinRemoveCommandQueue(cmdQ2);
    EXPECT_EQ(0u, kernelExecQueue.size());
}

} // namespace ULT
