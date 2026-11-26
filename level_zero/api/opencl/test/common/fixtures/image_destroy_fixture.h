/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/helpers/leo_cl_memory_properties_helpers.h"
#include "level_zero/api/opencl/source/mem_obj/leo_buffer.h"
#include "level_zero/api/opencl/source/mem_obj/leo_image.h"
#include "level_zero/api/opencl/test/common/fixtures/capturing_context.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"

#include <memory>

namespace NEO {
namespace LEO {
namespace ult {

struct ImageDestructorTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        cl_device_id clDeviceId = clDevice;
        capturingContext = std::make_unique<CapturingContext>(driverHandle.get(), clDevice->getL0Handle());
        leoContext = std::make_unique<Context>(nullptr, capturingContext->toHandle(), 1, &clDeviceId, true);
    }

    void TearDown() override {
        leoContext.reset();
        capturingContext.reset();
        Test<OclFixture>::TearDown();
    }

    Buffer *createBuffer(void *usmPtr) {
        auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &clDevice->getDevice());
        return new Buffer(leoContext.get(), memoryProperties, CL_MEM_READ_WRITE, usmPtr, nullptr, sizeof(dummyStorage), false);
    }

    Image *createImageFrom(MemObj *parent) {
        auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &clDevice->getDevice());
        cl_image_format format{CL_R, CL_UNORM_INT8};
        // Null handle - the destructor then skips zeImageDestroy.
        return new Image(leoContext.get(), memoryProperties, CL_MEM_READ_WRITE, nullptr, nullptr, nullptr, false,
                         format, static_cast<cl_mem>(parent));
    }

    ClDevice *clDevice = nullptr;
    std::unique_ptr<CapturingContext> capturingContext;
    std::unique_ptr<Context> leoContext;

    uint64_t dummyStorage = 0u;
};

struct DestroyCountingL0Image : public L0::ImageImp {
    DestroyCountingL0Image(NEO::GraphicsAllocation *imageAllocation, uint32_t &destroyCount) : destroyCount(destroyCount) {
        this->allocation = imageAllocation;
    }

    ze_result_t initialize(L0::Device *device, const ze_image_desc_t *desc) override {
        return ZE_RESULT_SUCCESS;
    }

    void copySurfaceStateToSSH(void *surfaceStateHeap, uint32_t surfaceStateOffset, uint32_t bindlessSlot,
                               bool isMediaBlockArg, uint32_t mipLevel) override {}

    ze_result_t destroy() override {
        ++this->destroyCount;
        return L0::ImageImp::destroy();
    }

    uint32_t &destroyCount;
};

struct ImageDeferredDestroyTest : public ImageDestructorTest {
    void SetUp() override {
        ImageDestructorTest::SetUp();
        ultMemoryManager = static_cast<MockMemoryManager *>(neoDevice->getMemoryManager());
        ultMemoryManager->callBaseAllocInUse = true;
        auto &engine = neoDevice->getDefaultEngine();
        contextId = engine.osContext->getContextId();
        tagAddress = engine.commandStreamReceiver->getTagAddress();
        initialTagValue = *tagAddress;
        imageAllocation.updateTaskCount(initialTagValue + 1, contextId);
    }

    void TearDown() override {
        completeSubmission();
        leoContext.reset();
        *tagAddress = initialTagValue;
        ultMemoryManager->callBaseAllocInUse = false;
        ImageDestructorTest::TearDown();
    }

    Image *createImage(MemObj *parent) {
        auto memoryProperties = ClMemoryPropertiesHelper::createMemoryProperties(CL_MEM_READ_WRITE, 0, 0, &clDevice->getDevice());
        cl_image_format format{CL_R, CL_UNORM_INT8};
        auto l0Image = new DestroyCountingL0Image(&imageAllocation, destroyCount);
        return new Image(leoContext.get(), memoryProperties, CL_MEM_READ_WRITE, l0Image->toHandle(), nullptr, nullptr, false,
                         format, static_cast<cl_mem>(parent));
    }

    void completeSubmission() {
        *tagAddress = initialTagValue + 1;
    }

    MockGraphicsAllocation imageAllocation;
    MockMemoryManager *ultMemoryManager = nullptr;
    uint32_t contextId = 0u;
    volatile TagAddressType *tagAddress = nullptr;
    TagAddressType initialTagValue = 0u;
    uint32_t destroyCount = 0u;
};

} // namespace ult
} // namespace LEO
} // namespace NEO
