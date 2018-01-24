/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"
#include "runtime/context/context.h"
#include "runtime/device/device.h"
#include "runtime/gtpin/gtpin_init.h"
#include "runtime/gtpin/gtpin_helpers.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/file_io.h"
#include "runtime/helpers/options.h"
#include "runtime/kernel/kernel.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/helpers/kernel_binary_helper.h"
#include "unit_tests/helpers/test_files.h"
#include "test.h"
#include "gtest/gtest.h"

using namespace OCLRT;
using namespace gtpin;

namespace OCLRT {
extern bool isGTPinInitialized;
}

namespace ULT {

int ContextCreateCallbackCount = 0;
int ContextDestroyCallbackCount = 0;
int KernelCreateCallbackCount = 0;

void OnContextCreate(context_handle_t context, platform_info_t *platformInfo, igc_init_t **igcInit) {
    ContextCreateCallbackCount++;
}

void OnContextDestroy(context_handle_t context) {
    ContextDestroyCallbackCount++;
}

void OnKernelCreate(context_handle_t context, const instrument_params_in_t *paramsIn, instrument_params_out_t *paramsOut) {
    paramsOut->inst_kernel_binary = const_cast<uint8_t *>(paramsIn->orig_kernel_binary);
    paramsOut->inst_kernel_size = paramsIn->orig_kernel_size;
    paramsOut->kernel_id = paramsIn->igc_hash_id;
    KernelCreateCallbackCount++;
}

void OnKernelSubmit(command_buffer_handle_t cb, uint64_t kernelId, uint32_t *entryOffset, resource_handle_t *resource) {
}

void OnCommandBufferCreate(context_handle_t context, command_buffer_handle_t cb) {
}

void OnCommandBufferComplete(command_buffer_handle_t cb) {
}

class GTPinFixture : public ContextFixture, public MemoryManagementFixture {
    using ContextFixture::SetUp;

  public:
    GTPinFixture() {
    }

  protected:
    void SetUp() override {
        MemoryManagementFixture::SetUp();
        pPlatform = platform();
        pPlatform->initialize(numPlatformDevices, platformDevices);
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

        OCLRT::isGTPinInitialized = false;
    }

    void TearDown() override {
        ContextFixture::TearDown();
        pPlatform->shutdown();
        MemoryManagementFixture::TearDown();
        OCLRT::isGTPinInitialized = false;
    }

    Platform *pPlatform = nullptr;
    Device *pDevice = nullptr;
    cl_int retVal = CL_SUCCESS;
    GTPIN_DI_STATUS retFromGtPin = GTPIN_DI_SUCCESS;
    driver_services_t driverServices;
    gtpin::ocl::gtpin_events_t gtpinCallbacks;
};

typedef Test<GTPinFixture> GTPinTests;

TEST_F(GTPinTests, givenInvalidArgumentsThenGTPinInitFails) {
    retFromGtPin = GTPin_Init(nullptr, nullptr, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    retFromGtPin = GTPin_Init(&gtpinCallbacks, nullptr, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    retFromGtPin = GTPin_Init(nullptr, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);
}

TEST_F(GTPinTests, givenIncompleteArgumentsThenGTPinInitFails) {
    uint32_t ver = 0;

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
    uint32_t ver = 0;

    retFromGtPin = GTPin_Init(nullptr, nullptr, &ver);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_EQ(gtpin::ocl::GTPIN_OCL_INTERFACE_VERSION, ver);

    retFromGtPin = GTPin_Init(&gtpinCallbacks, nullptr, &ver);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_EQ(gtpin::ocl::GTPIN_OCL_INTERFACE_VERSION, ver);

    retFromGtPin = GTPin_Init(nullptr, &driverServices, &ver);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_EQ(gtpin::ocl::GTPIN_OCL_INTERFACE_VERSION, ver);
}

TEST_F(GTPinTests, givenValidAndCompleteArgumentsThenGTPinInitSucceeds) {
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferComplete = OnCommandBufferComplete;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    EXPECT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    EXPECT_EQ(&OCLRT::gtpinCreateBuffer, driverServices.bufferAllocate);
    EXPECT_EQ(&OCLRT::gtpinFreeBuffer, driverServices.bufferDeallocate);
    EXPECT_EQ(&OCLRT::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&OCLRT::gtpinUnmapBuffer, driverServices.bufferUnMap);
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
    EXPECT_EQ(&OCLRT::gtpinCreateBuffer, driverServices.bufferAllocate);
    EXPECT_EQ(&OCLRT::gtpinFreeBuffer, driverServices.bufferDeallocate);
    EXPECT_EQ(&OCLRT::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&OCLRT::gtpinUnmapBuffer, driverServices.bufferUnMap);

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
    ASSERT_EQ(&OCLRT::gtpinCreateBuffer, driverServices.bufferAllocate);
    EXPECT_EQ(&OCLRT::gtpinFreeBuffer, driverServices.bufferDeallocate);
    EXPECT_EQ(&OCLRT::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&OCLRT::gtpinUnmapBuffer, driverServices.bufferUnMap);

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
    EXPECT_EQ(&OCLRT::gtpinCreateBuffer, driverServices.bufferAllocate);
    ASSERT_EQ(&OCLRT::gtpinFreeBuffer, driverServices.bufferDeallocate);
    EXPECT_EQ(&OCLRT::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&OCLRT::gtpinUnmapBuffer, driverServices.bufferUnMap);

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
    EXPECT_EQ(&OCLRT::gtpinCreateBuffer, driverServices.bufferAllocate);
    EXPECT_EQ(&OCLRT::gtpinFreeBuffer, driverServices.bufferDeallocate);
    ASSERT_EQ(&OCLRT::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&OCLRT::gtpinUnmapBuffer, driverServices.bufferUnMap);

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
    EXPECT_EQ(&OCLRT::gtpinCreateBuffer, driverServices.bufferAllocate);
    EXPECT_EQ(&OCLRT::gtpinFreeBuffer, driverServices.bufferDeallocate);
    EXPECT_EQ(&OCLRT::gtpinMapBuffer, driverServices.bufferMap);
    ASSERT_EQ(&OCLRT::gtpinUnmapBuffer, driverServices.bufferUnMap);

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
        if (nonfailingAllocation != failureIndex) {
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
    ASSERT_EQ(&OCLRT::gtpinCreateBuffer, driverServices.bufferAllocate);
    ASSERT_EQ(&OCLRT::gtpinFreeBuffer, driverServices.bufferDeallocate);
    EXPECT_EQ(&OCLRT::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&OCLRT::gtpinUnmapBuffer, driverServices.bufferUnMap);

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
    ASSERT_EQ(&OCLRT::gtpinCreateBuffer, driverServices.bufferAllocate);
    ASSERT_EQ(&OCLRT::gtpinFreeBuffer, driverServices.bufferDeallocate);
    EXPECT_EQ(&OCLRT::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&OCLRT::gtpinUnmapBuffer, driverServices.bufferUnMap);

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
    ASSERT_EQ(&OCLRT::gtpinCreateBuffer, driverServices.bufferAllocate);
    ASSERT_EQ(&OCLRT::gtpinFreeBuffer, driverServices.bufferDeallocate);
    ASSERT_EQ(&OCLRT::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&OCLRT::gtpinUnmapBuffer, driverServices.bufferUnMap);

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
    ASSERT_EQ(&OCLRT::gtpinCreateBuffer, driverServices.bufferAllocate);
    ASSERT_EQ(&OCLRT::gtpinFreeBuffer, driverServices.bufferDeallocate);
    ASSERT_EQ(&OCLRT::gtpinMapBuffer, driverServices.bufferMap);
    EXPECT_EQ(&OCLRT::gtpinUnmapBuffer, driverServices.bufferUnMap);

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
    ASSERT_EQ(&OCLRT::gtpinCreateBuffer, driverServices.bufferAllocate);
    ASSERT_EQ(&OCLRT::gtpinFreeBuffer, driverServices.bufferDeallocate);
    ASSERT_EQ(&OCLRT::gtpinMapBuffer, driverServices.bufferMap);
    ASSERT_EQ(&OCLRT::gtpinUnmapBuffer, driverServices.bufferUnMap);

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

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    deleteDataReadFromFile(pSource);
}

TEST_F(GTPinTests, givenInitializedGTPinInterfaceThenGTPinKernelCreateCallbackIsCalled) {
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
    EXPECT_EQ(prevCount + 1, KernelCreateCallbackCount);

    Kernel *pKernel = (Kernel *)kernel;
    const KernelInfo &kInfo = pKernel->getKernelInfo();
    uint64_t gtpinKernelId = pKernel->getKernelId();
    EXPECT_EQ(kInfo.heapInfo.pKernelHeader->ShaderHashCode, gtpinKernelId);

    retVal = clReleaseKernel(kernel);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseProgram(pProgram);
    EXPECT_EQ(CL_SUCCESS, retVal);

    deleteDataReadFromFile(pSource);
}

} // namespace ULT
