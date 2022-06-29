/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/sharings/gl/gl_buffer.h"
#include "opencl/test/unit_test/mocks/gl/windows/mock_gl_sharing_windows.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using namespace NEO;

struct GlReusedBufferTests : public ::testing::Test {
    void SetUp() override {
        glSharingFunctions = new GlSharingFunctionsMock();

        context.setSharingFunctions(glSharingFunctions);
        rootDeviceIndex = context.getDevice(0)->getRootDeviceIndex();
        graphicsAllocationsForGlBufferReuse = &glSharingFunctions->graphicsAllocationsForGlBufferReuse;
    }

    GlSharingFunctionsMock *glSharingFunctions = nullptr;

    uint32_t rootDeviceIndex = 0;
    MockContext context;

    std::vector<std::pair<unsigned int, GraphicsAllocation *>> *graphicsAllocationsForGlBufferReuse = nullptr;
    unsigned int bufferId1 = 5;
    unsigned int bufferId2 = 7;
    cl_int retVal = CL_SUCCESS;
};

class FailingMemoryManager : public MockMemoryManager {
  public:
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation) override {
        return nullptr;
    }
};

TEST_F(GlReusedBufferTests, givenMultipleBuffersWithTheSameIdWhenCreatedThenReuseGraphicsAllocation) {
    std::unique_ptr<Buffer> glBuffers[10]; // first 5 with bufferId1, next 5 with bufferId2

    for (size_t i = 0; i < 10; i++) {
        glBuffers[i].reset(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, (i < 5 ? bufferId1 : bufferId2), &retVal));
        EXPECT_NE(nullptr, glBuffers[i].get());
        EXPECT_NE(nullptr, glBuffers[i]->getGraphicsAllocation(rootDeviceIndex));
    }

    EXPECT_EQ(2u, graphicsAllocationsForGlBufferReuse->size());
    EXPECT_EQ(bufferId1, graphicsAllocationsForGlBufferReuse->at(0).first);
    EXPECT_EQ(bufferId2, graphicsAllocationsForGlBufferReuse->at(1).first);

    auto storedGraphicsAllocation1 = graphicsAllocationsForGlBufferReuse->at(0).second;
    auto storedGraphicsAllocation2 = graphicsAllocationsForGlBufferReuse->at(1).second;
    EXPECT_EQ(5u, storedGraphicsAllocation1->peekReuseCount());
    EXPECT_EQ(5u, storedGraphicsAllocation2->peekReuseCount());

    for (size_t i = 0; i < 10; i++) {
        EXPECT_EQ(i < 5 ? storedGraphicsAllocation1 : storedGraphicsAllocation2,
                  glBuffers[i]->getGraphicsAllocation(rootDeviceIndex));
    }
}

TEST_F(GlReusedBufferTests, givenMultipleBuffersWithReusedAllocationWhenReleasingThenClearVectorByLastObject) {
    std::unique_ptr<Buffer> glBuffer1(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    std::unique_ptr<Buffer> glBuffer2(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));

    EXPECT_EQ(1u, graphicsAllocationsForGlBufferReuse->size());
    EXPECT_EQ(2u, graphicsAllocationsForGlBufferReuse->at(0).second->peekReuseCount());

    glBuffer1.reset(nullptr);

    EXPECT_EQ(1u, graphicsAllocationsForGlBufferReuse->size());
    EXPECT_EQ(1u, graphicsAllocationsForGlBufferReuse->at(0).second->peekReuseCount());

    glBuffer2.reset(nullptr);
    EXPECT_EQ(0u, graphicsAllocationsForGlBufferReuse->size());
}

TEST_F(GlReusedBufferTests, givenMultipleBuffersWithReusedAllocationWhenCreatingThenReuseGmmResourceToo) {
    std::unique_ptr<Buffer> glBuffer1(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    glBuffer1->getGraphicsAllocation(rootDeviceIndex)->setDefaultGmm(new Gmm(context.getDevice(0)->getGmmHelper(), (void *)0x100, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true));

    std::unique_ptr<Buffer> glBuffer2(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));

    EXPECT_EQ(glBuffer1->getGraphicsAllocation(rootDeviceIndex)->getDefaultGmm()->gmmResourceInfo->peekHandle(),
              glBuffer2->getGraphicsAllocation(rootDeviceIndex)->getDefaultGmm()->gmmResourceInfo->peekHandle());
}

TEST_F(GlReusedBufferTests, givenGlobalShareHandleChangedWhenAcquiringSharedBufferThenChangeGraphicsAllocation) {
    GlDllHelper dllParam;
    CL_GL_BUFFER_INFO bufferInfoOutput = dllParam.getBufferInfo();
    bufferInfoOutput.globalShareHandle = 40;
    dllParam.loadBuffer(bufferInfoOutput);
    auto clBuffer = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto glBuffer = clBuffer->peekSharingHandler();
    auto oldGraphicsAllocation = clBuffer->getGraphicsAllocation(rootDeviceIndex);

    ASSERT_EQ(40, oldGraphicsAllocation->peekSharedHandle());

    bufferInfoOutput.globalShareHandle = 41;
    dllParam.loadBuffer(bufferInfoOutput);
    glBuffer->acquire(clBuffer.get(), rootDeviceIndex);
    auto newGraphicsAllocation = clBuffer->getGraphicsAllocation(rootDeviceIndex);

    EXPECT_NE(oldGraphicsAllocation, newGraphicsAllocation);
    EXPECT_EQ(41, newGraphicsAllocation->peekSharedHandle());

    glBuffer->release(clBuffer.get(), rootDeviceIndex);
}

TEST_F(GlReusedBufferTests, givenGlobalShareHandleDidNotChangeWhenAcquiringSharedBufferThenDontDynamicallyAllocateBufferInfo) {
    class MyGlBuffer : public GlBuffer {
      public:
        MyGlBuffer(GLSharingFunctions *sharingFunctions, unsigned int glObjectId) : GlBuffer(sharingFunctions, glObjectId) {}

      protected:
        void resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData) override {
            EXPECT_EQ(nullptr, updateData->updateData);
            GlBuffer::resolveGraphicsAllocationChange(currentSharedHandle, updateData);
        }
    };
    GlDllHelper dllParam;
    CL_GL_BUFFER_INFO bufferInfoOutput = dllParam.getBufferInfo();
    bufferInfoOutput.globalShareHandle = 40;

    dllParam.loadBuffer(bufferInfoOutput);
    auto clBuffer = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto glBuffer = new MyGlBuffer(context.getSharing<GLSharingFunctions>(), bufferId1);
    clBuffer->setSharingHandler(glBuffer);

    glBuffer->acquire(clBuffer.get(), rootDeviceIndex);

    glBuffer->release(clBuffer.get(), rootDeviceIndex);
}

TEST_F(GlReusedBufferTests, givenGlobalShareHandleChangedWhenAcquiringSharedBufferThenDynamicallyAllocateBufferInfo) {
    class MyGlBuffer : public GlBuffer {
      public:
        MyGlBuffer(GLSharingFunctions *sharingFunctions, unsigned int glObjectId) : GlBuffer(sharingFunctions, glObjectId) {}

      protected:
        void resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData) override {
            EXPECT_NE(nullptr, updateData->updateData);
            GlBuffer::resolveGraphicsAllocationChange(currentSharedHandle, updateData);
        }
    };
    GlDllHelper dllParam;
    CL_GL_BUFFER_INFO bufferInfoOutput = dllParam.getBufferInfo();
    bufferInfoOutput.globalShareHandle = 40;
    dllParam.loadBuffer(bufferInfoOutput);
    auto clBuffer = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto glBuffer = new MyGlBuffer(context.getSharing<GLSharingFunctions>(), bufferId1);
    clBuffer->setSharingHandler(glBuffer);

    bufferInfoOutput.globalShareHandle = 41;
    dllParam.loadBuffer(bufferInfoOutput);
    glBuffer->acquire(clBuffer.get(), rootDeviceIndex);

    glBuffer->release(clBuffer.get(), rootDeviceIndex);
}

TEST_F(GlReusedBufferTests, givenMultipleBuffersAndGlobalShareHandleChangedWhenAcquiringSharedBufferThenDeleteOldGfxAllocationFromReuseVector) {
    GlDllHelper dllParam;
    CL_GL_BUFFER_INFO bufferInfoOutput = dllParam.getBufferInfo();
    bufferInfoOutput.globalShareHandle = 40;
    dllParam.loadBuffer(bufferInfoOutput);
    auto clBuffer1 = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto clBuffer2 = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto graphicsAllocation1 = clBuffer1->getGraphicsAllocation(rootDeviceIndex);
    auto graphicsAllocation2 = clBuffer2->getGraphicsAllocation(rootDeviceIndex);
    ASSERT_EQ(graphicsAllocation1, graphicsAllocation2);
    ASSERT_EQ(2u, graphicsAllocation1->peekReuseCount());
    ASSERT_EQ(1u, graphicsAllocationsForGlBufferReuse->size());

    bufferInfoOutput.globalShareHandle = 41;
    dllParam.loadBuffer(bufferInfoOutput);
    clBuffer1->peekSharingHandler()->acquire(clBuffer1.get(), rootDeviceIndex);
    auto newGraphicsAllocation = clBuffer1->getGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(1u, graphicsAllocationsForGlBufferReuse->size());
    EXPECT_EQ(newGraphicsAllocation, graphicsAllocationsForGlBufferReuse->at(0).second);

    clBuffer2->peekSharingHandler()->acquire(clBuffer2.get(), rootDeviceIndex);
    EXPECT_EQ(clBuffer2->getGraphicsAllocation(rootDeviceIndex), newGraphicsAllocation);
    EXPECT_EQ(1u, graphicsAllocationsForGlBufferReuse->size());
    EXPECT_EQ(newGraphicsAllocation, graphicsAllocationsForGlBufferReuse->at(0).second);

    clBuffer1->peekSharingHandler()->release(clBuffer1.get(), rootDeviceIndex);
    clBuffer2->peekSharingHandler()->release(clBuffer2.get(), rootDeviceIndex);
}

TEST_F(GlReusedBufferTests, givenGraphicsAllocationCreationReturnsNullptrWhenAcquiringGlBufferThenReturnOutOfResourcesAndNullifyAllocation) {
    auto suceedingMemoryManager = context.getMemoryManager();
    auto failingMemoryManager = std::unique_ptr<FailingMemoryManager>(new FailingMemoryManager());

    GlDllHelper dllParam;
    CL_GL_BUFFER_INFO bufferInfoOutput = dllParam.getBufferInfo();
    bufferInfoOutput.globalShareHandle = 40;

    dllParam.loadBuffer(bufferInfoOutput);
    auto clBuffer = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto glBuffer = clBuffer->peekSharingHandler();

    bufferInfoOutput.globalShareHandle = 41;
    dllParam.loadBuffer(bufferInfoOutput);
    context.memoryManager = failingMemoryManager.get();
    auto result = glBuffer->acquire(clBuffer.get(), rootDeviceIndex);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, result);
    EXPECT_EQ(nullptr, clBuffer->getGraphicsAllocation(rootDeviceIndex));

    context.memoryManager = suceedingMemoryManager;
}
