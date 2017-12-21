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
#include "runtime/helpers/options.h"
#include "runtime/mem_obj/buffer.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/memory_management_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "test.h"
#include "gtest/gtest.h"

using namespace OCLRT;
using namespace gtpin;

namespace OCLRT {
extern bool isGTPinInitialized;
}

namespace ULT {

void OnContextCreate(context_handle_t context, platform_info_t *platformInfo, igc_init_t **igcInit) {
}

void OnContextDestroy(context_handle_t context) {
}

void OnKernelCreate(context_handle_t context, const instrument_params_in_t *paramsIn, instrument_params_out_t *paramsOut) {
}

void OnDraw(gtpin::dx11::command_buffer_handle_t cb) {
}

void OnKernelSubmit(gtpin::dx11::command_buffer_handle_t cb, uint64_t kernelId, uint32_t *entryOffset, resource_handle_t *resource) {
}

void OnCommandBufferCreate(context_handle_t context, gtpin::dx11::command_buffer_handle_t cb) {
}

void OnCommandBufferSubmit(gtpin::dx11::command_buffer_handle_t cb, resource_handle_t *resource) {
}

class GTPinFixture : public ContextFixture {
    using ContextFixture::SetUp;

  public:
    GTPinFixture() {
    }

  protected:
    void SetUp() {
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
        gtpinCallbacks.onDraw = nullptr;
        gtpinCallbacks.onKernelSubmit = nullptr;
        gtpinCallbacks.onCommandBufferCreate = nullptr;
        gtpinCallbacks.onCommandBufferSubmit = nullptr;

        OCLRT::isGTPinInitialized = false;
    }

    void TearDown() override {
        ContextFixture::TearDown();
        pPlatform->shutdown();
    }

    Platform *pPlatform = nullptr;
    Device *pDevice = nullptr;
    cl_int retVal = CL_SUCCESS;
    GTPIN_DI_STATUS retFromGtPin = GTPIN_DI_SUCCESS;
    driver_services_t driverServices;
    gtpin::dx11::gtpin_events_t gtpinCallbacks;
};

typedef Test<GTPinFixture> GTPinTests;

TEST_F(GTPinTests, givenInvalidArgumentsThenGTPinInitFails) {
    retFromGtPin = GTPin_Init(nullptr, nullptr, nullptr);
    ASSERT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    retFromGtPin = GTPin_Init(&gtpinCallbacks, nullptr, nullptr);
    ASSERT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    retFromGtPin = GTPin_Init(nullptr, &driverServices, nullptr);
    ASSERT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);
}

TEST_F(GTPinTests, givenIncompleteArgumentsThenGTPinInitFails) {
    uint32_t ver = 0;

    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, &ver);
    ASSERT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    gtpinCallbacks.onContextCreate = OnContextCreate;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    ASSERT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    ASSERT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    ASSERT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    gtpinCallbacks.onDraw = OnDraw;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    ASSERT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    ASSERT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);

    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    ASSERT_EQ(GTPIN_DI_ERROR_INVALID_ARGUMENT, retFromGtPin);
}

TEST_F(GTPinTests, givenInvalidArgumentsWhenVersionArgumentIsProvidedThenGTPinInitReturnsDriverVersion) {
    uint32_t ver = 0;

    retFromGtPin = GTPin_Init(nullptr, nullptr, &ver);
    ASSERT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_NE(ver, 0u);

    retFromGtPin = GTPin_Init(&gtpinCallbacks, nullptr, &ver);
    ASSERT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_NE(ver, 0u);

    retFromGtPin = GTPin_Init(nullptr, &driverServices, &ver);
    ASSERT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_NE(ver, 0u);
}

TEST_F(GTPinTests, givenValidAndCompleteArgumentsThenGTPinInitSucceeds) {
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onDraw = OnDraw;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferSubmit = OnCommandBufferSubmit;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    ASSERT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_NE(nullptr, driverServices.bufferAllocate);
}

TEST_F(GTPinTests, givenValidAndCompleteArgumentsWhenGTPinIsAlreadyInitializedThenGTPinInitFails) {
    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onDraw = OnDraw;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferSubmit = OnCommandBufferSubmit;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    ASSERT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_NE(nullptr, driverServices.bufferAllocate);

    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    ASSERT_EQ(GTPIN_DI_ERROR_INSTANCE_ALREADY_CREATED, retFromGtPin);
}

TEST_F(GTPinTests, givenInvalidArgumentsThenBufferAllocateFails) {
    resource_handle_t res;

    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onDraw = OnDraw;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferSubmit = OnCommandBufferSubmit;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    ASSERT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_NE(nullptr, driverServices.bufferAllocate);

    retFromGtPin = (*driverServices.bufferAllocate)(nullptr, 400u, &res);
    ASSERT_NE(GTPIN_DI_SUCCESS, retFromGtPin);
}

TEST_F(GTPinTests, givenValidRequestForHugeMemoryAllocationThenBufferAllocateFails) {
    resource_handle_t res;

    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onDraw = OnDraw;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferSubmit = OnCommandBufferSubmit;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    ASSERT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_NE(nullptr, driverServices.bufferAllocate);

    BufferFuncs BufferFuncsBackup[IGFX_MAX_CORE];
    for (uint32_t i = 0; i < IGFX_MAX_CORE; i++) {
        BufferFuncsBackup[i] = bufferFactory[i];
        bufferFactory[i].createBufferFunction =
            [](Context *,
               cl_mem_flags,
               size_t,
               void *,
               void *,
               GraphicsAllocation *,
               bool,
               bool,
               bool)
            -> OCLRT::Buffer * { return nullptr; };
    }
    cl_context ctxt = (cl_context)((Context *)pContext);
    uint32_t hugeSize = 400u; // Will be handled as huge memory allocation
    retFromGtPin = (*driverServices.bufferAllocate)((gtpin::context_handle_t)ctxt, hugeSize, &res);
    for (uint32_t i = 0; i < IGFX_MAX_CORE; i++) {
        bufferFactory[i] = BufferFuncsBackup[i];
    }
    ASSERT_NE(GTPIN_DI_SUCCESS, retFromGtPin);
}

TEST_F(GTPinTests, givenValidRequestForMemoryAllocationThenBufferAllocateSucceeds) {
    resource_handle_t res;

    gtpinCallbacks.onContextCreate = OnContextCreate;
    gtpinCallbacks.onContextDestroy = OnContextDestroy;
    gtpinCallbacks.onKernelCreate = OnKernelCreate;
    gtpinCallbacks.onDraw = OnDraw;
    gtpinCallbacks.onKernelSubmit = OnKernelSubmit;
    gtpinCallbacks.onCommandBufferCreate = OnCommandBufferCreate;
    gtpinCallbacks.onCommandBufferSubmit = OnCommandBufferSubmit;
    retFromGtPin = GTPin_Init(&gtpinCallbacks, &driverServices, nullptr);
    ASSERT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_NE(nullptr, driverServices.bufferAllocate);

    cl_context ctxt = (cl_context)((Context *)pContext);
    retFromGtPin = (*driverServices.bufferAllocate)((gtpin::context_handle_t)ctxt, 400u, &res);
    ASSERT_EQ(GTPIN_DI_SUCCESS, retFromGtPin);
    ASSERT_NE(nullptr, res);

    cl_mem buffer = (cl_mem)res;
    retVal = clReleaseMemObject(buffer);
    ASSERT_EQ(retVal, CL_SUCCESS);
}

} // namespace ULT
