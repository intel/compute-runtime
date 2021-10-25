/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"

#include "opencl/source/mem_obj/image.h"
#include "opencl/source/sampler/sampler.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_parent_kernel_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"
#include "test.h"

using namespace NEO;

GEN9TEST_F(AUBParentKernelFixture, WhenEnqueuingParentKernelThenExpectationsMet) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(pClDevice);

    ASSERT_NE(nullptr, pKernel);
    ASSERT_TRUE(pKernel->isParentKernel);

    const cl_queue_properties properties[3] = {(CL_QUEUE_ON_DEVICE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE),
                                               0, 0};

    DeviceQueue *devQueue = DeviceQueue::create(
        &pCmdQ->getContext(),
        pClDevice,
        properties[0],
        retVal);

    SchedulerKernel &scheduler = pCmdQ->getContext().getSchedulerKernel();
    // Aub execution takes huge time for bigger GWS
    scheduler.setGws(24);

    size_t offset[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};
    size_t lws[3] = {1, 1, 1};

    // clang-format off
    cl_image_format imageFormat;
    imageFormat.image_channel_data_type = CL_UNSIGNED_INT8;
    imageFormat.image_channel_order = CL_R;

    cl_image_desc desc = { 0 };
    desc.image_array_size = 0;
    desc.image_depth = 1;
    desc.image_height = 4;
    desc.image_width = 4;
    desc.image_type = CL_MEM_OBJECT_IMAGE3D;
    desc.image_row_pitch = 0;
    desc.image_slice_pitch = 0;
    // clang-format on

    auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(0, 0, 0, &context->getDevice(0)->getDevice());
    auto surfaceFormat = Image::getSurfaceFormatFromTable(0, &imageFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
    Image *image = Image::create(
        pContext,
        memoryProperties,
        0,
        0,
        surfaceFormat,
        &desc,
        nullptr,
        retVal);

    Buffer *buffer = BufferHelper<BufferUseHostPtr<>>::create(pContext);

    cl_mem bufferMem = buffer;
    cl_mem imageMem = image;

    auto sampler = Sampler::create(
        pContext,
        CL_TRUE,
        CL_ADDRESS_NONE,
        CL_FILTER_LINEAR,
        retVal);

    uint64_t argScalar = 2;
    pKernel->setArg(
        3,
        sizeof(uint64_t),
        &argScalar);

    pKernel->setArg(
        2,
        sizeof(cl_mem),
        &bufferMem);

    pKernel->setArg(
        1,
        sizeof(cl_mem),
        &imageMem);

    pKernel->setArg(
        0,
        sizeof(cl_sampler),
        &sampler);

    pCmdQ->enqueueKernel(pKernel, 1, offset, gws, lws, 0, 0, 0);

    pCmdQ->finish();

    uint32_t expectedNumberOfEnqueues = 1;
    uint64_t gpuAddress = devQueue->getQueueBuffer()->getGpuAddress() + offsetof(IGIL_CommandQueue, m_controls.m_TotalNumberOfQueues);

    AUBCommandStreamFixture::expectMemory<FamilyType>(reinterpret_cast<void *>(gpuAddress), &expectedNumberOfEnqueues, sizeof(uint32_t));
    AUBCommandStreamFixture::expectMemory<FamilyType>(reinterpret_cast<void *>(buffer->getGraphicsAllocation(pClDevice->getRootDeviceIndex())->getGpuAddress()), &argScalar, sizeof(size_t));

    delete devQueue;
    delete image;
    delete buffer;
    delete sampler;
}
