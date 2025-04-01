/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/sharings/gl/cl_gl_api_intel.h"
#include "opencl/source/sharings/gl/gl_arb_sync_event.h"
#include "opencl/source/sharings/gl/gl_buffer.h"
#include "opencl/source/sharings/gl/gl_context_guard.h"
#include "opencl/source/sharings/gl/gl_sync_event.h"
#include "opencl/source/sharings/gl/gl_texture.h"
#include "opencl/source/sharings/gl/windows/gl_sharing_windows.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/test/unit_test/mocks/gl/windows/mock_gl_arb_sync_event_windows.h"
#include "opencl/test/unit_test/mocks/gl/windows/mock_gl_sharing_windows.h"
#include "opencl/test/unit_test/mocks/mock_async_event_handler.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_event.h"

#include "gl_types.h"

using namespace NEO;

class GlSharingTests : public ::testing::Test {
  public:
    void SetUp() override {
        rootDeviceIndex = context.getDevice(0)->getRootDeviceIndex();
        mockGlSharingFunctions = mockGlSharing->sharingFunctions.release();
        context.setSharingFunctions(mockGlSharingFunctions);

        mockGlSharing->bufferInfoOutput.globalShareHandle = bufferId;
        mockGlSharing->bufferInfoOutput.bufferSize = 4096u;
        mockGlSharing->uploadDataToBufferInfo();
    }

    uint32_t rootDeviceIndex;
    MockContext context;
    std::unique_ptr<MockGlSharing> mockGlSharing = std::make_unique<MockGlSharing>();
    GlSharingFunctionsMock *mockGlSharingFunctions;
    unsigned int bufferId = 1u;
};

TEST_F(GlSharingTests, givenGlMockWhenItIsCreatedThenNonZeroObjectIsReturned) {
    EXPECT_NE(nullptr, &mockGlSharing);
    EXPECT_NE(nullptr, &mockGlSharing->clGlResourceInfo);
    EXPECT_NE(nullptr, &mockGlSharing->glClResourceInfo);
}

TEST_F(GlSharingTests, givenGLSharingFunctionsWhenAskedForIdThenClGlSharingIdIsReturned) {
    auto v = SharingType::CLGL_SHARING;
    EXPECT_EQ(v, mockGlSharingFunctions->getId());
}

TEST_F(GlSharingTests, givenMockGlWhenGlBufferIsCreatedThenMemObjectHasGlHandler) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId, &retVal);

    EXPECT_NE(nullptr, glBuffer);
    EXPECT_NE(nullptr, glBuffer->getGraphicsAllocation(rootDeviceIndex));
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(4096u, glBuffer->getGraphicsAllocation(rootDeviceIndex)->getUnderlyingBufferSize());
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));

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
    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        return nullptr;
    }
};

TEST_F(GlSharingTests, givenMockGlWhenGlBufferIsCreatedFromWrongHandleThenErrorAndNoBufferIsReturned) {
    auto tempMemoryManager = context.getMemoryManager();

    auto memoryManager = std::unique_ptr<FailingMemoryManager>(new FailingMemoryManager());
    context.memoryManager = memoryManager.get();

    auto retVal = CL_SUCCESS;
    auto glBuffer = GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, 0, &retVal);

    EXPECT_EQ(nullptr, glBuffer);
    EXPECT_EQ(CL_INVALID_GL_OBJECT, retVal);

    context.memoryManager = tempMemoryManager;
}

TEST_F(GlSharingTests, givenContextWhenClCreateFromGlBufferIsCalledThenBufferIsReturned) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, glBuffer);

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTests, givenContextWithoutSharingWhenClCreateFromGlBufferIsCalledThenErrorIsReturned) {
    context.resetSharingFunctions(CLGL_SHARING);
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    ASSERT_EQ(CL_INVALID_CONTEXT, retVal);
    ASSERT_EQ(nullptr, glBuffer);
}

GLboolean OSAPI mockGLAcquireSharedBuffer(GLDisplay, GLContext, GLContext, GLvoid *pResourceInfo) {
    return GL_FALSE;
};

TEST_F(GlSharingTests, givenContextWithSharingWhenClCreateFromGlBufferIsCalledWithIncorrectThenErrorIsReturned) {
    mockGlSharingFunctions->setGLAcquireSharedBufferMock(mockGLAcquireSharedBuffer);
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    ASSERT_EQ(CL_INVALID_GL_OBJECT, retVal);
    ASSERT_EQ(nullptr, glBuffer);
}

TEST_F(GlSharingTests, givenContextAnd32BitAddressingWhenClCreateFromGlBufferIsCalledThenBufferIsReturned) {
    auto flagToRestore = debugManager.flags.Force32bitAddressing.get();
    debugManager.flags.Force32bitAddressing.set(true);

    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, glBuffer);

    EXPECT_TRUE(castToObject<Buffer>(glBuffer)->getGraphicsAllocation(rootDeviceIndex)->is32BitAllocation());

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
    debugManager.flags.Force32bitAddressing.set(flagToRestore);
}

TEST_F(GlSharingTests, givenGlClBufferWhenAskedForCLGLGetInfoThenIdAndTypeIsReturned) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);

    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));

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

TEST_F(GlSharingTests, givenClBufferWhenAskedForCLGLGetInfoThenErrorIsReturned) {
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

TEST_F(GlSharingTests, givenClGLBufferWhenItIsAcquiredThenAcuqireCountIsIncremented) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));

    auto memObject = castToObject<Buffer>(glBuffer);
    EXPECT_FALSE(memObject->isMemObjZeroCopy());

    EXPECT_FALSE(memObject->isReadWriteOnCpuAllowed(context.getDevice(0)->getDevice()));
    auto currentGraphicsAllocation = memObject->getGraphicsAllocation(rootDeviceIndex);

    memObject->peekSharingHandler()->acquire(memObject, rootDeviceIndex);
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glGetCurrentContextCalled"));

    auto currentGraphicsAllocation2 = memObject->getGraphicsAllocation(rootDeviceIndex);

    EXPECT_EQ(currentGraphicsAllocation2, currentGraphicsAllocation);

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTests, givenClGLBufferWhenItIsAcquiredTwiceThenAcuqireIsNotCalled) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto memObject = castToObject<MemObj>(glBuffer);

    memObject->peekSharingHandler()->acquire(memObject, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glGetCurrentContextCalled"));

    memObject->peekSharingHandler()->acquire(memObject, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glGetCurrentContextCalled"));

    memObject->peekSharingHandler()->release(memObject, context.getDevice(0)->getRootDeviceIndex());
    memObject->peekSharingHandler()->release(memObject, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glGetCurrentContextCalled"));

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTests, givenClGLBufferWhenItIsCreatedAndGmmIsAvailableThenItIsUsedInGraphicsAllocation) {
    void *ptr = (void *)0x1000;
    auto rootDeviceIndex = context.getDevice(0)->getRootDeviceIndex();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    auto gmm = std::make_unique<Gmm>(context.getDevice(0)->getGmmHelper(), ptr, 4096u, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, StorageInfo{}, gmmRequirements);

    mockGlSharing->bufferInfoOutput.pGmmResInfo = gmm->gmmResourceInfo->peekGmmResourceInfo();
    mockGlSharing->uploadDataToBufferInfo();

    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto memObject = castToObject<MemObj>(glBuffer);

    auto graphicsAllocation = memObject->getGraphicsAllocation(rootDeviceIndex);
    ASSERT_NE(nullptr, graphicsAllocation->getDefaultGmm());
    EXPECT_NE(nullptr, graphicsAllocation->getDefaultGmm()->gmmResourceInfo->peekHandle());

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTests, givenClGLBufferWhenItIsAcquiredTwiceAfterReleaseThenAcuqireIsIncremented) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto memObject = castToObject<MemObj>(glBuffer);

    memObject->peekSharingHandler()->acquire(memObject, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glGetCurrentContextCalled"));

    memObject->peekSharingHandler()->release(memObject, context.getDevice(0)->getRootDeviceIndex());

    memObject->peekSharingHandler()->acquire(memObject, context.getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(3, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("glGetCurrentContextCalled"));

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTests, givenClGLBufferWhenItIsAcquireCountIsDecrementedToZeroThenCallReleaseFunction) {
    std::unique_ptr<Buffer> buffer(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId, nullptr));
    auto sharingHandler = buffer->peekSharingHandler();

    sharingHandler->acquire(buffer.get(), context.getDevice(0)->getRootDeviceIndex());
    sharingHandler->acquire(buffer.get(), context.getDevice(0)->getRootDeviceIndex());

    sharingHandler->release(buffer.get(), context.getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(0, mockGlSharing->dllParam->getParam("glReleaseSharedBufferCalled"));

    sharingHandler->release(buffer.get(), context.getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glReleaseSharedBufferCalled"));
    EXPECT_EQ(bufferId, mockGlSharing->dllParam->getBufferInfo().bufferName);
}

TEST_F(GlSharingTests, givenClGlBufferWhenCreatingSharedBufferThenCreateOrDestroyFlagInBufferInfoIsSet) {
    std::unique_ptr<Buffer> buffer(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId, nullptr));
    CL_GL_BUFFER_INFO info = mockGlSharing->dllParam->getBufferInfo();
    EXPECT_TRUE(info.createOrDestroy);
}

class MyGlBuffer : public GlBuffer {
  public:
    using GlBuffer::releaseResource;
    MyGlBuffer(GLSharingFunctions *sharingFunctions, unsigned int glObjectId) : GlBuffer(sharingFunctions, glObjectId) {}
};

TEST_F(GlSharingTests, givenClGlBufferWhenReleaseResourceCalledThenDoNotSetCreateOrDestroyFlag) {
    auto glSharingHandler = std::make_unique<MyGlBuffer>(context.getSharing<GLSharingFunctions>(), bufferId);

    glSharingHandler->releaseResource(nullptr, 0u);
    CL_GL_BUFFER_INFO info = mockGlSharing->dllParam->getBufferInfo();
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glReleaseSharedBufferCalled"));
    EXPECT_FALSE(info.createOrDestroy);
}

TEST_F(GlSharingTests, givenClGlBufferWhenDestroyClGLResourceThenReleaseWithCreateOrDestroyFlagSetIs) {
    auto glSharingHandler = std::make_unique<MyGlBuffer>(context.getSharing<GLSharingFunctions>(), bufferId);

    glSharingHandler.reset();
    CL_GL_BUFFER_INFO info = mockGlSharing->dllParam->getBufferInfo();
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glReleaseSharedBufferCalled"));
    EXPECT_TRUE(info.createOrDestroy);
}
TEST_F(GlSharingTests, givenClGLBufferWhenItIsAcquiredWithDifferentOffsetThenGraphicsAllocationContainsLatestOffsetValue) {
    auto retVal = CL_SUCCESS;
    auto rootDeviceIndex = context.getDevice(0)->getRootDeviceIndex();
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto memObject = castToObject<MemObj>(glBuffer);

    auto graphicsAddress = memObject->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress();

    mockGlSharing->bufferInfoOutput.bufferOffset = 50u;
    mockGlSharing->uploadDataToBufferInfo();

    memObject->peekSharingHandler()->acquire(memObject, rootDeviceIndex);

    auto offsetedGraphicsAddress = memObject->getGraphicsAllocation(rootDeviceIndex)->getGpuAddress();

    EXPECT_EQ(offsetedGraphicsAddress, graphicsAddress + mockGlSharing->bufferInfoOutput.bufferOffset);

    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTests, givenHwCommandQueueWhenAcquireIsCalledThenAcquireCountIsIncremented) {
    auto retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto buffer = castToObject<Buffer>(glBuffer);
    EXPECT_EQ(0u, buffer->acquireCount);

    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));
    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));
    EXPECT_EQ(1u, buffer->acquireCount);

    retVal = clEnqueueReleaseGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, buffer->acquireCount);

    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(3, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));
    EXPECT_EQ(1u, buffer->acquireCount);

    retVal = clReleaseCommandQueue(commandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTests, givenHwCommandQueueWhenAcquireIsCalledWithIncorrectWaitlistThenReturnError) {
    auto retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto buffer = castToObject<Buffer>(glBuffer);
    EXPECT_EQ(0u, buffer->acquireCount);

    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));
    retVal = clEnqueueAcquireGLObjects(commandQueue, 0, &glBuffer, 1, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, retVal);

    retVal = clReleaseCommandQueue(commandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTests, givenEnabledAsyncEventsHandlerWhenAcquireGlObjectsIsCalledWithIncompleteExternallySynchronizedEventThenItIsAddedToAsyncEventsHandler) {
    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
    debugManager.flags.EnableAsyncEventsHandler.set(true);

    auto handler = new MockHandler(false);
    context.getAsyncEventsHandlerUniquePtr().reset(handler);

    struct ExternallySynchronizedEvent : Event {
        ExternallySynchronizedEvent(Context *ctx)
            : Event(ctx, nullptr, 0, 0, 0) {
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

    auto *event = new ExternallySynchronizedEvent(&context);
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
}

TEST_F(GlSharingTests, givenDisabledAsyncEventsHandlerWhenAcquireGlObjectsIsCalledWithIncompleteExternallySynchronizedEventThenItIsNotAddedToAsyncEventsHandler) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableAsyncEventsHandler.set(false);

    auto handler = new MockHandler(false);
    context.getAsyncEventsHandlerUniquePtr().reset(handler);

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
}

TEST_F(GlSharingTests, givenEnabledAsyncEventsHandlerWhenAcquireGlObjectsIsCalledWithIncompleteButNotExternallySynchronizedEventThenItIsNotAddedToAsyncEventsHandler) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableAsyncEventsHandler.set(false);

    auto handler = new MockHandler(false);
    context.getAsyncEventsHandlerUniquePtr().reset(handler);

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
}

TEST_F(GlSharingTests, givenHwCommandQueueWhenReleaseIsCalledWithIncorrectWaitlistThenReturnError) {
    auto retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto buffer = castToObject<Buffer>(glBuffer);
    EXPECT_EQ(0u, buffer->acquireCount);

    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));
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

TEST_F(GlSharingTests, givenContextWithoutSharingWhenAcquireIsCalledThenErrorIsReturned) {
    auto retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto buffer = castToObject<Buffer>(glBuffer);
    EXPECT_EQ(0u, buffer->acquireCount);

    context.releaseSharingFunctions(CLGL_SHARING);
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));
    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);

    context.setSharingFunctions(mockGlSharingFunctions);
    retVal = clReleaseCommandQueue(commandQueue);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTests, givenContextWithoutSharingWhenReleaseIsCalledThenErrorIsReturned) {
    auto retVal = CL_SUCCESS;
    auto commandQueue = clCreateCommandQueue(&context, context.getDevice(0), 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto buffer = castToObject<Buffer>(glBuffer);
    EXPECT_EQ(0u, buffer->acquireCount);

    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));
    retVal = clEnqueueAcquireGLObjects(commandQueue, 1, &glBuffer, 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));
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

TEST_F(GlSharingTests, givenHwCommandQueueWhenAcquireAndReleaseCallsAreMadeWithEventsThenProperCmdTypeIsReturned) {
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

HWTEST_F(GlSharingTests, givenCommandQueueWhenReleaseGlObjectIsCalledThenFinishIsCalled) {
    MockCommandQueueHw<FamilyType> mockCmdQueue(&context, context.getDevice(0), nullptr);
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, nullptr);

    EXPECT_EQ(CL_SUCCESS, clEnqueueAcquireGLObjects(&mockCmdQueue, 1, &glBuffer, 0, nullptr, nullptr));
    mockCmdQueue.taskCount = 5u;
    EXPECT_EQ(CL_SUCCESS, clEnqueueReleaseGLObjects(&mockCmdQueue, 1, &glBuffer, 0, nullptr, nullptr));
    EXPECT_EQ(5u, mockCmdQueue.latestTaskCountWaited);

    clReleaseMemObject(glBuffer);
}

TEST_F(GlSharingTests, givenMockGLWhenFunctionsAreCalledThenCallsAreReceived) {
    auto ptrToStruct = &mockGlSharing->clGlResourceInfo;
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
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glAcquireSharedBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glAcquireSharedRenderBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glAcquireSharedTextureCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glReleaseSharedBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glReleaseSharedRenderBufferCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glReleaseSharedTextureCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glGetCurrentContextCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glGetCurrentDisplayCalled"));
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glMakeCurrentCalled"));
}

TEST(glSharingBasicTest, GivenSharingFunctionsWhenItIsConstructedThenOglContextFunctionIsCalled) {
    GLType glHDCType = 0;
    GLContext glHGLRCHandle = 0;
    GLDisplay glHDCHandle = 0;
    GlDllHelper getDllParam;

    GlSharingFunctionsMock glSharingFunctions(glHDCType, glHGLRCHandle, glHGLRCHandle, glHDCHandle);
    EXPECT_EQ(1, getDllParam.getGLSetSharedOCLContextStateReturnedValue());
}

TEST(glSharingBasicTest, givenInvalidExtensionNameWhenCheckGLExtensionSupportedThenReturnFalse) {
    MockGLSharingFunctions glSharingFunctions;
    const unsigned char invalidExtension[] = "InvalidExtensionName";
    bool retVal = glSharingFunctions.isOpenGlExtensionSupported(invalidExtension);
    EXPECT_FALSE(retVal);
}

TEST(glSharingBasicTest, givenglGetIntegervIsNullWhenCheckGLExtensionSupportedThenReturnFalse) {
    MockGLSharingFunctions glSharingFunctions;
    glSharingFunctions.glGetIntegerv = nullptr;
    const unsigned char invalidExtension[] = "InvalidExtensionName";
    bool retVal = glSharingFunctions.isOpenGlExtensionSupported(invalidExtension);
    EXPECT_FALSE(retVal);
}

TEST(glSharingBasicTest, givenValidExtensionNameWhenCheckGLExtensionSupportedThenReturnTrue) {
    MockGLSharingFunctions glSharingFunctions;
    const unsigned char supportGLOES[] = "GL_OES_framebuffer_object";
    bool retVal = glSharingFunctions.isOpenGlExtensionSupported(supportGLOES);
    EXPECT_TRUE(retVal);
}

TEST(glSharingBasicTest, givenWhenCheckGLSharingSupportedThenReturnTrue) {
    MockGLSharingFunctions glSharingFunctions;
    bool retVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_TRUE(retVal);
}

TEST(glSharingBasicTest, givenVendorisNullWhenCheckGLSharingSupportedThenReturnFalse) {
    auto invalidGetStringFcn = [](GLenum name) -> const GLubyte * {
        return (const GLubyte *)"";
    };

    MockGLSharingFunctions glSharingFunctions;
    glSharingFunctions.glGetString = invalidGetStringFcn;

    bool retVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_FALSE(retVal);
}

TEST(glSharingBasicTest, givenVersionisNullWhenCheckGLSharingSupportedThenReturnFalse) {

    MockGLSharingFunctions glSharingFunctions;
    glSharingFunctions.dllParam->glSetString("", GL_VERSION); // version returns null
    bool retVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_FALSE(retVal);
    glSharingFunctions.dllParam->glSetString("Int..", GL_VENDOR);
    retVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_FALSE(retVal);
}

TEST(glSharingBasicTest, givenVersionisGlesWhenCheckGLSharingSupportedThenReturnFalse) {
    MockGLSharingFunctions glSharingFunctions;

    glSharingFunctions.dllParam->glSetString("OpenGL ES", GL_VERSION);
    bool retVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_TRUE(retVal);

    glSharingFunctions.dllParam->glSetString("OpenGL ES 1.", GL_VERSION);
    retVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_TRUE(retVal);

    glSharingFunctions.dllParam->glSetString("2.0", GL_VERSION);
    retVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_TRUE(retVal);

    glSharingFunctions.dllParam->glSetStringi("GL_EXT_framebuffer_o...", 1);
    retVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_FALSE(retVal);

    glSharingFunctions.dllParam->glSetStringi("GL_EXT_framebuffer_object", 1);
    retVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_TRUE(retVal);

    glSharingFunctions.dllParam->glSetString("OpenGL ES 1.", GL_VERSION);
    glSharingFunctions.dllParam->glSetStringi("GL_OES_framebuffer_o...", 0);
    retVal = glSharingFunctions.isOpenGlSharingSupported();
    EXPECT_FALSE(retVal);
}

TEST(glSharingBasicTest, givensetSharedOCLContextStateWhenCallThenCorrectValue) {
    MockGLSharingFunctions glSharingFunctions;
    glSharingFunctions.dllParam->setGLSetSharedOCLContextStateReturnedValue(0u);
    EXPECT_EQ(0u, glSharingFunctions.setSharedOCLContextState());
    glSharingFunctions.dllParam->setGLSetSharedOCLContextStateReturnedValue(1u);
    EXPECT_EQ(1u, glSharingFunctions.setSharedOCLContextState());
}
TEST(glSharingBasicTest, givenGlSharingFunctionsWhenItIsConstructedThenFunctionsAreLoaded) {
    GLType glHDCType = 0;
    GLContext glHGLRCHandle = 0;
    GLDisplay glHDCHandle = 0;

    GlSharingFunctionsMock glSharingFunctions(glHDCType, glHGLRCHandle, glHGLRCHandle, glHDCHandle);

    EXPECT_NE(nullptr, glSharingFunctions.glGetCurrentContext);
    EXPECT_NE(nullptr, glSharingFunctions.glGetCurrentDisplay);
    EXPECT_NE(nullptr, glSharingFunctions.glGetString);
    EXPECT_NE(nullptr, glSharingFunctions.glGetIntegerv);
    EXPECT_NE(nullptr, glSharingFunctions.pfnWglCreateContext);
    EXPECT_NE(nullptr, glSharingFunctions.pfnWglDeleteContext);
    EXPECT_NE(nullptr, glSharingFunctions.pfnWglShareLists);
    EXPECT_NE(nullptr, glSharingFunctions.wglMakeCurrent);
    EXPECT_NE(nullptr, glSharingFunctions.glSetSharedOCLContextState);
    EXPECT_NE(nullptr, glSharingFunctions.glAcquireSharedBuffer);
    EXPECT_NE(nullptr, glSharingFunctions.glReleaseSharedBuffer);
    EXPECT_NE(nullptr, glSharingFunctions.glAcquireSharedRenderBuffer);
    EXPECT_NE(nullptr, glSharingFunctions.glReleaseSharedRenderBuffer);
    EXPECT_NE(nullptr, glSharingFunctions.glAcquireSharedTexture);
    EXPECT_NE(nullptr, glSharingFunctions.glReleaseSharedTexture);
    EXPECT_NE(nullptr, glSharingFunctions.glRetainSync);
    EXPECT_NE(nullptr, glSharingFunctions.glReleaseSync);
    EXPECT_NE(nullptr, glSharingFunctions.glGetSynciv);
    EXPECT_NE(nullptr, glSharingFunctions.glGetStringi);
}

TEST(glSharingBasicTest, givenNumEntriesLowerThanSupportedFormatsWhenGettingSupportedFormatsThenOnlyNumEntiresAreReturned) {
    MockGLSharingFunctions glSharingFunctions;
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 0;
    cl_GLenum glFormats[3] = {};

    auto retVal = glSharingFunctions.getSupportedFormats(flags, imageType, 1, glFormats, &numImageFormats);

    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(static_cast<uint32_t>(GlSharing::glToCLFormats.size()), numImageFormats);
    EXPECT_NE(0u, glFormats[0]);
    EXPECT_EQ(0u, glFormats[1]);
    EXPECT_EQ(0u, glFormats[2]);
}

TEST(glSharingBasicTest, givenCorrectFlagsWhenGettingSupportedFormatsThenCorrectListIsReturned) {
    MockGLSharingFunctions glSharingFunctions;
    cl_mem_flags flags[] = {CL_MEM_READ_ONLY, CL_MEM_WRITE_ONLY, CL_MEM_READ_WRITE, CL_MEM_KERNEL_READ_AND_WRITE};
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_GLenum glFormats[3] = {};
    cl_uint numImageFormats = 0;

    for (auto flag : flags) {

        auto result = glSharingFunctions.getSupportedFormats(flag, imageType, arrayCount(glFormats), glFormats, &numImageFormats);

        EXPECT_EQ(CL_SUCCESS, result);
        EXPECT_EQ(static_cast<uint32_t>(GlSharing::glToCLFormats.size()), numImageFormats);

        for (uint32_t formatIndex = 0; formatIndex < arrayCount(glFormats); formatIndex++) {
            EXPECT_NE(GlSharing::glToCLFormats.end(), GlSharing::glToCLFormats.find(glFormats[formatIndex]));
        }
    }
}

TEST(glSharingBasicTest, givenSupportedImageTypesWhenGettingSupportedFormatsThenCorrectListIsReturned) {
    MockGLSharingFunctions glSharingFunctions;
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageTypes[] = {CL_MEM_OBJECT_IMAGE1D, CL_MEM_OBJECT_IMAGE2D, CL_MEM_OBJECT_IMAGE3D, CL_MEM_OBJECT_IMAGE1D_ARRAY, CL_MEM_OBJECT_IMAGE1D_BUFFER, CL_MEM_OBJECT_IMAGE2D_ARRAY};
    cl_GLenum glFormats[3] = {};
    cl_uint numImageFormats = 0;

    for (auto imageType : imageTypes) {

        auto result = glSharingFunctions.getSupportedFormats(flags, imageType, arrayCount(glFormats), glFormats, &numImageFormats);

        EXPECT_EQ(CL_SUCCESS, result);
        EXPECT_EQ(static_cast<uint32_t>(GlSharing::glToCLFormats.size()), numImageFormats);

        for (auto glFormat : glFormats) {
            EXPECT_NE(GlSharing::glToCLFormats.end(), GlSharing::glToCLFormats.find(glFormat));
        }
    }
}

TEST(glSharingBasicTest, givenZeroNumEntriesWhenGettingSupportedFormatsThenNumFormatsIsReturned) {
    MockGLSharingFunctions glSharingFunctions;
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_uint numImageFormats = 0;

    auto result = glSharingFunctions.getSupportedFormats(flags, imageType, 0, nullptr, &numImageFormats);

    EXPECT_EQ(CL_SUCCESS, result);
    EXPECT_EQ(static_cast<uint32_t>(GlSharing::glToCLFormats.size()), numImageFormats);
}

TEST(glSharingBasicTest, givenNullNumImageFormatsWhenGettingSupportedFormatsThenNumFormatsIsNotDereferenced) {
    MockGLSharingFunctions glSharingFunctions;
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;

    auto result = glSharingFunctions.getSupportedFormats(flags, imageType, 0, nullptr, nullptr);

    EXPECT_EQ(CL_SUCCESS, result);
}

TEST(glSharingBasicTest, givenInvalidImageTypeWhenGettingSupportedFormatsThenIvalidValueErrorIsReturned) {
    MockGLSharingFunctions glSharingFunctions;
    cl_mem_flags flags = CL_MEM_READ_WRITE;
    cl_mem_object_type imageType = CL_MEM_OBJECT_PIPE;
    cl_GLenum glFormats[3] = {};
    cl_uint numImageFormats = 0;

    auto result = glSharingFunctions.getSupportedFormats(flags, imageType, arrayCount(glFormats), glFormats, &numImageFormats);

    EXPECT_EQ(CL_INVALID_VALUE, result);
    EXPECT_EQ(0u, numImageFormats);
}

TEST(glSharingBasicTest, givenInvalidFlagsWhenGettingSupportedFormatsThenIvalidValueErrorIsReturned) {
    MockGLSharingFunctions glSharingFunctions;
    cl_mem_flags flags = CL_MEM_NO_ACCESS_INTEL;
    cl_mem_object_type imageType = CL_MEM_OBJECT_IMAGE2D;
    cl_GLenum glFormats[3] = {};
    cl_uint numImageFormats = 0;

    auto result = glSharingFunctions.getSupportedFormats(flags, imageType, arrayCount(glFormats), glFormats, &numImageFormats);

    EXPECT_EQ(CL_INVALID_VALUE, result);
    EXPECT_EQ(0u, numImageFormats);
}

TEST_F(GlSharingTests, givenContextWhenCreateFromSharedBufferThenSharedImageIsReturned) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, glBuffer);
    auto parentbBuffer = castToObject<Buffer>(glBuffer);

    auto hardwareInfo = context.getDevice(0)->getRootDeviceEnvironment().getMutableHardwareInfo();
    hardwareInfo->capabilityTable.supportsImages = true;

    cl_image_format format = {CL_RGBA, CL_FLOAT};
    cl_image_desc imageDesc = {CL_MEM_OBJECT_IMAGE1D_BUFFER, 1, 1, 1, 1, 0, 0, 0, 0, {glBuffer}};
    cl_mem image = clCreateImage(&context, CL_MEM_READ_WRITE, &format, &imageDesc, 0, &retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);
    ASSERT_NE(nullptr, glBuffer);
    auto childImage = castToObject<Image>(image);

    EXPECT_EQ(parentbBuffer->peekSharingHandler(), childImage->peekSharingHandler());

    retVal = clReleaseMemObject(image);
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTests, givenClGLBufferWhenMapAndUnmapBufferIsCalledThenCopyOnGpu) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto buffer = castToObject<Buffer>(glBuffer);
    EXPECT_EQ(buffer->getCpuAddressForMemoryTransfer(), nullptr); // no cpu ptr
    auto gfxAllocation = buffer->getGraphicsAllocation(rootDeviceIndex);
    auto pClDevice = context.getDevice(0);
    for (auto handleId = 0u; handleId < gfxAllocation->getNumGmms(); handleId++) {
        gfxAllocation->setGmm(new MockGmm(pClDevice->getGmmHelper()), handleId);
    }
    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    auto heapless = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    auto heaplessStateInit = compilerProductHelper.isHeaplessStateInitEnabled(heapless);

    auto commandQueue = CommandQueue::create(&context, pClDevice, 0, false, retVal);
    ASSERT_EQ(CL_SUCCESS, retVal);

    size_t offset = 1;
    auto taskCount = commandQueue->taskCount + (heaplessStateInit ? 1u : 0u);
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

TEST_F(GlSharingTests, givenClGLBufferWhenretValIsNotPassedToCreateFunctionThenBufferIsCreated) {
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, nullptr);
    ASSERT_NE(nullptr, glBuffer);
    auto retVal = clReleaseMemObject(glBuffer);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(GlSharingTests, givenClGLBufferWhenMapAndUnmapBufferIsCalledTwiceThenReuseStorage) {
    auto retVal = CL_SUCCESS;
    auto glBuffer = clCreateFromGLBuffer(&context, 0, bufferId, &retVal);
    auto buffer = castToObject<Buffer>(glBuffer);
    EXPECT_EQ(buffer->getCpuAddressForMemoryTransfer(), nullptr); // no cpu ptr
    auto gfxAllocation = buffer->getGraphicsAllocation(rootDeviceIndex);
    auto pClDevice = context.getDevice(0);
    for (auto handleId = 0u; handleId < gfxAllocation->getNumGmms(); handleId++) {
        gfxAllocation->setGmm(new MockGmm(pClDevice->getGmmHelper()), handleId);
    }

    auto commandQueue = CommandQueue::create(&context, pClDevice, 0, false, retVal);
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

TEST_F(GlSharingTests, givenContextWithoutSharingWhenCreateEventFromGLThenErrorIsReturned) {
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

    EXPECT_EQ(0, mockGlSharing.dllParam->getParam("glMakeCurrentCalled"));
    EXPECT_EQ(1, mockGlSharing.dllParam->getParam("glGetCurrentContextCalled"));
    EXPECT_EQ(1, mockGlSharing.dllParam->getParam("glGetCurrentDisplayCalled"));

    mockGlSharing.overrideGetCurrentValues(bkpContext, display);
    {
        GLContextGuard guard(*mockGlSharing.sharingFunctions);
        EXPECT_EQ(0, mockGlSharing.dllParam->getParam("glMakeCurrentCalled"));
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().currentContext == bkpContext);
    }

    EXPECT_EQ(1, mockGlSharing.dllParam->getParam("glMakeCurrentCalled")); // destructor
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
        EXPECT_EQ(1, mockGlSharing.dllParam->getParam("glMakeCurrentCalled"));
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().madeCurrentContext == context);
    }
    EXPECT_EQ(2, mockGlSharing.dllParam->getParam("glMakeCurrentCalled")); // destructor
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
        EXPECT_EQ(2, mockGlSharing.dllParam->getParam("glMakeCurrentCalled"));
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().madeCurrentContext == bkpContext);
    }
    EXPECT_EQ(3, mockGlSharing.dllParam->getParam("glMakeCurrentCalled")); // destructor
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
        EXPECT_EQ(6, mockGlSharing.dllParam->getParam("glMakeCurrentCalled"));
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().madeCurrentContext == bkpContext);
    }
    EXPECT_EQ(7, mockGlSharing.dllParam->getParam("glMakeCurrentCalled")); // destructor
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
        EXPECT_EQ(1, mockGlSharing.dllParam->getParam("glMakeCurrentCalled"));
        EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().madeCurrentContext == bkpContext);
    }
    EXPECT_EQ(2, mockGlSharing.dllParam->getParam("glMakeCurrentCalled")); // destructor
    EXPECT_TRUE(mockGlSharing.dllParam->getGlMockReturnedValues().madeCurrentContext == zeroContext);
}

TEST(glSharingContextSwitch, givenSharingFunctionsWhenGlDeleteContextIsNotPresentThenItIsNotCalled) {
    auto glSharingFunctions = new GLSharingFunctionsWindows();
    GlDllHelper dllParam;
    auto currentGlDeleteContextCalledCount = dllParam.getParam("glDeleteContextCalled");
    delete glSharingFunctions;
    EXPECT_EQ(currentGlDeleteContextCalledCount, dllParam.getParam("glDeleteContextCalled"));
}

HWTEST_F(GlSharingTests, givenSyncObjectWhenCreateEventIsCalledThenCreateGLSyncObj) {
    cl_int retVal = CL_SUCCESS;
    GLsync glSync = {0};
    auto event = clCreateEventFromGLsyncKHR(&context, glSync, &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, event);

    auto &csr = reinterpret_cast<MockClDevice *>(context.getDevice(0))->getUltCommandStreamReceiver<FamilyType>();
    csr.taskLevel = 123;
    auto eventObj = castToObject<Event>(event);
    EXPECT_TRUE(eventObj->getCommandType() == CL_COMMAND_GL_FENCE_SYNC_OBJECT_KHR);
    EXPECT_TRUE(eventObj->peekExecutionStatus() == CL_SUBMITTED);
    EXPECT_EQ(CompletionStamp::notReady, eventObj->taskLevel);
    EXPECT_EQ(CompletionStamp::notReady, eventObj->getTaskLevel());
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glRetainSyncCalled"));

    eventObj->setStatus(CL_COMPLETE);
    EXPECT_EQ(0u, eventObj->getTaskLevel());
    clReleaseEvent(event);
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glReleaseSyncCalled"));
}

HWTEST_F(GlSharingTests, givenEventCreatedFromFenceObjectWhenItIsPassedToAcquireThenItsStatusIsUpdated) {
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

TEST_F(GlSharingTests, GivenGlSyncEventThenReportsAsExternallySynchronized) {
    GLsync glSync = {0};
    auto syncEvent = GlSyncEvent::create(context, glSync, nullptr);
    ASSERT_NE(nullptr, syncEvent);
    EXPECT_TRUE(syncEvent->isExternallySynchronized());
    syncEvent->release();
}

TEST_F(GlSharingTests, givenSyncEventWhenUpdateExecutionStatusIsCalledThenGLGetSyncivCalled) {
    GLsync glSync = {0};
    auto syncEvent = GlSyncEvent::create(context, glSync, nullptr);
    ASSERT_NE(nullptr, syncEvent);

    mockGlSharing->setGetSyncivReturnValue(GL_UNSIGNALED);
    syncEvent->updateExecutionStatus();
    EXPECT_EQ(1, mockGlSharing->dllParam->getParam("glGetSyncivCalled"));
    EXPECT_TRUE(syncEvent->updateEventAndReturnCurrentStatus() == CL_SUBMITTED);
    EXPECT_EQ(2, mockGlSharing->dllParam->getParam("glGetSyncivCalled")); // updateExecutionStatus called in peekExecutionStatus

    mockGlSharing->setGetSyncivReturnValue(GL_SIGNALED);
    syncEvent->updateExecutionStatus();
    EXPECT_EQ(3, mockGlSharing->dllParam->getParam("glGetSyncivCalled"));
    EXPECT_TRUE(syncEvent->peekExecutionStatus() == CL_COMPLETE);

    delete syncEvent;
}

TEST_F(GlSharingTests, givenContextWhenEmptySharingTableEmptyThenReturnsNullptr) {
    MockContext context;
    context.clearSharingFunctions();
    GLSharingFunctions *sharingF = context.getSharing<GLSharingFunctions>();
    EXPECT_EQ(sharingF, nullptr);
}

TEST_F(GlSharingTests, givenUnknownBaseEventWhenGetGlArbSyncEventIsCalledThenNullptrIsReturned) {
    auto *sharing = context.getSharing<GLSharingFunctionsWindows>();
    ASSERT_NE(nullptr, sharing);
    auto event = new MockEvent<UserEvent>();
    MockContext context;
    EXPECT_EQ(nullptr, sharing->getGlArbSyncEvent(*event));
    event->release();
}

TEST_F(GlSharingTests, givenKnownBaseEventWhenGetGlArbSyncEventIsCalledThenProperArbEventIsReturned) {
    auto *sharing = static_cast<GlSharingFunctionsMock *>(context.getSharing<GLSharingFunctions>());
    ASSERT_NE(nullptr, sharing);
    auto baseEvent = new MockEvent<UserEvent>;
    auto arbSyncEvent = reinterpret_cast<GlArbSyncEvent *>(0x1c);
    sharing->glArbEventMapping[baseEvent] = arbSyncEvent;
    EXPECT_EQ(arbSyncEvent, sharing->getGlArbSyncEvent(*baseEvent));
    baseEvent->release();
}

TEST_F(GlSharingTests, givenKnownBaseEventWhenRemoveGlArbSyncEventMappingIsCalledThenProperArbEventIsRemovedFromMap) {
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

TEST_F(GlSharingTests, givenUnknownBaseEventWhenRemoveGlArbSyncEventMappingIsCalledThenProperArbEventIsRemovedFromMap) {
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

TEST_F(GlSharingTests, givenUnknownBaseEventWhenGetOrCreateGlArbSyncEventIsCalledThenNewArbEventIsReturned) {
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

TEST_F(GlSharingTests, givenKnownBaseEventWhenGetOrCreateGlArbSyncEventIsCalledThenOldArbEventIsReused) {
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

TEST_F(GlSharingTests, WhenArbSyncEventCreationFailsThenGetOrCreateGlArbSyncEventReturnsNull) {
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

TEST_F(GlSharingTests, whenGetGlDeviceHandleIsCalledThenProperHandleIsReturned) {
    auto *sharing = static_cast<GlSharingFunctionsMock *>(context.getSharing<GLSharingFunctions>());
    ASSERT_NE(nullptr, sharing);
    sharing->glDeviceHandle = 0x2c;
    EXPECT_EQ(0x2cU, sharing->getGLDeviceHandle());
}

TEST_F(GlSharingTests, whenGetGlContextHandleIsCalledThenProperHandleIsReturned) {
    auto *sharing = static_cast<GlSharingFunctionsMock *>(context.getSharing<GLSharingFunctions>());
    ASSERT_NE(nullptr, sharing);
    sharing->glContextHandle = 0x2c;
    EXPECT_EQ(0x2cU, sharing->getGLContextHandle());
}

TEST_F(GlSharingTests, givenClGLBufferWhenCreatedThenSharedBufferAllocatoinTypeIsSet) {
    std::unique_ptr<Buffer> buffer(GlBuffer::createSharedGlBuffer(&context, CL_MEM_READ_WRITE, bufferId, nullptr));
    ASSERT_NE(nullptr, buffer->getGraphicsAllocation(rootDeviceIndex));
    EXPECT_EQ(AllocationType::sharedBuffer, buffer->getGraphicsAllocation(rootDeviceIndex)->getAllocationType());
}

using clGetSupportedGLTextureFormatsINTELTests = GlSharingTests;

TEST_F(clGetSupportedGLTextureFormatsINTELTests, givenContextWithoutGlSharingWhenGettingFormatsThenInvalidContextErrorIsReturned) {
    MockContext context;

    auto retVal = clGetSupportedGLTextureFormatsINTEL(&context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_CONTEXT, retVal);
}

TEST_F(clGetSupportedGLTextureFormatsINTELTests, givenValidInputsWhenGettingFormatsThenSuccesAndValidFormatsAreReturned) {
    cl_uint numFormats = 0;
    cl_GLenum glFormats[2] = {};
    auto glFormatsCount = static_cast<cl_uint>(arrayCount(glFormats));

    auto retVal = clGetSupportedGLTextureFormatsINTEL(&context, CL_MEM_READ_WRITE, CL_MEM_OBJECT_IMAGE2D,
                                                      glFormatsCount, glFormats, &numFormats);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(0u, numFormats);

    for (uint32_t i = 0; i < glFormatsCount; i++) {
        EXPECT_NE(GlSharing::glToCLFormats.end(), GlSharing::glToCLFormats.find(glFormats[i]));
    }
}

TEST(GlSharingAdapterLuid, whenInitializingGlSharingThenProperAdapterLuidIsObtained) {
    GlDllHelper dllParam;
    dllParam.resetParam("glGetLuidCalled");
    {
        dllParam.resetParam("glGetLuidFuncAvailable");
        MockGLSharingFunctions glSharing;
        LUID expectedLuid{};
        expectedLuid.HighPart = 0x1d2e;
        expectedLuid.LowPart = 0x3f4a;
        EXPECT_EQ(0, dllParam.getParam("glGetLuidCalled"));

        auto luid = glSharing.getAdapterLuid(reinterpret_cast<GLContext>(0x1));
        EXPECT_EQ(1, dllParam.getParam("glGetLuidCalled"));
        EXPECT_EQ(expectedLuid.HighPart, luid.HighPart);
        EXPECT_EQ(expectedLuid.LowPart, luid.LowPart);
        dllParam.resetParam("glGetLuidCalled");
    }

    {
        dllParam.resetParam("glGetLuidFuncAvailable");
        MockGLSharingFunctions glSharing;
        LUID expectedLuid{};
        expectedLuid.HighPart = 0x5d2e;
        expectedLuid.LowPart = 0x3f4a;
        EXPECT_EQ(0, dllParam.getParam("glGetLuidCalled"));

        auto luid = glSharing.getAdapterLuid(reinterpret_cast<GLContext>(0x2));
        EXPECT_EQ(1, dllParam.getParam("glGetLuidCalled"));
        EXPECT_EQ(expectedLuid.HighPart, luid.HighPart);
        EXPECT_EQ(expectedLuid.LowPart, luid.LowPart);
        dllParam.resetParam("glGetLuidCalled");
    }

    {
        dllParam.resetParam("glGetLuidFuncNotAvailable");
        MockGLSharingFunctions glSharing;

        EXPECT_EQ(0, dllParam.getParam("glGetLuidCalled"));

        auto luid = glSharing.getAdapterLuid(reinterpret_cast<GLContext>(0x1));
        EXPECT_EQ(0, dllParam.getParam("glGetLuidCalled"));

        EXPECT_EQ(0, luid.HighPart);
        EXPECT_EQ(0u, luid.LowPart);
    }
    dllParam.resetParam("glGetLuidFuncAvailable");
}
