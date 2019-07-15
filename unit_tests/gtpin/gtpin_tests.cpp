/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/basic_math.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/gtpin/gtpin_defs.h"
#include "runtime/gtpin/gtpin_helpers.h"
#include "runtime/gtpin/gtpin_hw_helper.h"
#include "runtime/gtpin/gtpin_init.h"
#include "runtime/gtpin/gtpin_notify.h"
#include "runtime/helpers/file_io.h"
#include "runtime/helpers/hash.h"
#include "runtime/helpers/options.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/os_interface/os_context.h"
#include "test.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/program/program_tests.h"

#include "gtest/gtest.h"

#include <deque>
#include <vector>

using namespace NEO;
using namespace gtpin;

namespace NEO {
extern std::deque<gtpinkexec_t> kernelExecQueue;
}

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

void OnContextCreate(context_handle_t context, platform_info_t *platformInfo, igc_init_t **igcInit) {
    currContext = context;
    kernelResources.clear();
    ContextCreateCallbackCount++;
    *igcInit = reinterpret_cast<igc_init_t *>(0x1234);
}

void OnContextDestroy(context_handle_t context) {
    currContext = nullptr;
    EXPECT_EQ(0u, kernelResources.size());
    kernelResources.clear();
    ContextDestroyCallbackCount++;
}

void OnKernelCreate(context_handle_t context, const instrument_params_in_t *paramsIn, instrument_params_out_t *paramsOut) {
    paramsOut->inst_kernel_binary = const_cast<uint8_t *>(paramsIn->orig_kernel_binary);
    paramsOut->inst_kernel_size = paramsIn->orig_kernel_size;
    paramsOut->kernel_id = paramsIn->igc_hash_id;
    KernelCreateCallbackCount++;
}

void OnKernelSubmit(command_buffer_handle_t cb, uint64_t kernelId, uint32_t *entryOffset, resource_handle_t *resource) {
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

void OnCommandBufferCreate(context_handle_t context, command_buffer_handle_t cb) {
    CommandBufferCreateCallbackCount++;
}

void OnCommandBufferComplete(command_buffer_handle_t cb) {
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

class GTPinFixture : public ContextFixture, public MemoryManagementFixture {
    using ContextFixture::SetUp;

  public:
    void SetUp() override {
        platformImpl.reset();
        MemoryManagementFixture::SetUp();
        constructPlatform();
        pPlatform = platform();
        auto executionEnvironment = pPlatform->peekExecutionEnvironment();
        executionEnvironment->setHwInfo(*platformDevices);
        memoryManager = new MockMemoryManagerWithFailures(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);
        pPlatform->initialize();
        pDevice = pPlatform->getDevice(0);
        cl_device_id device = (cl_device_id)pDevice;
        ContextFixture::SetUp(1, &device);

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

    void TearDown() override {
        ContextFixture::TearDown();
        platformImpl.reset(nullptr);
        MemoryManagementFixture::TearDown();
        NEO::isGTPinInitialized = false;
    }

    Platform *pPlatform = nullptr;
    Device *pDevice = nullptr;
    cl_int retVal = CL_SUCCESS;
    GTPIN_DI_STATUS retFromGtPin = GTPIN_DI_SUCCESS;
    driver_services_t driverServices;
    gtpin::ocl::gtpin_events_t gtpinCallbacks;
    MockMemoryManagerWithFailures *memoryManager = nullptr;
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

    gtpinCallbacks.onContextCreate = OnContextCreate;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
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

    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
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
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
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

    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
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
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
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
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
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
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
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

    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
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

    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
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

    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
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

    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
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

    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
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
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
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
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
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
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;

    KernelBinaryHelper kbHelper("CopyBuffer_simd8", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl");
    sourceSize = loadDataFromFile(testFile.c_str(), pSource);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    pProgram = clCreateProgramWithSource(
        (cl_context)((Context *)pContext),
        1,
        (const char **)&pSource,
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

    deleteDataReadFromFile(pSource);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenKernelIsExecutedThenGTPinCallbacksAreCalled) {
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_kernel kernel1 = nullptr;
    cl_kernel kernel2 = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd8", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl");
    sourceSize = loadDataFromFile(testFile.c_str(), pSource);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pProgram = clCreateProgramWithSource(
        context,
        1,
        (const char **)&pSource,
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

    Kernel *pKernel1 = (Kernel *)kernel1;
    const KernelInfo &kInfo1 = pKernel1->getKernelInfo();
    uint64_t gtpinKernelId1 = pKernel1->getKernelId();
    EXPECT_EQ(kInfo1.heapInfo.pKernelHeader->ShaderHashCode, gtpinKernelId1);

    constexpr size_t n = 256;
    auto buff10 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);
    auto buff11 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);

    retVal = clSetKernelArg(pKernel1, 0, sizeof(cl_mem), &buff10);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel1, 1, sizeof(cl_mem), &buff11);
    EXPECT_EQ(CL_SUCCESS, retVal);

    int prevCount12 = KernelSubmitCallbackCount;
    int prevCount13 = CommandBufferCreateCallbackCount;
    int prevCount14 = CommandBufferCompleteCallbackCount;
    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {n, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    retVal = clEnqueueNDRangeKernel(cmdQ, pKernel1, workDim, globalWorkOffset, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
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

    Kernel *pKernel2 = (Kernel *)kernel2;
    const KernelInfo &kInfo2 = pKernel2->getKernelInfo();
    uint64_t gtpinKernelId2 = pKernel2->getKernelId();
    EXPECT_EQ(kInfo2.heapInfo.pKernelHeader->ShaderHashCode, gtpinKernelId2);

    auto buff20 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);
    auto buff21 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);

    retVal = clSetKernelArg(pKernel2, 0, sizeof(cl_mem), &buff20);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel2, 1, sizeof(cl_mem), &buff21);
    EXPECT_EQ(CL_SUCCESS, retVal);

    int prevCount22 = KernelSubmitCallbackCount;
    int prevCount23 = CommandBufferCreateCallbackCount;
    int prevCount24 = CommandBufferCompleteCallbackCount;
    retVal = clEnqueueNDRangeKernel(cmdQ, pKernel2, workDim, globalWorkOffset, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
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

    deleteDataReadFromFile(pSource);

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
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_device_id device = (cl_device_id)pDevice;
    cl_context context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);
    auto pContext = castToObject<Context>(context);

    // Prepare a kernel without SSH
    char binary[1024] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t binSize = 10;
    Program *pProgram = Program::createFromGenBinary(*pDevice->getExecutionEnvironment(), pContext, &binary[0], binSize, false, &retVal);
    ASSERT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    char *pBin = &binary[0];
    SProgramBinaryHeader *pBHdr = (SProgramBinaryHeader *)pBin;
    pBHdr->Magic = iOpenCL::MAGIC_CL;
    pBHdr->Version = iOpenCL::CURRENT_ICBE_VERSION;
    pBHdr->Device = pDevice->getHardwareInfo().platform.eRenderCoreFamily;
    pBHdr->GPUPointerSizeInBytes = 8;
    pBHdr->NumberOfKernels = 1;
    pBHdr->SteppingId = 0;
    pBHdr->PatchListSize = 0;
    pBin += sizeof(SProgramBinaryHeader);
    binSize += sizeof(SProgramBinaryHeader);

    SKernelBinaryHeaderCommon *pKHdr = (SKernelBinaryHeaderCommon *)pBin;
    pKHdr->CheckSum = 0;
    pKHdr->ShaderHashCode = 0;
    pKHdr->KernelNameSize = 4;
    pKHdr->PatchListSize = 0;
    pKHdr->KernelHeapSize = 16;
    pKHdr->GeneralStateHeapSize = 0;
    pKHdr->DynamicStateHeapSize = 0;
    pKHdr->SurfaceStateHeapSize = 0;
    pKHdr->KernelUnpaddedSize = 0;
    pBin += sizeof(SKernelBinaryHeaderCommon);
    binSize += sizeof(SKernelBinaryHeaderCommon);
    char *pKernelBin = pBin;

    strcpy(pBin, "Tst");
    pBin += pKHdr->KernelNameSize;
    binSize += pKHdr->KernelNameSize;

    strcpy(pBin, "fake_ISA_code__");
    binSize += pKHdr->KernelHeapSize;

    uint32_t kernelBinSize =
        pKHdr->DynamicStateHeapSize +
        pKHdr->GeneralStateHeapSize +
        pKHdr->KernelHeapSize +
        pKHdr->KernelNameSize +
        pKHdr->PatchListSize +
        pKHdr->SurfaceStateHeapSize;
    uint64_t hashValue = Hash::hash(reinterpret_cast<const char *>(pKernelBin), kernelBinSize);
    pKHdr->CheckSum = static_cast<uint32_t>(hashValue & 0xFFFFFFFF);

    pProgram->storeGenBinary(&binary[0], binSize);
    retVal = pProgram->processGenBinary();
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Verify that GT-Pin Kernel Create callback is not called
    int prevCount = KernelCreateCallbackCount;
    cl_kernel kernel = clCreateKernel(pProgram, "Tst", &retVal);
    EXPECT_NE(nullptr, kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount, KernelCreateCallbackCount);

    // Cleanup
    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenKernelWithExecEnvIsUsedThenKernelCreateCallbacksIsNotCalled) {
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_device_id device = (cl_device_id)pDevice;
    cl_context context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);
    auto pContext = castToObject<Context>(context);

    // Prepare a kernel with fake Execution Environment
    char binary[1024] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t binSize = 10;
    Program *pProgram = Program::createFromGenBinary(*pDevice->getExecutionEnvironment(), pContext, &binary[0], binSize, false, &retVal);
    ASSERT_NE(nullptr, pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    char *pBin = &binary[0];
    SProgramBinaryHeader *pBHdr = (SProgramBinaryHeader *)pBin;
    pBHdr->Magic = iOpenCL::MAGIC_CL;
    pBHdr->Version = iOpenCL::CURRENT_ICBE_VERSION;
    pBHdr->Device = pDevice->getHardwareInfo().platform.eRenderCoreFamily;
    pBHdr->GPUPointerSizeInBytes = 8;
    pBHdr->NumberOfKernels = 1;
    pBHdr->SteppingId = 0;
    pBHdr->PatchListSize = 0;
    pBin += sizeof(SProgramBinaryHeader);
    binSize += sizeof(SProgramBinaryHeader);

    SKernelBinaryHeaderCommon *pKHdr = (SKernelBinaryHeaderCommon *)pBin;
    pKHdr->CheckSum = 0;
    pKHdr->ShaderHashCode = 0;
    pKHdr->KernelNameSize = 4;
    pKHdr->PatchListSize = sizeof(SPatchExecutionEnvironment) + sizeof(SPatchBindingTableState);
    pKHdr->KernelHeapSize = 16;
    pKHdr->GeneralStateHeapSize = 0;
    pKHdr->DynamicStateHeapSize = 0;
    pKHdr->SurfaceStateHeapSize = 64;
    pKHdr->KernelUnpaddedSize = 0;
    pBin += sizeof(SKernelBinaryHeaderCommon);
    binSize += sizeof(SKernelBinaryHeaderCommon);
    char *pKernelBin = pBin;

    strcpy(pBin, "Tst");
    pBin += pKHdr->KernelNameSize;
    binSize += pKHdr->KernelNameSize;

    strcpy(pBin, "fake_ISA_code__");
    pBin += pKHdr->KernelHeapSize;
    binSize += pKHdr->KernelHeapSize;

    memset(pBin, 0, pKHdr->SurfaceStateHeapSize);
    pBin += pKHdr->SurfaceStateHeapSize;
    binSize += pKHdr->SurfaceStateHeapSize;

    SPatchExecutionEnvironment *pPatch1 = (SPatchExecutionEnvironment *)pBin;
    pPatch1->Token = iOpenCL::PATCH_TOKEN_EXECUTION_ENVIRONMENT;
    pPatch1->Size = sizeof(iOpenCL::SPatchExecutionEnvironment);
    pPatch1->RequiredWorkGroupSizeX = 0;
    pPatch1->RequiredWorkGroupSizeY = 0;
    pPatch1->RequiredWorkGroupSizeZ = 0;
    pPatch1->LargestCompiledSIMDSize = 8;
    pPatch1->CompiledSubGroupsNumber = 0;
    pPatch1->HasBarriers = 0;
    pPatch1->DisableMidThreadPreemption = 0;
    pPatch1->CompiledSIMD8 = 1;
    pPatch1->CompiledSIMD16 = 0;
    pPatch1->CompiledSIMD32 = 0;
    pPatch1->HasDeviceEnqueue = 1;
    pPatch1->MayAccessUndeclaredResource = 0;
    pPatch1->UsesFencesForReadWriteImages = 0;
    pPatch1->UsesStatelessSpillFill = 0;
    pPatch1->IsCoherent = 0;
    pPatch1->IsInitializer = 0;
    pPatch1->IsFinalizer = 0;
    pPatch1->SubgroupIndependentForwardProgressRequired = 0;
    pPatch1->CompiledForGreaterThan4GBBuffers = 0;
    pBin += sizeof(SPatchExecutionEnvironment);
    binSize += sizeof(SPatchExecutionEnvironment);

    SPatchBindingTableState *pPatch2 = (SPatchBindingTableState *)pBin;
    pPatch2->Token = iOpenCL::PATCH_TOKEN_BINDING_TABLE_STATE;
    pPatch2->Size = sizeof(iOpenCL::SPatchBindingTableState);
    pPatch2->Offset = 0;
    pPatch2->Count = 1;
    pPatch2->SurfaceStateOffset = 0;
    binSize += sizeof(SPatchBindingTableState);

    uint32_t kernelBinSize =
        pKHdr->DynamicStateHeapSize +
        pKHdr->GeneralStateHeapSize +
        pKHdr->KernelHeapSize +
        pKHdr->KernelNameSize +
        pKHdr->PatchListSize +
        pKHdr->SurfaceStateHeapSize;
    uint64_t hashValue = Hash::hash(reinterpret_cast<const char *>(pKernelBin), kernelBinSize);
    pKHdr->CheckSum = static_cast<uint32_t>(hashValue & 0xFFFFFFFF);

    pProgram->storeGenBinary(&binary[0], binSize);
    retVal = pProgram->processGenBinary();
    EXPECT_EQ(CL_SUCCESS, retVal);

    // Verify that GT-Pin Kernel Create callback is not called
    int prevCount = KernelCreateCallbackCount;
    cl_kernel kernel = clCreateKernel(pProgram, "Tst", &retVal);
    EXPECT_NE(nullptr, kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(prevCount, KernelCreateCallbackCount);

    // Cleanup
    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenKernelWithoutSSHIsUsedThenGTPinSubmitKernelCallbackIsNotCalled) {
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_kernel kernel = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd8", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl");
    sourceSize = loadDataFromFile(testFile.c_str(), pSource);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pProgram = clCreateProgramWithSource(
        context,
        1,
        (const char **)&pSource,
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

    Kernel *pKernel = (Kernel *)kernel;
    const KernelInfo &kInfo = pKernel->getKernelInfo();
    uint64_t gtpinKernelId = pKernel->getKernelId();
    EXPECT_EQ(kInfo.heapInfo.pKernelHeader->ShaderHashCode, gtpinKernelId);

    constexpr size_t n = 256;
    auto buff0 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);
    auto buff1 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);

    retVal = clSetKernelArg(pKernel, 0, sizeof(cl_mem), &buff0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel, 1, sizeof(cl_mem), &buff1);
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
    retVal = clEnqueueNDRangeKernel(cmdQ, pKernel, workDim, globalWorkOffset, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
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

    deleteDataReadFromFile(pSource);

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
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_kernel kernel = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd8", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl");
    sourceSize = loadDataFromFile(testFile.c_str(), pSource);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pProgram = clCreateProgramWithSource(
        context,
        1,
        (const char **)&pSource,
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

    Kernel *pKernel = (Kernel *)kernel;
    const KernelInfo &kInfo = pKernel->getKernelInfo();
    uint64_t gtpinKernelId = pKernel->getKernelId();
    EXPECT_EQ(kInfo.heapInfo.pKernelHeader->ShaderHashCode, gtpinKernelId);

    constexpr size_t n = 256;
    auto buff0 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);
    auto buff1 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);

    retVal = clSetKernelArg(pKernel, 0, sizeof(cl_mem), &buff0);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel, 1, sizeof(cl_mem), &buff1);
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
    retVal = clEnqueueNDRangeKernel(cmdQ, pKernel, workDim, globalWorkOffset, globalWorkSize, localWorkSize, 1, &userEvent, nullptr);
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

    deleteDataReadFromFile(pSource);

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
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_kernel kernel1 = nullptr;
    cl_kernel kernel2 = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd8", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl");
    sourceSize = loadDataFromFile(testFile.c_str(), pSource);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pProgram = clCreateProgramWithSource(
        context,
        1,
        (const char **)&pSource,
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

    Kernel *pKernel1 = (Kernel *)kernel1;
    const KernelInfo &kInfo1 = pKernel1->getKernelInfo();
    uint64_t gtpinKernelId1 = pKernel1->getKernelId();
    EXPECT_EQ(kInfo1.heapInfo.pKernelHeader->ShaderHashCode, gtpinKernelId1);

    constexpr size_t n = 256;
    auto buff10 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);
    auto buff11 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);

    retVal = clSetKernelArg(pKernel1, 0, sizeof(cl_mem), &buff10);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel1, 1, sizeof(cl_mem), &buff11);
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
    retVal = clEnqueueNDRangeKernel(cmdQ, pKernel1, workDim, globalWorkOffset, globalWorkSize, localWorkSize, 1, &userEvent, nullptr);
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

    Kernel *pKernel2 = (Kernel *)kernel2;
    const KernelInfo &kInfo2 = pKernel2->getKernelInfo();
    uint64_t gtpinKernelId2 = pKernel2->getKernelId();
    EXPECT_EQ(kInfo2.heapInfo.pKernelHeader->ShaderHashCode, gtpinKernelId2);

    auto buff20 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);
    auto buff21 = clCreateBuffer(context, 0, n * sizeof(unsigned int), nullptr, nullptr);

    retVal = clSetKernelArg(pKernel2, 0, sizeof(cl_mem), &buff20);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clSetKernelArg(pKernel2, 1, sizeof(cl_mem), &buff21);
    EXPECT_EQ(CL_SUCCESS, retVal);

    int prevCount22 = KernelSubmitCallbackCount;
    int prevCount23 = CommandBufferCreateCallbackCount;
    int prevCount24 = CommandBufferCompleteCallbackCount;
    EXPECT_EQ(prevCount14, prevCount24);
    retVal = clEnqueueNDRangeKernel(cmdQ, pKernel2, workDim, globalWorkOffset, globalWorkSize, localWorkSize, 0, nullptr, nullptr);
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

    deleteDataReadFromFile(pSource);

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
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    cl_kernel kernel1 = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd8", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl");
    sourceSize = loadDataFromFile(testFile.c_str(), pSource);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pProgram = clCreateProgramWithSource(
        context,
        1,
        (const char **)&pSource,
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

    Kernel *pKernel1 = (Kernel *)kernel1;
    returnNullResource = true;

    auto pCmdQueue = castToObject<CommandQueue>(cmdQ);

    gtpinNotifyKernelSubmit(pKernel1, pCmdQueue);
    EXPECT_EQ(nullptr, kernelExecQueue[0].gtpinResource);

    CommandStreamReceiver &csr = pCmdQueue->getGpgpuCommandStreamReceiver();
    gtpinNotifyMakeResident(pKernel1, &csr);
    EXPECT_FALSE(kernelExecQueue[0].isResourceResident);

    std::vector<Surface *> residencyVector;
    gtpinNotifyUpdateResidencyList(pKernel1, &residencyVector);
    EXPECT_EQ(0u, residencyVector.size());

    returnNullResource = false;

    gtpinNotifyKernelSubmit(pKernel1, pCmdQueue);
    EXPECT_NE(nullptr, kernelExecQueue[1].gtpinResource);
    gtpinNotifyMakeResident(pKernel1, &csr);
    EXPECT_TRUE(kernelExecQueue[1].isResourceResident);
    cl_mem gtpinBuffer1 = kernelExecQueue[1].gtpinResource;

    gtpinNotifyKernelSubmit(pKernel1, pCmdQueue);
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

    deleteDataReadFromFile(pSource);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenKernelIsCreatedThenAllKernelSubmitRelatedNotificationsAreCalled) {
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    kernelExecQueue.clear();

    cl_kernel kernel = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd8", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl");
    sourceSize = loadDataFromFile(testFile.c_str(), pSource);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pProgram = clCreateProgramWithSource(
        context,
        1,
        (const char **)&pSource,
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
    auto pKernel = castToObject<Kernel>(kernel);
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
    auto pKernel2 = castToObject<Kernel>(kernel2);
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
    GraphicsAllocation *pGfxAlloc0 = pBuffer0->getGraphicsAllocation();
    cl_mem gtpinBuffer1 = kernelExecQueue[1].gtpinResource;
    auto pBuffer1 = castToObject<Buffer>(gtpinBuffer1);
    GraphicsAllocation *pGfxAlloc1 = pBuffer1->getGraphicsAllocation();
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

    deleteDataReadFromFile(pSource);

    retVal = clReleaseCommandQueue(cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenOneKernelIsSubmittedSeveralTimesThenCorrectBuffersAreMadeResident) {
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    kernelExecQueue.clear();

    cl_kernel kernel = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;
    cl_command_queue cmdQ = nullptr;
    cl_queue_properties properties = 0;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd8", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl");
    sourceSize = loadDataFromFile(testFile.c_str(), pSource);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    cmdQ = clCreateCommandQueue(context, device, properties, &retVal);
    ASSERT_NE(nullptr, cmdQ);
    EXPECT_EQ(CL_SUCCESS, retVal);

    pProgram = clCreateProgramWithSource(
        context,
        1,
        (const char **)&pSource,
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
    auto pKernel = castToObject<Kernel>(kernel);
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
    GraphicsAllocation *pGfxAlloc0 = pBuffer0->getGraphicsAllocation();
    cl_mem gtpinBuffer1 = kernelExecQueue[1].gtpinResource;
    auto pBuffer1 = castToObject<Buffer>(gtpinBuffer1);
    GraphicsAllocation *pGfxAlloc1 = pBuffer1->getGraphicsAllocation();
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

    deleteDataReadFromFile(pSource);

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

        // Prepare a program with one kernel having Stateless Private Surface
        char binary[1024] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
        size_t binSize = 10;
        Program *pProgram = Program::createFromGenBinary(*pDevice->getExecutionEnvironment(), pContext, &binary[0], binSize, false, &retVal);
        ASSERT_NE(nullptr, pProgram);
        EXPECT_EQ(CL_SUCCESS, retVal);

        char *pBin = &binary[0];
        SProgramBinaryHeader *pBHdr = (SProgramBinaryHeader *)pBin;
        pBHdr->Magic = iOpenCL::MAGIC_CL;
        pBHdr->Version = iOpenCL::CURRENT_ICBE_VERSION;
        pBHdr->Device = pDevice->getHardwareInfo().platform.eRenderCoreFamily;
        pBHdr->GPUPointerSizeInBytes = 8;
        pBHdr->NumberOfKernels = 1;
        pBHdr->SteppingId = 0;
        pBHdr->PatchListSize = 0;
        pBin += sizeof(SProgramBinaryHeader);
        binSize += sizeof(SProgramBinaryHeader);

        SKernelBinaryHeaderCommon *pKHdr = (SKernelBinaryHeaderCommon *)pBin;
        pKHdr->CheckSum = 0;
        pKHdr->ShaderHashCode = 0;
        pKHdr->KernelNameSize = 4;
        pKHdr->PatchListSize = sizeof(SPatchAllocateStatelessPrivateSurface);
        pKHdr->KernelHeapSize = 16;
        pKHdr->GeneralStateHeapSize = 0;
        pKHdr->DynamicStateHeapSize = 0;
        pKHdr->SurfaceStateHeapSize = 0;
        pKHdr->KernelUnpaddedSize = 0;
        pBin += sizeof(SKernelBinaryHeaderCommon);
        binSize += sizeof(SKernelBinaryHeaderCommon);
        char *pKernelBin = pBin;

        strcpy(pBin, "Tst");
        pBin += pKHdr->KernelNameSize;
        binSize += pKHdr->KernelNameSize;

        strcpy(pBin, "fake_ISA_code__");
        pBin += pKHdr->KernelHeapSize;
        binSize += pKHdr->KernelHeapSize;

        SPatchAllocateStatelessPrivateSurface *pPatch = (SPatchAllocateStatelessPrivateSurface *)pBin;
        pPatch->Token = iOpenCL::PATCH_TOKEN_ALLOCATE_STATELESS_PRIVATE_MEMORY;
        pPatch->Size = sizeof(iOpenCL::SPatchAllocateStatelessPrivateSurface);
        pPatch->SurfaceStateHeapOffset = 0;
        pPatch->DataParamOffset = 0;
        pPatch->DataParamSize = 0;
        pPatch->PerThreadPrivateMemorySize = 4;
        binSize += sizeof(SPatchAllocateStatelessPrivateSurface);

        uint32_t kernelBinSize =
            pKHdr->DynamicStateHeapSize +
            pKHdr->GeneralStateHeapSize +
            pKHdr->KernelHeapSize +
            pKHdr->KernelNameSize +
            pKHdr->PatchListSize +
            pKHdr->SurfaceStateHeapSize;
        uint64_t hashValue = Hash::hash(reinterpret_cast<const char *>(pKernelBin), kernelBinSize);
        pKHdr->CheckSum = static_cast<uint32_t>(hashValue & 0xFFFFFFFF);

        pProgram->storeGenBinary(&binary[0], binSize);
        retVal = pProgram->processGenBinary();
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

    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
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
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd8", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl");
    sourceSize = loadDataFromFile(testFile.c_str(), pSource);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    pProgram = clCreateProgramWithSource(
        context,
        1,
        (const char **)&pSource,
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
    Kernel *pKernel = castToObject<Kernel>(kernel);
    ASSERT_NE(nullptr, pKernel);

    size_t numBTS1 = pKernel->getNumberOfBindingTableStates();
    EXPECT_EQ(2u, numBTS1);
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

    deleteDataReadFromFile(pSource);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, givenKernelThenVerifyThatKernelCodeSubstitutionWorksWell) {
    cl_kernel kernel = nullptr;
    cl_program pProgram = nullptr;
    cl_device_id device = (cl_device_id)pDevice;
    void *pSource = nullptr;
    size_t sourceSize = 0;
    std::string testFile;
    cl_context context = nullptr;

    KernelBinaryHelper kbHelper("CopyBuffer_simd8", false);
    testFile.append(clFiles);
    testFile.append("CopyBuffer_simd8.cl");
    sourceSize = loadDataFromFile(testFile.c_str(), pSource);
    EXPECT_NE(0u, sourceSize);
    EXPECT_NE(nullptr, pSource);

    context = clCreateContext(nullptr, 1, &device, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);

    pProgram = clCreateProgramWithSource(
        context,
        1,
        (const char **)&pSource,
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
    Kernel *pKernel = castToObject<Kernel>(kernel);
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

    deleteDataReadFromFile(pSource);

    retVal = clReleaseContext(context);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GTPinTests, checkWhetherGTPinHwHelperGetterWorksWell) {
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
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    VariableBackup<bool> returnNullResourceBckp(&returnNullResource);
    VariableBackup<uint32_t> kernelOffsetBckp(&kernelOffset);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);

    char surfaceStateHeap[0x80];
    SKernelBinaryHeaderCommon kernelHeader;
    std::unique_ptr<MockContext> context(new MockContext(pDevice));

    EXPECT_EQ(CL_SUCCESS, retVal);
    auto pKernelInfo = std::make_unique<KernelInfo>();
    kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);
    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;
    pKernelInfo->usesSsh = true;

    auto pProgramm = std::make_unique<MockProgram>(*pDevice->getExecutionEnvironment(), context.get(), false);
    std::unique_ptr<MockCommandQueue> cmdQ(new MockCommandQueue(context.get(), pDevice, nullptr));
    std::unique_ptr<MockKernel> pKernel(new MockKernel(pProgramm.get(), *pKernelInfo, *pDevice));

    pKernel->setSshLocal(nullptr, sizeof(surfaceStateHeap));

    kernelOffset = 0x1234;
    EXPECT_NE(pKernel->getStartOffset(), kernelOffset);
    returnNullResource = true;
    cl_context ctxt = (cl_context)((Context *)context.get());
    currContext = (gtpin::context_handle_t)ctxt;
    gtpinNotifyKernelSubmit(pKernel.get(), cmdQ.get());
    EXPECT_EQ(pKernel->getStartOffset(), kernelOffset);

    EXPECT_EQ(CL_SUCCESS, retVal);

    kernelResources.clear();
}
TEST_F(GTPinTests, givenInitializedGTPinInterfaceWhenOnContextCreateIsCalledThenGtpinInitIsSet) {
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    auto context = std::make_unique<MockContext>();
    gtpinNotifyContextCreate(context.get());
    EXPECT_NE(gtpinGetIgcInit(), nullptr);
}

TEST_F(ProgramTests, givenGenBinaryWithGtpinInfoWhenProcessGenBinaryCalledThenGtpinInfoIsSet) {
    cl_int retVal = CL_INVALID_BINARY;
    char genBin[1024] = {1, 2, 3, 4, 5, 6, 7, 8, 9, '\0'};
    size_t binSize = 10;

    std::unique_ptr<Program> pProgram(Program::createFromGenBinary(*pDevice->getExecutionEnvironment(), nullptr, &genBin[0], binSize, false, &retVal));
    EXPECT_NE(nullptr, pProgram.get());
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ((uint32_t)CL_PROGRAM_BINARY_TYPE_EXECUTABLE, (uint32_t)pProgram->getProgramBinaryType());

    cl_device_id deviceId = pContext->getDevice(0);
    Device *pDevice = castToObject<Device>(deviceId);
    char *pBin = &genBin[0];
    retVal = CL_INVALID_BINARY;
    binSize = 0;

    // Prepare simple program binary containing patch token PATCH_TOKEN_GLOBAL_MEMORY_OBJECT_KERNEL_ARGUMENT
    SProgramBinaryHeader *pBHdr = (SProgramBinaryHeader *)pBin;
    pBHdr->Magic = iOpenCL::MAGIC_CL;
    pBHdr->Version = iOpenCL::CURRENT_ICBE_VERSION;
    pBHdr->Device = pDevice->getHardwareInfo().platform.eRenderCoreFamily;
    pBHdr->GPUPointerSizeInBytes = 8;
    pBHdr->NumberOfKernels = 1;
    pBHdr->SteppingId = 0;
    pBHdr->PatchListSize = 0;
    pBin += sizeof(SProgramBinaryHeader);
    binSize += sizeof(SProgramBinaryHeader);

    SKernelBinaryHeaderCommon *pKHdr = (SKernelBinaryHeaderCommon *)pBin;
    pKHdr->CheckSum = 0;
    pKHdr->ShaderHashCode = 0;
    pKHdr->KernelNameSize = 8;
    pKHdr->PatchListSize = 8;
    pKHdr->KernelHeapSize = 0;
    pKHdr->GeneralStateHeapSize = 0;
    pKHdr->DynamicStateHeapSize = 0;
    pKHdr->SurfaceStateHeapSize = 0;
    pKHdr->KernelUnpaddedSize = 0;
    pBin += sizeof(SKernelBinaryHeaderCommon);
    binSize += sizeof(SKernelBinaryHeaderCommon);

    strcpy(pBin, "TstCopy");
    pBin += pKHdr->KernelNameSize;
    binSize += pKHdr->KernelNameSize;

    iOpenCL::SPatchItemHeader *pPatch = (iOpenCL::SPatchItemHeader *)pBin;
    pPatch->Token = iOpenCL::PATCH_TOKEN_GTPIN_INFO;
    pPatch->Size = sizeof(iOpenCL::SPatchItemHeader);
    binSize += sizeof(iOpenCL::SPatchItemHeader);
    // Decode prepared program binary
    pProgram->storeGenBinary(&genBin[0], binSize);
    retVal = pProgram->processGenBinary();
    auto kernelInfo = pProgram->getKernelInfo("TstCopy");
    EXPECT_NE(kernelInfo->igcInfoForGtpin, nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
}
} // namespace ULT
