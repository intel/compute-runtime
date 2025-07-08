/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/command_stream/command_stream_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/helpers/cl_hw_parse.h"
#include "opencl/test/unit_test/indirect_heap/indirect_heap_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

#include "test_traits_common.h"

namespace NEO {

struct CommandDeviceFixture : public ClDeviceFixture,
                              public CommandQueueHwFixture {
    using CommandQueueHwFixture::setUp;
    void setUp(cl_command_queue_properties cmdQueueProperties = 0) {
        ClDeviceFixture::setUp();
        CommandQueueHwFixture::setUp(pClDevice, cmdQueueProperties);
    }

    void tearDown() {
        CommandQueueHwFixture::tearDown();
        ClDeviceFixture::tearDown();
    }
};

struct CommandEnqueueBaseFixture : CommandDeviceFixture,
                                   public IndirectHeapFixture,
                                   virtual public ClHardwareParse {
    using IndirectHeapFixture::setUp;
    void setUp(cl_command_queue_properties cmdQueueProperties = 0) {
        CommandDeviceFixture::setUp(cmdQueueProperties);
        IndirectHeapFixture::setUp(pCmdQ);
        ClHardwareParse::setUp();
    }

    void tearDown() {
        ClHardwareParse::tearDown();
        IndirectHeapFixture::tearDown();
        CommandDeviceFixture::tearDown();
    }
};

struct CommandEnqueueFixture : public CommandEnqueueBaseFixture,
                               public CommandStreamFixture {
    void setUp(cl_command_queue_properties cmdQueueProperties = 0) {
        CommandEnqueueBaseFixture::setUp(cmdQueueProperties);
        CommandStreamFixture::setUp(pCmdQ);
    }

    void tearDown() {
        CommandEnqueueBaseFixture::tearDown();
        CommandStreamFixture::tearDown();
    }
};

struct SurfaceStateAccessor : virtual public ClHardwareParse {
    template <typename FamilyType>
    const FamilyType::RENDER_SURFACE_STATE *getSurfaceState(std::unique_ptr<MockCommandQueueHw<FamilyType>> &mockCmdQ, uint32_t index) {
        typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

        const RENDER_SURFACE_STATE *surfaceState = nullptr;

        auto kernel = mockCmdQ->storedMultiDispatchInfo.begin()->getKernel();
        const auto &kernelInfo = kernel->getKernelInfo();
        if (kernelInfo.kernelDescriptor.kernelAttributes.imageAddressingMode == KernelDescriptor::AddressingMode::Bindless) {
            auto bindlessOffset = static_cast<uint32_t>(kernelInfo.getArgDescriptorAt(index).template as<ArgDescImage>().bindless);
            auto bindlessSurfaceStateIndex = kernel->getSurfaceStateIndexForBindlessOffset(bindlessOffset);
            void *surfaceStateAddress = ptrOffset(kernel->getSurfaceStateHeap(), bindlessSurfaceStateIndex * sizeof(RENDER_SURFACE_STATE));
            surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateAddress);
        } else {
            uint32_t bindfulIndex = static_cast<uint32_t>(kernelInfo.getArgDescriptorAt(index).template as<ArgDescImage>().bindful) / sizeof(RENDER_SURFACE_STATE);
            surfaceState = HardwareParse::getSurfaceState<FamilyType>(&mockCmdQ->getIndirectHeap(IndirectHeap::Type::surfaceState, 0), bindfulIndex);
        }

        return surfaceState;
    }
};

struct NegativeFailAllocationCommandEnqueueBaseFixture : public CommandEnqueueBaseFixture {
    void setUp() {
        CommandEnqueueBaseFixture::setUp();
        failMemManager.reset(new FailMemoryManager(*pDevice->getExecutionEnvironment()));

        BufferDefaults::context = context;
        Image2dDefaults::context = context;
        buffer.reset(BufferHelper<>::create());
        image.reset(ImageHelperUlt<Image2dDefaults>::create());
        ptr = static_cast<void *>(array);
        oldMemManager = pDevice->getExecutionEnvironment()->memoryManager.release();
        pDevice->injectMemoryManager(failMemManager.release());
    }

    void tearDown() {
        pDevice->injectMemoryManager(oldMemManager);
        buffer.reset(nullptr);
        image.reset(nullptr);
        BufferDefaults::context = nullptr;
        Image2dDefaults::context = nullptr;
        CommandEnqueueBaseFixture::tearDown();
    }

    std::unique_ptr<Buffer> buffer;
    std::unique_ptr<Image> image;
    std::unique_ptr<FailMemoryManager> failMemManager;
    char array[MemoryConstants::cacheLineSize];
    void *ptr;
    MemoryManager *oldMemManager;
};

template <typename FamilyType>
struct CommandQueueStateless : public CommandQueueHw<FamilyType> {
    CommandQueueStateless(Context *context, ClDevice *device) : CommandQueueHw<FamilyType>(context, device, nullptr, false){};

    void enqueueHandlerHook(const unsigned int commandType, const MultiDispatchInfo &dispatchInfo) override {
        auto kernel = dispatchInfo.begin()->getKernel();

        EXPECT_TRUE(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());
        if (kernel->getKernelInfo().getArgDescriptorAt(0).is<ArgDescriptor::argTPointer>()) {
            EXPECT_FALSE(kernel->getKernelInfo().getArgDescriptorAt(0).as<ArgDescPointer>().isPureStateful());
        }

        if (validateKernelSystemMemory) {
            if (expectedKernelSystemMemory) {
                EXPECT_TRUE(kernel->getDestinationAllocationInSystemMemory());
            } else {
                EXPECT_FALSE(kernel->getDestinationAllocationInSystemMemory());
            }
        }
    }

    bool validateKernelSystemMemory = false;
    bool expectedKernelSystemMemory = false;
};

template <typename FamilyType>
struct CommandQueueStateful : public CommandQueueHw<FamilyType> {
    CommandQueueStateful(Context *context, ClDevice *device) : CommandQueueHw<FamilyType>(context, device, nullptr, false){

                                                               };

    void enqueueHandlerHook(const unsigned int commandType, const MultiDispatchInfo &dispatchInfo) override {
        auto kernel = dispatchInfo.begin()->getKernel();
        EXPECT_FALSE(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.supportsBuffersBiggerThan4Gb());

        auto &gfxCoreHelper = kernel->getContext().getDevice(0)->getGfxCoreHelper();
        if (gfxCoreHelper.isStatelessToStatefulWithOffsetSupported()) {
            EXPECT_TRUE(kernel->allBufferArgsStateful);
        }

        if (validateKernelSystemMemory) {
            if (expectedKernelSystemMemory) {
                EXPECT_TRUE(kernel->getDestinationAllocationInSystemMemory());
            } else {
                EXPECT_FALSE(kernel->getDestinationAllocationInSystemMemory());
            }
        }
    }

    bool validateKernelSystemMemory = false;
    bool expectedKernelSystemMemory = false;
};

} // namespace NEO
