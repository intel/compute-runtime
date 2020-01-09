/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm.h"
#include "core/gmm_helper/resource_info.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/sharings/gl/gl_buffer.h"
#include "test.h"
#include "unit_tests/mocks/gl/mock_gl_sharing.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_memory_manager.h"

using namespace NEO;

struct GlReusedBufferTests : public ::testing::Test {
    void SetUp() override {
        glSharingFunctions = new GlSharingFunctionsMock();

        context.setSharingFunctions(glSharingFunctions);
        graphicsAllocationsForGlBufferReuse = &glSharingFunctions->graphicsAllocationsForGlBufferReuse;
    }

    GlSharingFunctionsMock *glSharingFunctions = nullptr;

    MockContext context;

    std::vector<std::pair<unsigned int, GraphicsAllocation *>> *graphicsAllocationsForGlBufferReuse = nullptr;
    unsigned int bufferId1 = 5;
    unsigned int bufferId2 = 7;
    cl_int retVal = CL_SUCCESS;
};

class FailingMemoryManager : public MockMemoryManager {
  public:
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness) override {
        return nullptr;
    }
};

TEST_F(GlReusedBufferTests, givenMultipleBuffersWithTheSameIdWhenCreatedThenReuseGraphicsAllocation) {
    std::unique_ptr<Buffer> glBuffers[10]; // first 5 with bufferId1, next 5 with bufferId2

    for (size_t i = 0; i < 10; i++) {
        glBuffers[i].reset(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, (i < 5 ? bufferId1 : bufferId2), &retVal));
        EXPECT_NE(nullptr, glBuffers[i].get());
        EXPECT_NE(nullptr, glBuffers[i]->getGraphicsAllocation());
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
                  glBuffers[i]->getGraphicsAllocation());
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
    glBuffer1->getGraphicsAllocation()->setDefaultGmm(new Gmm(context.getDevice(0)->getExecutionEnvironment()->getGmmClientContext(), (void *)0x100, 1, false));

    std::unique_ptr<Buffer> glBuffer2(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));

    EXPECT_EQ(glBuffer1->getGraphicsAllocation()->getDefaultGmm()->gmmResourceInfo->peekHandle(),
              glBuffer2->getGraphicsAllocation()->getDefaultGmm()->gmmResourceInfo->peekHandle());
}

TEST_F(GlReusedBufferTests, givenGlobalShareHandleChangedWhenAcquiringSharedBufferThenChangeGraphicsAllocation) {
    std::unique_ptr<glDllHelper> dllParam = std::make_unique<glDllHelper>();
    CL_GL_BUFFER_INFO bufferInfoOutput = dllParam->getBufferInfo();
    bufferInfoOutput.globalShareHandle = 40;
    dllParam->loadBuffer(bufferInfoOutput);
    auto clBuffer = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto glBuffer = clBuffer->peekSharingHandler();
    auto oldGraphicsAllocation = clBuffer->getGraphicsAllocation();

    ASSERT_EQ(40, oldGraphicsAllocation->peekSharedHandle());

    bufferInfoOutput.globalShareHandle = 41;
    dllParam->loadBuffer(bufferInfoOutput);
    glBuffer->acquire(clBuffer.get());
    auto newGraphicsAllocation = clBuffer->getGraphicsAllocation();

    EXPECT_NE(oldGraphicsAllocation, newGraphicsAllocation);
    EXPECT_EQ(41, newGraphicsAllocation->peekSharedHandle());

    glBuffer->release(clBuffer.get());
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
    std::unique_ptr<glDllHelper> dllParam = std::make_unique<glDllHelper>();
    CL_GL_BUFFER_INFO bufferInfoOutput = dllParam->getBufferInfo();
    bufferInfoOutput.globalShareHandle = 40;

    dllParam->loadBuffer(bufferInfoOutput);
    auto clBuffer = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto glBuffer = new MyGlBuffer(context.getSharing<GLSharingFunctions>(), bufferId1);
    clBuffer->setSharingHandler(glBuffer);

    glBuffer->acquire(clBuffer.get());

    glBuffer->release(clBuffer.get());
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
    std::unique_ptr<glDllHelper> dllParam = std::make_unique<glDllHelper>();
    CL_GL_BUFFER_INFO bufferInfoOutput = dllParam->getBufferInfo();
    bufferInfoOutput.globalShareHandle = 40;
    dllParam->loadBuffer(bufferInfoOutput);
    auto clBuffer = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto glBuffer = new MyGlBuffer(context.getSharing<GLSharingFunctions>(), bufferId1);
    clBuffer->setSharingHandler(glBuffer);

    bufferInfoOutput.globalShareHandle = 41;
    dllParam->loadBuffer(bufferInfoOutput);
    glBuffer->acquire(clBuffer.get());

    glBuffer->release(clBuffer.get());
}

TEST_F(GlReusedBufferTests, givenMultipleBuffersAndGlobalShareHandleChangedWhenAcquiringSharedBufferDeleteOldGfxAllocationFromReuseVector) {
    std::unique_ptr<glDllHelper> dllParam = std::make_unique<glDllHelper>();
    CL_GL_BUFFER_INFO bufferInfoOutput = dllParam->getBufferInfo();
    bufferInfoOutput.globalShareHandle = 40;
    dllParam->loadBuffer(bufferInfoOutput);
    auto clBuffer1 = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto clBuffer2 = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto graphicsAllocation1 = clBuffer1->getGraphicsAllocation();
    auto graphicsAllocation2 = clBuffer2->getGraphicsAllocation();
    ASSERT_EQ(graphicsAllocation1, graphicsAllocation2);
    ASSERT_EQ(2, graphicsAllocation1->peekReuseCount());
    ASSERT_EQ(1, graphicsAllocationsForGlBufferReuse->size());

    bufferInfoOutput.globalShareHandle = 41;
    dllParam->loadBuffer(bufferInfoOutput);
    clBuffer1->peekSharingHandler()->acquire(clBuffer1.get());
    auto newGraphicsAllocation = clBuffer1->getGraphicsAllocation();
    EXPECT_EQ(1, graphicsAllocationsForGlBufferReuse->size());
    EXPECT_EQ(newGraphicsAllocation, graphicsAllocationsForGlBufferReuse->at(0).second);

    clBuffer2->peekSharingHandler()->acquire(clBuffer2.get());
    EXPECT_EQ(clBuffer2->getGraphicsAllocation(), newGraphicsAllocation);
    EXPECT_EQ(1, graphicsAllocationsForGlBufferReuse->size());
    EXPECT_EQ(newGraphicsAllocation, graphicsAllocationsForGlBufferReuse->at(0).second);

    clBuffer1->peekSharingHandler()->release(clBuffer1.get());
    clBuffer2->peekSharingHandler()->release(clBuffer2.get());
}

TEST_F(GlReusedBufferTests, givenGraphicsAllocationCreationReturnsNullptrWhenAcquiringGlBufferThenReturnOutOfResourcesAndNullifyAllocation) {
    auto suceedingMemoryManager = context.getMemoryManager();
    auto failingMemoryManager = std::unique_ptr<FailingMemoryManager>(new FailingMemoryManager());

    std::unique_ptr<glDllHelper> dllParam = std::make_unique<glDllHelper>();
    CL_GL_BUFFER_INFO bufferInfoOutput = dllParam->getBufferInfo();
    bufferInfoOutput.globalShareHandle = 40;

    dllParam->loadBuffer(bufferInfoOutput);
    auto clBuffer = std::unique_ptr<Buffer>(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId1, &retVal));
    auto glBuffer = clBuffer->peekSharingHandler();

    bufferInfoOutput.globalShareHandle = 41;
    dllParam->loadBuffer(bufferInfoOutput);
    context.memoryManager = failingMemoryManager.get();
    auto result = glBuffer->acquire(clBuffer.get());

    EXPECT_EQ(CL_OUT_OF_RESOURCES, result);
    EXPECT_EQ(nullptr, clBuffer->getGraphicsAllocation());

    context.memoryManager = suceedingMemoryManager;
}
