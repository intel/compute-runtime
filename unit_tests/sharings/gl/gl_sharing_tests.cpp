/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "runtime/device/device.h"
#include "runtime/event/user_event.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/mem_obj/image.h"
#include "runtime/os_interface/os_interface.h"
#include "runtime/platform/platform.h"
#include "runtime/sharings/gl/gl_arb_sync_event.h"
#include "runtime/sharings/gl/gl_buffer.h"
#include "runtime/sharings/gl/gl_sync_event.h"
#include "runtime/sharings/gl/gl_texture.h"
#include "runtime/sharings/sharing.h"
#include "test.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/libult/create_command_stream.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "unit_tests/mocks/gl/mock_gl_arb_sync_event.h"
#include "unit_tests/mocks/gl/mock_gl_sharing.h"
#include "unit_tests/mocks/mock_async_event_handler.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_event.h"
#include "unit_tests/mocks/mock_gmm_resource_info.h"
#include "unit_tests/mocks/mock_memory_manager.h"

#include "gl/gl_sharing_os.h"

using namespace OCLRT;
bool MockGLSharingFunctions::SharingEnabled = false;

class glSharingTests : public ::testing::Test {
  public:
    void SetUp() override {
        mockGlSharingFunctions = mockGlSharing->sharingFunctions.release();
        context.setSharingFunctions(mockGlSharingFunctions);

        mockGlSharing->m_bufferInfoOutput.globalShareHandle = bufferId;
        mockGlSharing->m_bufferInfoOutput.bufferSize = 4096u;
        mockGlSharing->uploadDataToBufferInfo();
        ASSERT_FALSE(overrideCommandStreamReceiverCreation);
    }

    MockContext context;
    std::unique_ptr<MockGlSharing> mockGlSharing = std::make_unique<MockGlSharing>();
    GlSharingFunctionsMock *mockGlSharingFunctions;
    unsigned int bufferId = 1u;
};

TEST_F(glSharingTests, givenGlMockWhenItIsCreatedThenNonZeroObjectIsReturned) {
    EXPECT_NE(nullptr, &mockGlSharing);
    EXPECT_NE(nullptr, &mockGlSharing->m_clGlResourceInfo);
    EXPECT_NE(nullptr, &mockGlSharing->m_glClResourceInfo);
}

TEST_F(glSharingTests, givenGLSharingFunctionsWhenAskedForIdThenClGlSharingIdIsReturned) {
    auto v = SharingType::CLGL_SHARING;
    EXPECT_EQ(v, mockGlSharingFunctions->getId());
}

TEST_F(glSharingTests, givenMockGlWhenGlBufferIsCreatedThenMemObjectHasGlHandler) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId, &retVal);

    EXPECT_NE(nullptr, glBuffer);
    EXPECT_NE(nullptr, glBuffer->getGraphicsAllocation());
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(4096u, glBuffer->getGraphicsAllocation()->getUnderlyingBufferSize());
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));

    EXPECT_EQ(bufferId, mockGlSharing->dllParam->getBufferInfo().bufferName);
    EXPECT_EQ(4096u, glBuffer->getSize());
    size_t flagsExpected = CL_MEM_READ_WRITE;
    EXPECT_EQ(flagsExpected, glBuffer->getFlags());

    auto handler = glBuffer->peekSharingHandler();
    ASSERT_NE(nullptr, handler);
    auto glHandler = static_cast<GlSharing *>(handler);

    EXPECT_EQ(glHandler->peekFunctionsHandler(), mockGlSharingFunctions);

    delete glBuffer;
}

class FailingMemoryManager : public MockMemoryManager {
  public:
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, bool requireSpecificBitness) override {
        return nullptr;
    }
};

TEST_F(glSharingTests, givenMockGlWhenGlBufferIsCreatedFromWrongHandleThenErrorAndNoBufferIsReturned) {
    auto tempMemoryManager = context.getMemoryManager();

    auto memoryManager = std::unique_ptr<FailingMemoryManager>(new FailingMemoryManager());
    context.setMemoryManager(memoryManager.get());

    auto retVal = CL_SUCCESS;
    auto glBuffer = GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, 0, &retVal);

    EXPECT_EQ(nullptr, glBuffer);
    EXPECT_EQ(CL_INVALID_GL_OBJECT, retVal);

    context.setMemoryManager(tempMemoryManager);
}

TEST_F(glSharingTests, givenContextWhenClCreateFromGlBufferIsCalledThenBufferIsReturned) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, glBuffer);

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(glSharingTests, givenContextWithoutSharingWhenClCreateFromGlBufferIsCalledThenErrorIsReturned) {
    context.resetSharingFunctions(CLGL_SHARING);
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
    ASSERT_EQ(nullptr, glBuffer);
}

GLboolean OSAPI mockGLAcquireSharedBuffer(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
    return GL_FALSE;
};

TEST_F(glSharingTests, givenContextWithSharingWhenClCreateFromGlBufferIsCalledWithIncorrectThenErrorIsReturned) {
    mockGlSharingFunctions->setGLAcquireSharedBufferMock(mockGLAcquireSharedBuffer);
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    ASSERT_EQ(CL_INVALID_GL_OBJECT, retVal);
    ASSERT_EQ(nullptr, glBuffer);
}

TEST_F(glSharingTests, givenContextAnd32BitAddressingWhenClCreateFromGlBufferIsCalledThenBufferIsReturned) {
    auto flagToRestore = DebugManager.flags.Force32bitAddressing.get();
    DebugManager.flags.Force32bitAddressing.set(true);

    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, glBuffer);

    EXPECT_TRUE(castToObject<Buffer>(glBuffer)->getGraphicsAllocation()->is32BitAllocation());

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    DebugManager.flags.Force32bitAddressing.set(flagToRestore);
}

TEST_F(glSharingTests, givenGlClBufferWhenAskedForCLGLGetInfoThenIdAndTypeIsReturned) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);

    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));

    cl_gl_object_type objectType = 0u;
    cl_GLuint objectId = 0u;

    retVal = clGetGLObjectInfo(glBuffer, &objectType, &objectId);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(objectType, (cl_gl_object_type)CL_GL_OBJECT_BUFFER);
    EXPECT_EQ(objectId, bufferId);

    retVal = clGetGLObjectInfo(glBuffer, &objectType, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(objectType, (cl_gl_object_type)CL_GL_OBJECT_BUFFER);

    retVal = clGetGLObjectInfo(glBuffer, nullptr, &objectId);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(objectId, bufferId);

    retVal = clGetGLObjectInfo(glBuffer, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(glSharingTests, givenClBufferWhenAskedForCLGLGetInfoThenErrorIsReturned) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateBuffer(&context, 0, 1, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_gl_object_type objectType = 0u;
    cl_GLuint objectId = 0u;

    retVal = clGetGLObjectInfo(glBuffer, &objectType, &objectId);
    EXPECT_EQ(CL_INVALID_GL_OBJECT, retVal);

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(glSharingTests, givenClGLBufferWhenItIsAcquiredThenAcuqireCountIsIncremented) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));

    auto memObject = castToObject<Buffer>(glBuffer);
    EXPECT_FALSE(memObject->isMemObjZeroCopy());

    EXPECT_FALSE(memObject->isReadWriteOnCpuAllowed(true, 0, (void *)0x1001, 100));
    auto currentGraphicsAllocation = memObject->getGraphicsAllocation();

    memObject->peekSharingHandler()->acquire(memObject);
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLGetCurrentContextCalled"));

    auto currentGraphicsAllocation2 = memObject->getGraphicsAllocation();

    EXPECT_EQ(currentGraphicsAllocation2, currentGraphicsAllocation);

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(glSharingTests, givenClGLBufferWhenItIsAcquiredTwiceThenAcuqireIsNotCalled) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto memObject = castToObject<MemObj>(glBuffer);

    memObject->peekSharingHandler()->acquire(memObject);
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLGetCurrentContextCalled"));

    memObject->peekSharingHandler()->acquire(memObject);
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLGetCurrentContextCalled"));

    memObject->peekSharingHandler()->release(memObject);
    memObject->peekSharingHandler()->release(memObject);
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLGetCurrentContextCalled"));

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(glSharingTests, givenClGLBufferWhenItIsCreatedAndGmmIsAvailableThenItIsUsedInGraphicsAllocation) {
    void *ptr = (void *)0x1000;
    auto gmm = new Gmm(ptr, 4096u, false);

    mockGlSharing->m_bufferInfoOutput.pGmmResInfo = gmm->gmmResourceInfo->peekHandle();
    mockGlSharing->uploadDataToBufferInfo();

    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto memObject = castToObject<MemObj>(glBuffer);

    auto graphicsAllocation = memObject->getGraphicsAllocation();
    ASSERT_NE(nullptr, graphicsAllocation->gmm);
    EXPECT_NE(nullptr, graphicsAllocation->gmm->gmmResourceInfo->peekHandle());

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    delete gmm;
}

TEST_F(glSharingTests, givenClGLBufferWhenItIsAcquiredTwiceAfterReleaseThenAcuqireIsIncremented) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto memObject = castToObject<MemObj>(glBuffer);

    memObject->peekSharingHandler()->acquire(memObject);
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLGetCurrentContextCalled"));

    memObject->peekSharingHandler()->release(memObject);

    memObject->peekSharingHandler()->acquire(memObject);
    EXPECT_EQ(3, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("GLGetCurrentContextCalled"));

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(glSharingTests, givenClGLBufferWhenItIsAcquireCountIsDecrementedToZeroThenCallReleaseFunction) {
    std::unique_ptr<Buffer> buffer(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId, nullptr));
    auto sharingHandler = buffer->peekSharingHandler();

    sharingHandler->acquire(buffer.get());
    sharingHandler->acquire(buffer.get());

    sharingHandler->release(buffer.get());
    EXPECT_EQ(0, mockGlSharing->dllParam->getParam("GLReleaseSharedBufferCalled"));

    sharingHandler->release(buffer.get());
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLReleaseSharedBufferCalled"));
    EXPECT_EQ(bufferId, mockGlSharing->dllParam->getBufferInfo().bufferName);
}

TEST_F(glSharingTests, givenClGLBufferWhenItIsAcquiredWithDifferentOffsetThenGraphicsAllocationContainsLatestOffsetValue) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto memObject = castToObject<MemObj>(glBuffer);

    auto graphicsAddress = memObject->getGraphicsAllocation()->getGpuAddress();

    mockGlSharing->m_bufferInfoOutput.bufferOffset = 50u;
    mockGlSharing->uploadDataToBufferInfo();

    memObject->peekSharingHandler()->acquire(memObject);

    auto offsetedGraphicsAddress = memObject->getGraphicsAllocation()->getGpuAddress();

    EXPECT_EQ(offsetedGraphicsAddress, graphicsAddress + mockGlSharing->m_bufferInfoOutput.bufferOffset);

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(glSharingTests, givenHwCommandQueueWhenAcquireIsCalledThenAcquireCountIsIncremented) {
    auto retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto buffer = castToObject<Buffer>(glBuffer);
    EXPECT_EQ(0u, buffer->acquireCount);

    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));
    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));
    EXPECT_EQ(1u, buffer->acquireCount);

    retVal = clEnqueueReleaseGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, buffer->acquireCount);

    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(3, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));
    EXPECT_EQ(1u, buffer->acquireCount);

    retVal = clReleaseCommandQueue(commandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(glSharingTests, givenHwCommandQueueWhenAcquireIsCalledWithIncorrectWaitlistThenReturnError) {
    auto retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto buffer = castToObject<Buffer>(glBuffer);
    EXPECT_EQ(0u, buffer->acquireCount);

    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));
    retVal = clEnqueueAcquireGLObjects(commandQueue, 0, &glBuffer, 1, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);

    retVal = clReleaseCommandQueue(commandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(glSharingTests, givenEnabledAsyncEventsHandlerWhenAcquireGlObjectsIsCalledWithIncompleteExternallySynchronizedEventThenItIsAddedToAsyncEventsHandler) {
    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(true);

    auto handler = new MockHandler(false);
    auto oldHandler = OCLRT::platform()->setAsyncEventsHandler(std::unique_ptr<AsyncEventsHandler>(handler));

    struct ExternallySynchronizedEvent : Event {
        ExternallySynchronizedEvent()
            : Event(nullptr, 0, 0, 0) {
        }

        bool isExternallySynchronized() const override {
            return true;
        }

        void updateExecutionStatus() override {
            ++updateCount;
            if (complete) {
                transitionExecutionStatus(CL_COMPLETE);
            }
        }

        bool complete = false;
        uint32_t updateCount = 0;
    };

    auto *event = new ExternallySynchronizedEvent;
    cl_event clEvent = static_cast<cl_event>(event);

    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, nullptr);

    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, nullptr);
    EXPECT_EQ(CL_SUCCESS, clEnqueueAcquireGLObjects(commandQueue, 1, &glBuffer, 1, &clEvent, nullptr));
    EXPECT_EQ(CL_SUCCESS, clReleaseCommandQueue(commandQueue));
    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(glBuffer));

    EXPECT_LT(CL_SUCCESS, event->peekExecutionStatus());
    EXPECT_FALSE(handler->peekIsRegisterListEmpty());

    uint32_t updateCount = event->updateCount;
    handler->process();
    EXPECT_LT(updateCount, event->updateCount);
    updateCount = event->updateCount;
    handler->process();
    EXPECT_LT(updateCount, event->updateCount);
    updateCount = event->updateCount;
    event->complete = true;
    handler->process();
    EXPECT_LE(updateCount, event->updateCount);
    updateCount = event->updateCount;
    handler->process();
    EXPECT_EQ(updateCount, event->updateCount);

    event->release();

    OCLRT::platform()->setAsyncEventsHandler(std::move(oldHandler));
}

TEST_F(glSharingTests, givenDisabledAsyncEventsHandlerWhenAcquireGlObjectsIsCalledWithIncompleteExternallySynchronizedEventThenItIsNotAddedToAsyncEventsHandler) {
    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);

    auto handler = new MockHandler(false);
    auto oldHandler = OCLRT::platform()->setAsyncEventsHandler(std::unique_ptr<AsyncEventsHandler>(handler));

    struct ExternallySynchronizedEvent : Event {
        ExternallySynchronizedEvent()
            : Event(nullptr, 0, 0, 0) {
        }

        bool isExternallySynchronized() const override {
            return true;
        }
    };

    auto *event = new ExternallySynchronizedEvent;
    cl_event clEvent = static_cast<cl_event>(event);

    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, nullptr);

    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, nullptr);
    EXPECT_EQ(CL_SUCCESS, clEnqueueAcquireGLObjects(commandQueue, 1, &glBuffer, 1, &clEvent, nullptr));
    EXPECT_EQ(CL_SUCCESS, clReleaseCommandQueue(commandQueue));
    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(glBuffer));

    EXPECT_LT(CL_SUCCESS, event->peekExecutionStatus());
    EXPECT_TRUE(handler->peekIsRegisterListEmpty());

    event->release();

    OCLRT::platform()->setAsyncEventsHandler(std::move(oldHandler));
}

TEST_F(glSharingTests, givenEnabledAsyncEventsHandlerWhenAcquireGlObjectsIsCalledWithIncompleteButNotExternallySynchronizedEventThenItIsNotAddedToAsyncEventsHandler) {
    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);

    auto handler = new MockHandler(false);
    auto oldHandler = OCLRT::platform()->setAsyncEventsHandler(std::unique_ptr<AsyncEventsHandler>(handler));

    auto *event = new UserEvent;
    cl_event clEvent = static_cast<cl_event>(event);

    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, nullptr);

    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, nullptr);
    EXPECT_EQ(CL_SUCCESS, clEnqueueAcquireGLObjects(commandQueue, 1, &glBuffer, 1, &clEvent, nullptr));
    EXPECT_EQ(CL_SUCCESS, clReleaseCommandQueue(commandQueue));
    EXPECT_EQ(CL_SUCCESS, clReleaseMemObject(glBuffer));

    EXPECT_LT(CL_SUCCESS, event->peekExecutionStatus());
    EXPECT_TRUE(handler->peekIsRegisterListEmpty());

    event->release();

    OCLRT::platform()->setAsyncEventsHandler(std::move(oldHandler));
}

TEST_F(glSharingTests, givenHwCommandQueueWhenReleaseIsCalledWithIncorrectWaitlistThenReturnError) {
    auto retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto buffer = castToObject<Buffer>(glBuffer);
    EXPECT_EQ(0u, buffer->acquireCount);

    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));
    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueReleaseGLObjects(commandQueue, 1, &glBuffer, 1, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);

    retVal = clEnqueueReleaseGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseCommandQueue(commandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(glSharingTests, givenContextWithoutSharingWhenAcquireIsCalledThenErrorIsReturned) {
    auto retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto buffer = castToObject<Buffer>(glBuffer);
    EXPECT_EQ(0u, buffer->acquireCount);

    context.releaseSharingFunctions(CLGL_SHARING);
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));
    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);

    context.setSharingFunctions(mockGlSharingFunctions);
    retVal = clReleaseCommandQueue(commandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(glSharingTests, givenContextWithoutSharingWhenReleaseIsCalledThenErrorIsReturned) {
    auto retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto buffer = castToObject<Buffer>(glBuffer);
    EXPECT_EQ(0u, buffer->acquireCount);

    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));
    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));
    EXPECT_EQ(1u, buffer->acquireCount);

    context.releaseSharingFunctions(CLGL_SHARING);
    retVal = clEnqueueReleaseGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);

    context.setSharingFunctions(mockGlSharingFunctions);
    retVal = clReleaseCommandQueue(commandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(glSharingTests, givenHwCommandQueueWhenAcquireAndReleaseCallsAreMadeWithEventsThenProperCmdTypeIsReturned) {
    cl_event retEvent;
    auto retVal = CL_SUCCESS;

    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, &retVal);
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_command_type cmdType = 0;
    size_t sizeReturned = 0;
    retVal = clGetEventInfo(retEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_ACQUIRE_GL_OBJECTS), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueReleaseGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, &retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clGetEventInfo(retEvent, CL_EVENT_COMMAND_TYPE, sizeof(cmdType), &cmdType, &sizeReturned);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_RELEASE_GL_OBJECTS), cmdType);
    EXPECT_EQ(sizeof(cl_command_type), sizeReturned);

    retVal = clReleaseEvent(retEvent);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseCommandQueue(commandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(glSharingTests, givenCommandQueueWhenReleaseGlObjectIsCalledThenFinishIsCalled) {
    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, nullptr);
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, nullptr);

    auto neoQueue = castToObject<CommandQueue>(commandQueue);
    clEnqueueAcquireGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, nullptr);
    neoQueue->taskCount = 5u;
    clEnqueueReleaseGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, nullptr);

    EXPECT_EQ(5u, neoQueue->latestTaskCountWaited);

    clReleaseCommandQueue(commandQueue);
    clReleaseMemObject(glBuffer);
}

TEST_F(glSharingTests, givenMockGLWhenFunctionsAreCalledThenCallsAreReceived) {
    auto ptrToStruct = &mockGlSharing->m_clGlResourceInfo;
    auto glDisplay = (GLDisplay)1;
    auto glContext = (GLContext)1;
    mockGlSharing->overrideGetCurrentValues(glContext, glDisplay);

    EXPECT_EQ(1u, mockGlSharingFunctions->setSharedOCLContextState());
    EXPECT_EQ(1u, mockGlSharingFunctions->acquireSharedBufferINTEL(ptrToStruct));
    EXPECT_EQ(1u, mockGlSharingFunctions->acquireSharedRenderBuffer(ptrToStruct));
    EXPECT_EQ(1u, mockGlSharingFunctions->acquireSharedTexture(ptrToStruct));
    EXPECT_EQ(1u, mockGlSharingFunctions->releaseSharedBufferINTEL(ptrToStruct));
    EXPECT_EQ(1u, mockGlSharingFunctions->releaseSharedRenderBuffer(ptrToStruct));
    EXPECT_EQ(1u, mockGlSharingFunctions->releaseSharedTexture(ptrToStruct));
    EXPECT_EQ(glContext, mockGlSharingFunctions->getCurrentContext());
    EXPECT_EQ(glDisplay, mockGlSharingFunctions->getCurrentDisplay());
    EXPECT_EQ(1u, mockGlSharingFunctions->makeCurrent(glContext, glDisplay));

    EXPECT_EQ(1, mockGlSharing->dllParam->getGLSetSharedOCLContextStateReturnedValue());
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLAcquireSharedBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLAcquireSharedRenderBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLAcquireSharedTextureCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLReleaseSharedBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLReleaseSharedRenderBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLReleaseSharedTextureCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLGetCurrentContextCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLGetCurrentDisplayCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLMakeCurrentCalled"));
}

TEST(glSharingBasicTest, GivenSharingFunctionsWhenItIsConstructedThenOglContextFunctionIsCalled) {
    GLType GLHDCType = 0;
    GLContext GLHGLRCHandle = 0;
    GLDisplay GLHDCHandle = 0;
    glDllHelper getDllParam;

    GlSharingFunctionsMock glSharingFunctions(GLHDCType, GLHGLRCHandle, GLHGLRCHandle, GLHDCHandle);
    EXPECT_EQ(1, getDllParam.getGLSetSharedOCLContextStateReturnedValue());
}

TEST(glSharingBasicTest, givenInvalidExtensionNameWhenCheckGLExtensionSupportedThenReturnFalse) {
    GLSharingFunctions glSharingFunctions;
    const unsigned char invalidExtension[] = "InvalidExtensionName";
    bool RetVal = glSharingFunctions.isOpenGlExtensionSupported(invalidExtension);
    EXPECT_FALSE(RetVal);
}

TEST(glSharingBasicTest, givenglGetIntegervIsNullWhenCheckGLExtensionSupportedThenReturnFalse) {
    MockGLSharingFunctions glSharingFunctions;
    glSharingFunctions.glGetIntegerv = nullptr;
    const unsigned char invalidExtension[] = "InvalidExtensionName";
    bool RetVal = glSharingFunctions.isOpenGlExtensionSupported(invalidExtension);
    EXPECT_FALSE(RetVal);
}

TEST(glSharingBasicTest, givenValidExtensionNameWhenCheckGLExtensionSupportedThenReturnTrue) {
    MockGLSharingFunctions glSharingFunctions;
    const unsigned char supportGLOES[] = "GL_OES_framebuffer_object";
    bool RetVal = glSharingFunctions.isOpenGlExtensionSupported(supportGLOES);
    EXPECT_TRUE(RetVal);
}

TEST(glSharingBasicTest, givenWhenCheckGLSharingSupportedThenReturnTrue) {
    MockGLSharingFunctions glSharingFunctions;
    bool RetVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_TRUE(RetVal);
}

TEST(glSharingBasicTest, givenVendorisNullWhenCheckGLSharingSupportedThenReturnFalse) {
    auto invalidGetStringFcn = [](GLenum name) {
        return (const GLubyte *)"";
    };

    MockGLSharingFunctions glSharingFunctions;
    glSharingFunctions.glGetString = invalidGetStringFcn;

    bool RetVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_FALSE(RetVal);
}

TEST(glSharingBasicTest, givenVersionisNullWhenCheckGLSharingSupportedThenReturnFalse) {

    MockGLSharingFunctions glSharingFunctions;
    glSharingFunctions.dllParam->glSetString("", GL_VERSION); // version returns null
    bool RetVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_FALSE(RetVal);
    glSharingFunctions.dllParam->glSetString("Int..", GL_VENDOR);
    RetVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_FALSE(RetVal);
}

TEST(glSharingBasicTest, givenVersionisGlesWhenCheckGLSharingSupportedThenReturnFalse) {
    MockGLSharingFunctions glSharingFunctions;

    glSharingFunctions.dllParam->glSetString("OpenGL ES", GL_VERSION);
    bool RetVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_TRUE(RetVal);

    glSharingFunctions.dllParam->glSetString("OpenGL ES 1.", GL_VERSION);
    RetVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_TRUE(RetVal);

    glSharingFunctions.dllParam->glSetString("2.0", GL_VERSION);
    RetVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_TRUE(RetVal);

    glSharingFunctions.dllParam->glSetStringi("GL_EXT_framebuffer_o...", 1);
    RetVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_FALSE(RetVal);

    glSharingFunctions.dllParam->glSetStringi("GL_EXT_framebuffer_object", 1);
    RetVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_TRUE(RetVal);

    glSharingFunctions.dllParam->glSetString("OpenGL ES 1.", GL_VERSION);
    glSharingFunctions.dllParam->glSetStringi("GL_OES_framebuffer_o...", 0);
    RetVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_FALSE(RetVal);
}

TEST(glSharingBasicTest, givensetSharedOCLContextStateWhenCallThenCorrectValue) {
    MockGLSharingFunctions glSharingFunctions;
    glSharingFunctions.dllParam->setGLSetSharedOCLContextStateReturnedValue(0u);
    EXPECT_EQ(0u, glSharingFunctions.setSharedOCLContextState());
    glSharingFunctions.dllParam->setGLSetSharedOCLContextStateReturnedValue(1u);
    EXPECT_EQ(1u, glSharingFunctions.setSharedOCLContextState());
}
TEST(glSharingBasicTest, givenGlSharingFunctionsWhenItIsConstructedThenFunctionsAreLoaded) {
    GLType GLHDCType = 0;
    GLContext GLHGLRCHandle = 0;
    GLDisplay GLHDCHandle = 0;

    GlSharingFunctionsMock glSharingFunctions(GLHDCType, GLHGLRCHandle, GLHGLRCHandle, GLHDCHandle);

    EXPECT_NE(nullptr, glSharingFunctions.GLGetCurrentContext);
    EXPECT_NE(nullptr, glSharingFunctions.GLGetCurrentDisplay);
    EXPECT_NE(nullptr, glSharingFunctions.glGetString);
    EXPECT_NE(nullptr, glSharingFunctions.glGetIntegerv);
    EXPECT_NE(nullptr, glSharingFunctions.pfnWglCreateContext);
    EXPECT_NE(nullptr, glSharingFunctions.pfnWglDeleteContext);
    EXPECT_NE(nullptr, glSharingFunctions.pfnWglShareLists);
    EXPECT_NE(nullptr, glSharingFunctions.wglMakeCurrent);
    EXPECT_NE(nullptr, glSharingFunctions.GLSetSharedOCLContextState);
    EXPECT_NE(nullptr, glSharingFunctions.GLAcquireSharedBuffer);
    EXPECT_NE(nullptr, glSharingFunctions.GLReleaseSharedBuffer);
    EXPECT_NE(nullptr, glSharingFunctions.GLAcquireSharedRenderBuffer);
    EXPECT_NE(nullptr, glSharingFunctions.GLReleaseSharedRenderBuffer);
    EXPECT_NE(nullptr, glSharingFunctions.GLAcquireSharedTexture);
    EXPECT_NE(nullptr, glSharingFunctions.GLReleaseSharedTexture);
    EXPECT_NE(nullptr, glSharingFunctions.GLRetainSync);
    EXPECT_NE(nullptr, glSharingFunctions.GLReleaseSync);
    EXPECT_NE(nullptr, glSharingFunctions.GLGetSynciv);
    EXPECT_NE(nullptr, glSharingFunctions.glGetStringi);
}
TEST_F(glSharingTests, givenContextWhenCreateFromSharedBufferThenSharedImageIsReturned) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, glBuffer);
    auto parentbBuffer = castToObject<Buffer>(glBuffer);

    cl_image_format format = {CL_RGBA, CL_FLOAT};
    cl_image_desc image_desc = {CL_MEM_OBJECT_IMAGE1D_BUFFER, 1, 1, 1, 1, 0, 0, 0, 0, {glBuffer}};
    cl_mem image = clCreateImage(&context, CL_MEM_READ_WRITE, &format, &image_desc, 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, glBuffer);
    auto childImage = castToObject<Image>(image);

    EXPECT_EQ(parentbBuffer->peekSharingHandler(), childImage->peekSharingHandler());

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(glSharingTests, givenClGLBufferWhenMapAndUnmapBufferIsCalledThenCopyOnGpu) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto buffer = castToObject<Buffer>(glBuffer);
    EXPECT_EQ(buffer->getCpuAddressForMemoryTransfer(), nullptr); // no cpu ptr

    auto commandQueue = CommandQueue::create(&context, context.getDevice(0), 0, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t offset = 1;
    auto taskCount = commandQueue->taskCount;
    auto mappedPtr = clEnqueueMapBuffer(commandQueue, glBuffer, CL_TRUE, CL_MAP_WRITE, offset, (buffer->getSize() - offset),
                                        0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(taskCount + 1, commandQueue->taskCount);
    MapInfo mapInfo;
    EXPECT_TRUE(buffer->findMappedPtr(mappedPtr, mapInfo));
    EXPECT_EQ(mappedPtr, ptrOffset(buffer->getAllocatedMapPtr(), offset));
    EXPECT_EQ(mapInfo.size[0], buffer->getSize() - offset);
    EXPECT_EQ(mapInfo.offset[0], offset);

    retVal = commandQueue->enqueueUnmapMemObject(buffer, mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(taskCount + 2, commandQueue->taskCount);

    EXPECT_FALSE(buffer->findMappedPtr(mappedPtr, mapInfo)); // delete in destructor

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    delete commandQueue;
}

TEST_F(glSharingTests, givenClGLBufferWhenretValIsNotPassedToCreateFunctionThenBufferIsCreated) {
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, nullptr);
    ASSERT_NE(nullptr, glBuffer);
    auto retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(glSharingTests, givenClGLBufferWhenMapAndUnmapBufferIsCalledTwiceThenReuseStorage) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto buffer = castToObject<Buffer>(glBuffer);
    EXPECT_EQ(buffer->getCpuAddressForMemoryTransfer(), nullptr); // no cpu ptr

    auto commandQueue = CommandQueue::create(&context, context.getDevice(0), 0, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto mappedPtr = clEnqueueMapBuffer(commandQueue, glBuffer, CL_TRUE, CL_MAP_READ, 0, buffer->getSize(),
                                        0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clEnqueueUnmapMemObject(commandQueue, glBuffer, mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto mappedPtr2 = clEnqueueMapBuffer(commandQueue, glBuffer, CL_TRUE, CL_MAP_READ, 0, buffer->getSize(),
                                         0, nullptr, nullptr, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(mappedPtr, mappedPtr2);

    retVal = clEnqueueUnmapMemObject(commandQueue, glBuffer, mappedPtr, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    delete commandQueue;
}

TEST(APIclCreateEventFromGLsyncKHR, givenInvalidContexWhenCreateThenReturnError) {
    cl_int retVal = CL_SUCCESS;
    cl_GLsync sync = {0};

    auto event = clCreateEventFromGLsyncKHR(nullptr, sync, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, event);
}

TEST_F(glSharingTests, givenContextWithoutSharingWhenCreateEventFromGLThenErrorIsReturned) {
    context.resetSharingFunctions(CLGL_SHARING);
    cl_int retVal = CL_SUCCESS;
    cl_GLsync sync = {0};
    auto event = clCreateEventFromGLsyncKHR(&context, sync, &retVal);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
    EXPECT_EQ(nullptr, event);
}

TEST(glSharingContextSwitch, givenContextOrBkpContextHandleAsCurrentWhenSwitchAttemptedThenDontMakeSwitch) {
    GLType type = 0;
    auto context = (GLContext)1;
    auto display = (GLDisplay)2;
    auto bkpContext = (GLContext)3;
    MockGlSharing mockGlSharing(type, context, bkpContext, display);
    mockGlSharing.overrideGetCurrentValues(context, display);

    {
        GLContextGuard guard(*mockGlSharing.sharingFunctions);
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().currentContext == context);
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().currentDisplay == display);
    }

    EXPECT_EQ(0, mockGlSharing.dllParam->getParam("GLMakeCurrentCalled"));
    EXPECT_EQ(1, mockGlSharing.dllParam->getParam("GLGetCurrentContextCalled"));
    EXPECT_EQ(1, mockGlSharing.dllParam->getParam("GLGetCurrentDisplayCalled"));

    mockGlSharing.overrideGetCurrentValues(bkpContext, display);
    {
        GLContextGuard guard(*mockGlSharing.sharingFunctions);
        EXPECT_EQ(0, mockGlSharing.dllParam->getParam("GLMakeCurrentCalled"));
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().currentContext == bkpContext);
    }

    EXPECT_EQ(1, mockGlSharing.dllParam->getParam("GLMakeCurrentCalled")); // destructor
    EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().madeCurrentContext == bkpContext);
}

TEST(glSharingContextSwitch, givenUnknownCurrentContextAndNoFailsOnCallWhenSwitchAttemptedThenMakeSwitchToCtxHandle) {
    GLType type = 0;
    auto context = (GLContext)1;
    auto display = (GLDisplay)2;
    auto bkpContext = (GLContext)3;
    auto unknownContext = (GLContext)4;
    MockGlSharing mockGlSharing(type, context, bkpContext, display);
    mockGlSharing.overrideGetCurrentValues(unknownContext, display, false);

    {
        GLContextGuard guard(*mockGlSharing.sharingFunctions);
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().currentContext == unknownContext);
        EXPECT_EQ(1, mockGlSharing.dllParam->getParam("GLMakeCurrentCalled"));
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().madeCurrentContext == context);
    }
    EXPECT_EQ(2, mockGlSharing.dllParam->getParam("GLMakeCurrentCalled")); // destructor
    EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().madeCurrentContext == unknownContext);
}

TEST(glSharingContextSwitch, givenUnknownCurrentContextAndOneFailOnCallWhenSwitchAttemptedThenMakeSwitchToBkpCtxHandle) {
    GLType type = 0;
    auto context = (GLContext)1;
    auto display = (GLDisplay)2;
    auto bkpContext = (GLContext)3;
    auto unknownContext = (GLContext)4;
    MockGlSharing mockGlSharing(type, context, bkpContext, display);
    mockGlSharing.overrideGetCurrentValues(unknownContext, display, true, 1);

    {
        GLContextGuard guard(*mockGlSharing.sharingFunctions);
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().currentContext == unknownContext);
        EXPECT_EQ(2, mockGlSharing.dllParam->getParam("GLMakeCurrentCalled"));
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().madeCurrentContext == bkpContext);
    }
    EXPECT_EQ(3, mockGlSharing.dllParam->getParam("GLMakeCurrentCalled")); // destructor
    EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().madeCurrentContext == unknownContext);
}

TEST(glSharingContextSwitch, givenUnknownCurrentContextAndMultipleFailOnCallWhenSwitchAttemptedThenMakeSwitchToBkpCtxHandleUntilSuccess) {
    GLType type = 0;
    auto context = (GLContext)1;
    auto display = (GLDisplay)2;
    auto bkpContext = (GLContext)3;
    auto unknownContext = (GLContext)4;
    MockGlSharing mockGlSharing(type, context, bkpContext, display);
    mockGlSharing.overrideGetCurrentValues(unknownContext, display, true, 5);

    {
        GLContextGuard guard(*mockGlSharing.sharingFunctions);
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().currentContext == unknownContext);
        EXPECT_EQ(6, mockGlSharing.dllParam->getParam("GLMakeCurrentCalled"));
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().madeCurrentContext == bkpContext);
    }
    EXPECT_EQ(7, mockGlSharing.dllParam->getParam("GLMakeCurrentCalled")); // destructor
    EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().madeCurrentContext == unknownContext);
}

TEST(glSharingContextSwitch, givenZeroCurrentContextWhenSwitchAttemptedThenMakeSwitchToBkpCtxHandle) {
    GLType type = 0;
    auto context = (GLContext)1;
    auto display = (GLDisplay)2;
    auto bkpContext = (GLContext)3;
    auto zeroContext = (GLContext)0;

    MockGlSharing mockGlSharing(type, context, bkpContext, display);
    mockGlSharing.overrideGetCurrentValues(zeroContext, display, false);

    {
        GLContextGuard guard(*mockGlSharing.sharingFunctions);
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().currentContext == zeroContext);
        EXPECT_EQ(1, mockGlSharing.dllParam->getParam("GLMakeCurrentCalled"));
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().madeCurrentContext == bkpContext);
    }
    EXPECT_EQ(2, mockGlSharing.dllParam->getParam("GLMakeCurrentCalled")); // destructor
    EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().madeCurrentContext == zeroContext);
}

TEST(glSharingContextSwitch, givenSharingFunctionsWhenGlDeleteContextIsNotPresentThenItIsNotCalled) {
    auto glSharingFunctions = new GLSharingFunctions();
    glDllHelper dllParam;
    auto currentGlDeleteContextCalledCount = dllParam.getParam("GLDeleteContextCalled");
    delete glSharingFunctions;
    EXPECT_EQ(currentGlDeleteContextCalledCount, dllParam.getParam("GLDeleteContextCalled"));
}

HWTEST_F(glSharingTests, givenSyncObjectWhenCreateEventIsCalledThenCreateGLSyncObj) {
    cl_int retVal = CL_SUCCESS;
    GLsync glSync = {0};
    auto event = clCreateEventFromGLsyncKHR(&context, glSync, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, event);

    auto &csr = reinterpret_cast<MockDevice *>(context.getDevice(0))->getUltCommandStreamReceiver<FamilyType>();
    csr.taskLevel = 123;
    auto eventObj = castToObject<Event>(event);
    EXPECT_TRUE(eventObj->getCommandType() == CL_COMMAND_GL_FENCE_SYNC_OBJECT_KHR);
    EXPECT_TRUE(eventObj->peekExecutionStatus() == CL_SUBMITTED);
    EXPECT_EQ(Event::eventNotReady, eventObj->taskLevel);
    EXPECT_EQ(Event::eventNotReady, eventObj->getTaskLevel());
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLRetainSyncCalled"));

    eventObj->setStatus(CL_COMPLETE);
    EXPECT_EQ(0u, eventObj->getTaskLevel());
    clReleaseEvent(event);
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLReleaseSyncCalled"));
}

HWTEST_F(glSharingTests, givenEventCreatedFromFenceObjectWhenItIsPassedToAcquireThenItsStatusIsUpdated) {
    GLsync glSync = {0};
    auto retVal = CL_SUCCESS;
    auto event = clCreateEventFromGLsyncKHR(&context, glSync, &retVal);
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, &retVal);

    mockGlSharing->setGetSyncivReturnValue(GL_SIGNALED);

    auto neoEvent = castToObject<Event>(event);
    EXPECT_FALSE(neoEvent->isReadyForSubmission());

    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glBuffer, 1, &event, nullptr);
    EXPECT_TRUE(neoEvent->isReadyForSubmission());

    EXPECT_EQ(CL_SUCCESS, retVal);

    clReleaseCommandQueue(commandQueue);
    clReleaseMemObject(glBuffer);
    clReleaseEvent(event);
}

TEST_F(glSharingTests, glSyncEventReportsAsExternallySynchronized) {
    GLsync glSync = {0};
    auto syncEvent = GlSyncEvent::create(context, glSync, nullptr);
    ASSERT_NE(nullptr, syncEvent);
    EXPECT_TRUE(syncEvent->isExternallySynchronized());
    syncEvent->release();
}

TEST_F(glSharingTests, givenSyncEventWhenUpdateExecutionStatusIsCalledThenGLGetSyncivCalled) {
    GLsync glSync = {0};
    auto syncEvent = GlSyncEvent::create(context, glSync, nullptr);
    ASSERT_NE(nullptr, syncEvent);

    mockGlSharing->setGetSyncivReturnValue(GL_UNSIGNALED);
    syncEvent->updateExecutionStatus();
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("GLGetSyncivCalled"));
    EXPECT_TRUE(syncEvent->updateEventAndReturnCurrentStatus() == CL_SUBMITTED);
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("GLGetSyncivCalled")); // updateExecutionStatus called in peekExecutionStatus

    mockGlSharing->setGetSyncivReturnValue(GL_SIGNALED);
    syncEvent->updateExecutionStatus();
    EXPECT_EQ(3, mockGlSharing->dllParam->getParam("GLGetSyncivCalled"));
    EXPECT_TRUE(syncEvent->peekExecutionStatus() == CL_COMPLETE);

    delete syncEvent;
}

TEST_F(glSharingTests, givenContextWhenEmptySharingTableEmptyThenReturnsNullptr) {
    MockContext context;
    context.clearSharingFunctions();
    GLSharingFunctions *sharingF = context.getSharing<GLSharingFunctions>();
    EXPECT_EQ(sharingF, nullptr);
}

TEST_F(glSharingTests, givenUnknownBaseEventWhenGetGlArbSyncEventIsCalledThenNullptrIsReturned) {
    auto *sharing = context.getSharing<GLSharingFunctions>();
    ASSERT_NE(nullptr, sharing);
    auto event = new MockEvent<UserEvent>();
    MockContext context;
    EXPECT_EQ(nullptr, sharing->getGlArbSyncEvent(*event));
    event->release();
}

TEST_F(glSharingTests, givenKnownBaseEventWhenGetGlArbSyncEventIsCalledThenProperArbEventIsReturned) {
    auto *sharing = static_cast<GlSharingFunctionsMock *>(context.getSharing<GLSharingFunctions>());
    ASSERT_NE(nullptr, sharing);
    auto baseEvent = new MockEvent<UserEvent>;
    auto arbSyncEvent = reinterpret_cast<GlArbSyncEvent *>(0x1c);
    sharing->glArbEventMapping[baseEvent] = arbSyncEvent;
    EXPECT_EQ(arbSyncEvent, sharing->getGlArbSyncEvent(*baseEvent));
    baseEvent->release();
}

TEST_F(glSharingTests, givenKnownBaseEventWhenRemoveGlArbSyncEventMappingIsCalledThenProperArbEventIsRemovedFromMap) {
    auto *sharing = static_cast<GlSharingFunctionsMock *>(context.getSharing<GLSharingFunctions>());
    ASSERT_NE(nullptr, sharing);
    auto baseEvent = new MockEvent<UserEvent>;
    auto arbSyncEvent = reinterpret_cast<GlArbSyncEvent *>(0x1c);
    sharing->glArbEventMapping[baseEvent] = arbSyncEvent;
    EXPECT_NE(sharing->glArbEventMapping.end(), sharing->glArbEventMapping.find(baseEvent));
    sharing->removeGlArbSyncEventMapping(*baseEvent);
    EXPECT_EQ(sharing->glArbEventMapping.end(), sharing->glArbEventMapping.find(baseEvent));
    baseEvent->release();
}

TEST_F(glSharingTests, givenUnknownBaseEventWhenRemoveGlArbSyncEventMappingIsCalledThenProperArbEventIsRemovedFromMap) {
    auto *sharing = static_cast<GlSharingFunctionsMock *>(context.getSharing<GLSharingFunctions>());
    ASSERT_NE(nullptr, sharing);
    auto baseEvent = new MockEvent<UserEvent>;
    auto unknownBaseEvent = new MockEvent<UserEvent>;
    auto arbSyncEvent = reinterpret_cast<GlArbSyncEvent *>(0x1c);
    sharing->glArbEventMapping[baseEvent] = arbSyncEvent;
    EXPECT_NE(sharing->glArbEventMapping.end(), sharing->glArbEventMapping.find(baseEvent));
    EXPECT_EQ(sharing->glArbEventMapping.end(), sharing->glArbEventMapping.find(unknownBaseEvent));
    sharing->removeGlArbSyncEventMapping(*unknownBaseEvent);
    EXPECT_NE(sharing->glArbEventMapping.end(), sharing->glArbEventMapping.find(baseEvent));
    EXPECT_EQ(sharing->glArbEventMapping.end(), sharing->glArbEventMapping.find(unknownBaseEvent));
    unknownBaseEvent->release();
    baseEvent->release();
}

TEST_F(glSharingTests, givenUnknownBaseEventWhenGetOrCreateGlArbSyncEventIsCalledThenNewArbEventIsReturned) {
    auto *sharing = static_cast<GlSharingFunctionsMock *>(context.getSharing<GLSharingFunctions>());
    sharing->pfnGlArbSyncObjectCleanup = glArbSyncObjectCleanupMockDoNothing;
    ASSERT_NE(nullptr, sharing);

    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, nullptr);
    ASSERT_NE(nullptr, commandQueue);
    auto baseEvent = new Event(castToObjectOrAbort<CommandQueue>(commandQueue), CL_COMMAND_RELEASE_GL_OBJECTS, -1, -1);
    auto syncEv = sharing->getOrCreateGlArbSyncEvent<DummyArbEvent<false>>(*baseEvent);
    ASSERT_NE(nullptr, syncEv);
    EXPECT_NE(nullptr, syncEv->getSyncInfo());

    std::unique_ptr<OSInterface> osInterface{new OSInterface};
    static_cast<DummyArbEvent<false> *>(syncEv)->osInterface = osInterface.get();
    syncEv->release();

    clReleaseCommandQueue(commandQueue);
}

TEST_F(glSharingTests, givenKnownBaseEventWhenGetOrCreateGlArbSyncEventIsCalledThenOldArbEventIsReused) {
    auto *sharing = static_cast<GlSharingFunctionsMock *>(context.getSharing<GLSharingFunctions>());
    sharing->pfnGlArbSyncObjectCleanup = glArbSyncObjectCleanupMockDoNothing;
    ASSERT_NE(nullptr, sharing);

    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, nullptr);
    ASSERT_NE(nullptr, commandQueue);
    auto baseEvent = new Event(castToObjectOrAbort<CommandQueue>(commandQueue), CL_COMMAND_RELEASE_GL_OBJECTS, -1, -1);
    auto syncEv = sharing->getOrCreateGlArbSyncEvent<DummyArbEvent<false>>(*baseEvent);
    ASSERT_NE(nullptr, syncEv);
    EXPECT_EQ(syncEv, sharing->getOrCreateGlArbSyncEvent<DummyArbEvent<false>>(*baseEvent));

    std::unique_ptr<OSInterface> osInterface{new OSInterface};
    static_cast<DummyArbEvent<false> *>(syncEv)->osInterface = osInterface.get();
    syncEv->release();

    clReleaseCommandQueue(commandQueue);
}

TEST_F(glSharingTests, WhenArbSyncEventCreationFailsThenGetOrCreateGlArbSyncEventReturnsNull) {
    auto *sharing = static_cast<GlSharingFunctionsMock *>(context.getSharing<GLSharingFunctions>());
    ASSERT_NE(nullptr, sharing);

    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, nullptr);
    ASSERT_NE(nullptr, commandQueue);
    auto baseEvent = new Event(castToObjectOrAbort<CommandQueue>(commandQueue), CL_COMMAND_RELEASE_GL_OBJECTS, -1, -1);
    auto syncEv = sharing->getOrCreateGlArbSyncEvent<DummyArbEvent<true>>(*baseEvent);
    EXPECT_EQ(nullptr, syncEv);
    baseEvent->release();

    clReleaseCommandQueue(commandQueue);
}

TEST_F(glSharingTests, whenGetGlDeviceHandleIsCalledThenProperHandleIsReturned) {
    auto *sharing = static_cast<GlSharingFunctionsMock *>(context.getSharing<GLSharingFunctions>());
    ASSERT_NE(nullptr, sharing);
    sharing->GLDeviceHandle = 0x2c;
    EXPECT_EQ(0x2cU, sharing->getGLDeviceHandle());
}

TEST_F(glSharingTests, whenGetGlContextHandleIsCalledThenProperHandleIsReturned) {
    auto *sharing = static_cast<GlSharingFunctionsMock *>(context.getSharing<GLSharingFunctions>());
    ASSERT_NE(nullptr, sharing);
    sharing->GLContextHandle = 0x2c;
    EXPECT_EQ(0x2cU, sharing->getGLContextHandle());
}
