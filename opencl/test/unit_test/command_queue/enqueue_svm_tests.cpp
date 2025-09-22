/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/memory_manager/allocations_list.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/utilities/hw_timestamps.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_align_malloc_memory_manager.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_cpu_page_fault_manager.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/event/user_event.h"
#include "opencl/source/program/create.inl"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/command_queue/enqueue_map_buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_builder.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

using namespace NEO;

struct EnqueueSvmTest : public ClDeviceFixture,
                        public CommandQueueHwFixture,
                        public ::testing::Test {
    typedef CommandQueueHwFixture CommandQueueFixture;

    EnqueueSvmTest() {
    }

    void SetUp() override {
        ClDeviceFixture::setUp();
        CommandQueueFixture::setUp(pClDevice, 0);
        ptrSVM = context->getSVMAllocsManager()->createSVMAlloc(256, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
    }

    void TearDown() override {
        context->getSVMAllocsManager()->freeSVMAlloc(ptrSVM);
        CommandQueueFixture::tearDown();
        ClDeviceFixture::tearDown();
    }

    std::pair<ReleaseableObjectPtr<Buffer>, void *> createBufferAndMapItOnGpu() {
        DebugManagerStateRestore restore{};
        debugManager.flags.DisableZeroCopyForBuffers.set(1);

        BufferDefaults::context = this->context;
        ReleaseableObjectPtr<Buffer> buffer = clUniquePtr(BufferHelper<>::create());
        void *mappedPtr = pCmdQ->enqueueMapBuffer(buffer.get(), CL_TRUE, CL_MAP_READ, 0, buffer->getSize(), 0, nullptr, nullptr, retVal);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_NE(nullptr, mappedPtr);
        return {std::move(buffer), mappedPtr};
    }

    cl_int retVal = CL_SUCCESS;
    void *ptrSVM = nullptr;
};

TEST_F(EnqueueSvmTest, GivenInvalidSvmPtrWhenMappingSvmThenInvalidValueErrorIsReturned) {
    void *svmPtr = nullptr;
    retVal = this->pCmdQ->enqueueSVMMap(
        CL_FALSE,    // cl_bool blocking_map
        CL_MAP_READ, // cl_map_flags map_flags
        svmPtr,      // void *svm_ptr
        0,           // size_t size
        0,           // cl_uint num_events_in_wait_list
        nullptr,     // const cL_event *event_wait_list
        nullptr,     // cl_event *event
        false);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(EnqueueSvmTest, GivenValidParamsWhenMappingSvmThenSuccessIsReturned) {
    retVal = this->pCmdQ->enqueueSVMMap(
        CL_FALSE,    // cl_bool blocking_map
        CL_MAP_READ, // cl_map_flags map_flags
        ptrSVM,      // void *svm_ptr
        256,         // size_t size
        0,           // cl_uint num_events_in_wait_list
        nullptr,     // const cL_event *event_wait_list
        nullptr,     // cl_event *event
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueSvmTest, GivenGpuHangAndBlockingCallAndValidParamsWhenMappingSvmThenOutOfResourcesIsReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    const auto enqueueResult = mockCommandQueueHw.enqueueSVMMap(
        CL_TRUE,     // cl_bool blocking_map
        CL_MAP_READ, // cl_map_flags map_flags
        ptrSVM,      // void *svm_ptr
        256,         // size_t size
        0,           // cl_uint num_events_in_wait_list
        nullptr,     // const cL_event *event_wait_list
        nullptr,     // cl_event *event
        false);      // bool externalAppCall

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

TEST_F(EnqueueSvmTest, GivenValidParamsWhenMappingSvmWithBlockingThenSuccessIsReturned) {
    retVal = this->pCmdQ->enqueueSVMMap(
        CL_TRUE,     // cl_bool blocking_map
        CL_MAP_READ, // cl_map_flags map_flags
        ptrSVM,      // void *svm_ptr
        256,         // size_t size
        0,           // cl_uint num_events_in_wait_list
        nullptr,     // const cL_event *event_wait_list
        nullptr,     // cl_event *event
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueSvmTest, GivenValidParamsWhenMappingSvmWithEventsThenSuccessIsReturned) {
    UserEvent uEvent;
    cl_event eventWaitList[] = {&uEvent};
    retVal = this->pCmdQ->enqueueSVMMap(
        CL_FALSE,      // cl_bool blocking_map
        CL_MAP_READ,   // cl_map_flags map_flags
        ptrSVM,        // void *svm_ptr
        256,           // size_t size
        1,             // cl_uint num_events_in_wait_list
        eventWaitList, // const cL_event *event_wait_list
        nullptr,       // cl_event *event
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueSvmTest, GivenInvalidSvmPtrWhenUnmappingSvmThenInvalidValueErrorIsReturned) {
    void *svmPtr = nullptr;
    retVal = this->pCmdQ->enqueueSVMUnmap(
        svmPtr,  // void *svm_ptr
        0,       // cl_uint num_events_in_wait_list
        nullptr, // const cL_event *event_wait_list
        nullptr, // cl_event *event
        false);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(EnqueueSvmTest, GivenValidParamsWhenUnmappingSvmThenSuccessIsReturned) {
    retVal = this->pCmdQ->enqueueSVMUnmap(
        ptrSVM,  // void *svm_ptr
        0,       // cl_uint num_events_in_wait_list
        nullptr, // const cL_event *event_wait_list
        nullptr, // cl_event *event
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueSvmTest, GivenGpuHangAndBlockingCallAndValidParamsWhenUnmappingSvmThenOutOfResourcesIsReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    const auto enqueueResult = mockCommandQueueHw.enqueueSVMUnmap(
        ptrSVM,
        0,
        nullptr,
        nullptr,
        false);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

TEST_F(EnqueueSvmTest, GivenValidParamsWhenUnmappingSvmWithEventsThenSuccessIsReturned) {
    UserEvent uEvent;
    cl_event eventWaitList[] = {&uEvent};
    retVal = this->pCmdQ->enqueueSVMUnmap(
        ptrSVM,        // void *svm_ptr
        1,             // cl_uint num_events_in_wait_list
        eventWaitList, // const cL_event *event_wait_list
        nullptr,       // cl_event *event
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueSvmTest, GivenValidParamsWhenFreeingSvmThenSuccessIsReturned) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableAsyncEventsHandler.set(false);
    ASSERT_EQ(1U, this->context->getSVMAllocsManager()->getNumAllocs());
    void *svmPtrs[] = {ptrSVM};
    retVal = this->pCmdQ->enqueueSVMFree(
        1,       // cl_uint num_svm_pointers
        svmPtrs, // void *svm_pointers[]
        nullptr, // (CL_CALLBACK  *pfn_free_func) (cl_command_queue queue, cl_uint num_svm_pointers, void *svm_pointers[])
        nullptr, // void *user_data
        0,       // cl_uint num_events_in_wait_list
        nullptr, // const cl_event *event_wait_list
        nullptr  // cl_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(0U, this->context->getSVMAllocsManager()->getNumAllocs());
}

TEST_F(EnqueueSvmTest, GivenValidParamsWhenFreeingSvmWithCallbackThenSuccessIsReturned) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableAsyncEventsHandler.set(false);
    void *svmPtrs[] = {ptrSVM};
    bool callbackWasCalled = false;
    struct ClbHelper {
        ClbHelper(bool &callbackWasCalled)
            : callbackWasCalled(callbackWasCalled) {}

        static void CL_CALLBACK clb(cl_command_queue queue, cl_uint numSvmPointers, void **svmPointers, void *usrData) {
            ClbHelper *data = (ClbHelper *)usrData;
            data->callbackWasCalled = true;
        }

        bool &callbackWasCalled;
    } userData(callbackWasCalled);

    retVal = this->pCmdQ->enqueueSVMFree(
        1,              // cl_uint num_svm_pointers
        svmPtrs,        // void *svm_pointers[]
        ClbHelper::clb, // (CL_CALLBACK  *pfn_free_func) (cl_command_queue queue, cl_uint num_svm_pointers, void *svm_pointers[])
        &userData,      // void *user_data
        0,              // cl_uint num_events_in_wait_list
        nullptr,        // const cl_event *event_wait_list
        nullptr         // cl_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(callbackWasCalled);
}

TEST_F(EnqueueSvmTest, GivenValidParamsWhenFreeingSvmWithCallbackAndEventThenSuccessIsReturned) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableAsyncEventsHandler.set(false);
    void *svmPtrs[] = {ptrSVM};
    bool callbackWasCalled = false;
    struct ClbHelper {
        ClbHelper(bool &callbackWasCalled)
            : callbackWasCalled(callbackWasCalled) {}

        static void CL_CALLBACK clb(cl_command_queue queue, cl_uint numSvmPointers, void **svmPointers, void *usrData) {
            ClbHelper *data = (ClbHelper *)usrData;
            data->callbackWasCalled = true;
        }

        bool &callbackWasCalled;
    } userData(callbackWasCalled);
    cl_event event = nullptr;

    retVal = this->pCmdQ->enqueueSVMFree(
        1,              // cl_uint num_svm_pointers
        svmPtrs,        // void *svm_pointers[]
        ClbHelper::clb, // (CL_CALLBACK  *pfn_free_func) (cl_command_queue queue, cl_uint num_svm_pointers, void *svm_pointers[])
        &userData,      // void *user_data
        0,              // cl_uint num_events_in_wait_list
        nullptr,        // const cl_event *event_wait_list
        &event          // cl_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(callbackWasCalled);

    auto pEvent = (Event *)event;
    delete pEvent;
}

TEST_F(EnqueueSvmTest, GivenValidParamsWhenFreeingSvmWithBlockingThenSuccessIsReturned) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableAsyncEventsHandler.set(false);
    void *svmPtrs[] = {ptrSVM};
    UserEvent uEvent;
    cl_event eventWaitList[] = {&uEvent};
    retVal = this->pCmdQ->enqueueSVMFree(
        1,             // cl_uint num_svm_pointers
        svmPtrs,       // void *svm_pointers[]
        nullptr,       // (CL_CALLBACK  *pfn_free_func) (cl_command_queue queue, cl_uint num_svm_pointers, void *svm_pointers[])
        nullptr,       // void *user_data
        1,             // cl_uint num_events_in_wait_list
        eventWaitList, // const cl_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueSvmTest, GivenEventAndGpuHangAndBlockingCallAndValidParamsWhenFreeingSvmWithBlockingThenEventIsNotDeletedAndOutOfResourcesIsReturned) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableAsyncEventsHandler.set(false);
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    const cl_uint numOfSvmPointers = 1;
    void *svmPtrs[numOfSvmPointers] = {ptrSVM};

    UserEvent uEvent;
    const cl_uint numOfEvents = 1;
    cl_event eventWaitList[numOfEvents] = {&uEvent};

    cl_event retEvent = nullptr;
    const auto enqueueResult = mockCommandQueueHw.enqueueSVMFree(
        numOfSvmPointers,
        svmPtrs,
        nullptr,
        nullptr,
        numOfEvents,
        eventWaitList,
        &retEvent);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);

    ASSERT_NE(nullptr, retEvent);
    castToObjectOrAbort<Event>(retEvent)->release();
}

HWTEST_F(EnqueueSvmTest, GivenGpuHangAndBlockingCallAndValidParamsWhenFreeingSvmWithBlockingThenOutOfResourcesIsReturned) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableAsyncEventsHandler.set(false);
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    const cl_uint numOfSvmPointers = 1;
    void *svmPtrs[numOfSvmPointers] = {ptrSVM};

    UserEvent uEvent;
    const cl_uint numOfEvents = 1;
    cl_event eventWaitList[numOfEvents] = {&uEvent};

    const auto enqueueResult = mockCommandQueueHw.enqueueSVMFree(
        numOfSvmPointers,
        svmPtrs,
        nullptr,
        nullptr,
        numOfEvents,
        eventWaitList,
        nullptr);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

TEST_F(EnqueueSvmTest, GivenNullDstPtrWhenCopyingMemoryThenInvalidVaueErrorIsReturned) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.EnableAsyncEventsHandler.set(false);
    void *pDstSVM = nullptr;
    void *pSrcSVM = context->getSVMAllocsManager()->createSVMAlloc(256, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    context->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
}

TEST_F(EnqueueSvmTest, GivenNullSrcPtrWhenCopyingMemoryThenInvalidVaueErrorIsReturned) {
    void *pDstSVM = ptrSVM;
    void *pSrcSVM = nullptr;
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(EnqueueSvmTest, givenSrcHostPtrAndEventWhenEnqueueSVMMemcpyThenEventCommandTypeIsCorrectlySet) {
    char srcHostPtr[260] = {};
    void *pDstSVM = ptrSVM;
    void *pSrcSVM = srcHostPtr;
    cl_event event = nullptr;
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        &event,  // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    constexpr cl_command_type expectedCmd = CL_COMMAND_SVM_MEMCPY;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);
    clReleaseEvent(event);
}

TEST_F(EnqueueSvmTest, givenSrcHostPtrAndSizeZeroWhenEnqueueSVMMemcpyThenReturnSuccess) {
    char srcHostPtr[260] = {};
    void *pDstSVM = ptrSVM;
    void *pSrcSVM = srcHostPtr;
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        0,       // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueSvmTest, givenSrcHostPtrWhenEnqueueSVMMemcpyThenEnqueuWriteBufferIsCalled) {
    char srcHostPtr[260];
    void *pSrcSVM = srcHostPtr;
    void *pDstSVM = ptrSVM;
    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    retVal = myCmdQ.enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(myCmdQ.lastCommandType, static_cast<cl_command_type>(CL_COMMAND_WRITE_BUFFER));

    auto tempAlloc = myCmdQ.getGpgpuCommandStreamReceiver().getTemporaryAllocations().peekHead();
    EXPECT_EQ(0u, tempAlloc->countSuccessors());
    EXPECT_EQ(pSrcSVM, reinterpret_cast<void *>(tempAlloc->getGpuAddress()));

    auto srcAddress = myCmdQ.kernelParams.srcPtr;
    auto srcOffset = myCmdQ.kernelParams.srcOffset.x;
    auto dstAddress = myCmdQ.kernelParams.dstPtr;
    auto dstOffset = myCmdQ.kernelParams.dstOffset.x;
    EXPECT_EQ(alignDown(pSrcSVM, 4), srcAddress);
    EXPECT_EQ(ptrDiff(pSrcSVM, alignDown(pSrcSVM, 4)), srcOffset);
    EXPECT_EQ(alignDown(pDstSVM, 4), dstAddress);
    EXPECT_EQ(ptrDiff(pDstSVM, alignDown(pDstSVM, 4)), dstOffset);
}

HWTEST_F(EnqueueSvmTest, givenDstHostPtrWhenEnqueueSVMMemcpyThenEnqueuReadBufferIsCalled) {
    char dstHostPtr[260];
    void *pDstSVM = dstHostPtr;
    void *pSrcSVM = ptrSVM;
    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    retVal = myCmdQ.enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(myCmdQ.lastCommandType, static_cast<cl_command_type>(CL_COMMAND_READ_BUFFER));
    auto tempAlloc = myCmdQ.getGpgpuCommandStreamReceiver().getTemporaryAllocations().peekHead();

    EXPECT_EQ(0u, tempAlloc->countSuccessors());
    EXPECT_EQ(pDstSVM, reinterpret_cast<void *>(tempAlloc->getGpuAddress()));

    auto srcAddress = myCmdQ.kernelParams.srcPtr;
    auto srcOffset = myCmdQ.kernelParams.srcOffset.x;
    auto dstAddress = myCmdQ.kernelParams.dstPtr;
    auto dstOffset = myCmdQ.kernelParams.dstOffset.x;
    EXPECT_EQ(alignDown(pSrcSVM, 4), srcAddress);
    EXPECT_EQ(ptrDiff(pSrcSVM, alignDown(pSrcSVM, 4)), srcOffset);
    EXPECT_EQ(alignDown(pDstSVM, 4), dstAddress);
    EXPECT_EQ(ptrDiff(pDstSVM, alignDown(pDstSVM, 4)), dstOffset);
}

TEST_F(EnqueueSvmTest, givenDstHostPtrAndEventWhenEnqueueSVMMemcpyThenEventCommandTypeIsCorrectlySet) {
    char dstHostPtr[260];
    void *pDstSVM = dstHostPtr;
    void *pSrcSVM = ptrSVM;
    cl_event event = nullptr;
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        &event,  // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    constexpr cl_command_type expectedCmd = CL_COMMAND_SVM_MEMCPY;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);
    clReleaseEvent(event);
}

TEST_F(EnqueueSvmTest, givenDstHostPtrAndSizeZeroWhenEnqueueSVMMemcpyThenReturnSuccess) {
    char dstHostPtr[260];
    void *pDstSVM = dstHostPtr;
    void *pSrcSVM = ptrSVM;
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        0,       // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueSvmTest, givenDstHostPtrAndSrcHostPtrWhenEnqueueNonBlockingSVMMemcpyThenEnqueuWriteBufferIsCalled) {
    char dstHostPtr[] = {0, 0, 0};
    char srcHostPtr[] = {1, 2, 3};
    void *pDstSVM = dstHostPtr;
    void *pSrcSVM = srcHostPtr;
    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    retVal = myCmdQ.enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        3,       // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(myCmdQ.lastCommandType, static_cast<cl_command_type>(CL_COMMAND_WRITE_BUFFER));

    auto tempAlloc = myCmdQ.getGpgpuCommandStreamReceiver().getTemporaryAllocations().peekHead();
    EXPECT_EQ(1u, tempAlloc->countSuccessors());
    EXPECT_EQ(pSrcSVM, reinterpret_cast<void *>(tempAlloc->getGpuAddress()));
    EXPECT_EQ(pDstSVM, reinterpret_cast<void *>(tempAlloc->next->getGpuAddress()));

    auto srcAddress = myCmdQ.kernelParams.srcPtr;
    auto srcOffset = myCmdQ.kernelParams.srcOffset.x;
    auto dstAddress = myCmdQ.kernelParams.dstPtr;
    auto dstOffset = myCmdQ.kernelParams.dstOffset.x;
    EXPECT_EQ(alignDown(pSrcSVM, 4), srcAddress);
    EXPECT_EQ(ptrDiff(pSrcSVM, alignDown(pSrcSVM, 4)), srcOffset);
    EXPECT_EQ(alignDown(pDstSVM, 4), dstAddress);
    EXPECT_EQ(ptrDiff(pDstSVM, alignDown(pDstSVM, 4)), dstOffset);
}

HWTEST_F(EnqueueSvmTest, givenDstHostPtrAndSrcHostPtrWhenEnqueueBlockingSVMMemcpyThenEnqueuWriteBufferIsCalled) {
    char dstHostPtr[] = {0, 0, 0};
    char srcHostPtr[] = {1, 2, 3};
    void *pDstSVM = dstHostPtr;
    void *pSrcSVM = srcHostPtr;
    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    retVal = myCmdQ.enqueueSVMMemcpy(
        true,    // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        3,       // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(myCmdQ.lastCommandType, static_cast<cl_command_type>(CL_COMMAND_WRITE_BUFFER));

    auto srcAddress = myCmdQ.kernelParams.srcPtr;
    auto srcOffset = myCmdQ.kernelParams.srcOffset.x;
    auto dstAddress = myCmdQ.kernelParams.dstPtr;
    auto dstOffset = myCmdQ.kernelParams.dstOffset.x;
    EXPECT_EQ(alignDown(pSrcSVM, 4), srcAddress);
    EXPECT_EQ(ptrDiff(pSrcSVM, alignDown(pSrcSVM, 4)), srcOffset);
    EXPECT_EQ(alignDown(pDstSVM, 4), dstAddress);
    EXPECT_EQ(ptrDiff(pDstSVM, alignDown(pDstSVM, 4)), dstOffset);
}

TEST_F(EnqueueSvmTest, givenDstHostPtrAndSrcHostPtrAndSizeZeroWhenEnqueueSVMMemcpyThenReturnSuccess) {
    char dstHostPtr[260] = {};
    char srcHostPtr[260] = {};
    void *pDstSVM = dstHostPtr;
    void *pSrcSVM = srcHostPtr;
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        0,       // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueSvmTest, givenSvmToSvmCopyTypeWhenEnqueueNonBlockingSVMMemcpyThenSvmMemcpyCommandIsEnqueued) {
    void *pDstSVM = ptrSVM;
    void *pSrcSVM = context->getSVMAllocsManager()->createSVMAlloc(256, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    retVal = myCmdQ.enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(myCmdQ.lastCommandType, static_cast<cl_command_type>(CL_COMMAND_SVM_MEMCPY));

    auto tempAlloc = myCmdQ.getGpgpuCommandStreamReceiver().getTemporaryAllocations().peekHead();
    EXPECT_EQ(nullptr, tempAlloc);

    auto srcAddress = myCmdQ.kernelParams.srcPtr;
    auto srcOffset = myCmdQ.kernelParams.srcOffset.x;
    auto dstAddress = myCmdQ.kernelParams.dstPtr;
    auto dstOffset = myCmdQ.kernelParams.dstOffset.x;
    EXPECT_EQ(alignDown(pSrcSVM, 4), srcAddress);
    EXPECT_EQ(ptrDiff(pSrcSVM, alignDown(pSrcSVM, 4)), srcOffset);
    EXPECT_EQ(alignDown(pDstSVM, 4), dstAddress);
    EXPECT_EQ(ptrDiff(pDstSVM, alignDown(pDstSVM, 4)), dstOffset);
    context->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
}

TEST_F(EnqueueSvmTest, givenSvmToSvmCopyTypeWhenEnqueueBlockingSVMMemcpyThenSuccessIsReturned) {
    void *pDstSVM = ptrSVM;
    void *pSrcSVM = context->getSVMAllocsManager()->createSVMAlloc(256, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        true,    // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    context->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
}

TEST_F(EnqueueSvmTest, GivenValidParamsWhenCopyingMemoryWithBlockingThenSuccessisReturned) {
    void *pDstSVM = ptrSVM;
    void *pSrcSVM = context->getSVMAllocsManager()->createSVMAlloc(256, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
    auto uEvent = makeReleaseable<UserEvent>();
    cl_event eventWaitList[] = {uEvent.get()};
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,         // cl_bool  blocking_copy
        pDstSVM,       // void *dst_ptr
        pSrcSVM,       // const void *src_ptr
        256,           // size_t size
        1,             // cl_uint num_events_in_wait_list
        eventWaitList, // cl_evebt *event_wait_list
        nullptr,       // cL_event *event
        nullptr        // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    context->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
    uEvent->setStatus(-1);
}

TEST_F(EnqueueSvmTest, GivenCoherencyWhenCopyingMemoryThenSuccessIsReturned) {
    void *pDstSVM = ptrSVM;
    SVMAllocsManager::SvmAllocationProperties svmProperties;
    svmProperties.coherent = true;
    void *pSrcSVM = context->getSVMAllocsManager()->createSVMAlloc(256, svmProperties, context->getRootDeviceIndices(), context->getDeviceBitfields());
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    context->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
}

TEST_F(EnqueueSvmTest, GivenCoherencyWhenCopyingMemoryWithBlockingThenSuccessIsReturned) {
    void *pDstSVM = ptrSVM;
    SVMAllocsManager::SvmAllocationProperties svmProperties;
    svmProperties.coherent = true;
    void *pSrcSVM = context->getSVMAllocsManager()->createSVMAlloc(256, svmProperties, context->getRootDeviceIndices(), context->getDeviceBitfields());
    auto uEvent = makeReleaseable<UserEvent>();
    cl_event eventWaitList[] = {uEvent.get()};
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,         // cl_bool  blocking_copy
        pDstSVM,       // void *dst_ptr
        pSrcSVM,       // const void *src_ptr
        256,           // size_t size
        1,             // cl_uint num_events_in_wait_list
        eventWaitList, // cl_evebt *event_wait_list
        nullptr,       // cL_event *event
        nullptr        // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    context->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
    uEvent->setStatus(-1);
}

HWTEST_F(EnqueueSvmTest, givenUnalignedAddressWhenEnqueueMemcpyThenDispatchInfoHasAlignedAddressAndProperOffset) {
    void *pDstSVM = reinterpret_cast<void *>(0x17);
    void *pSrcSVM = ptrSVM;
    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    retVal = myCmdQ.enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        0,       // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto srcAddress = myCmdQ.kernelParams.srcPtr;
    auto srcOffset = myCmdQ.kernelParams.srcOffset.x;
    auto dstAddress = myCmdQ.kernelParams.dstPtr;
    auto dstOffset = myCmdQ.kernelParams.dstOffset.x;
    EXPECT_EQ(alignDown(pSrcSVM, 4), srcAddress);
    EXPECT_EQ(ptrDiff(pSrcSVM, alignDown(pSrcSVM, 4)), srcOffset);
    EXPECT_EQ(alignDown(pDstSVM, 4), dstAddress);
    EXPECT_EQ(ptrDiff(pDstSVM, alignDown(pDstSVM, 4)), dstOffset);
}

TEST_F(EnqueueSvmTest, GivenNullSvmPtrWhenFillingMemoryThenInvalidValueErrorIsReturned) {
    void *svmPtr = nullptr;
    const float pattern[1] = {1.2345f};
    const size_t patternSize = sizeof(pattern);
    retVal = this->pCmdQ->enqueueSVMMemFill(
        svmPtr,      // void *svm_ptr
        pattern,     // const void *pattern
        patternSize, // size_t pattern_size
        256,         // size_t size
        0,           // cl_uint num_events_in_wait_list
        nullptr,     // cl_evebt *event_wait_list
        nullptr      // cL_event *event
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

HWTEST_F(EnqueueSvmTest, givenSvmAllocWhenEnqueueSvmFillThenSuccessIsReturnedAndAddressIsProperlyAligned) {
    const float pattern[1] = {1.2345f};
    const size_t patternSize = sizeof(pattern);
    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    retVal = myCmdQ.enqueueSVMMemFill(
        ptrSVM,      // void *svm_ptr
        pattern,     // const void *pattern
        patternSize, // size_t pattern_size
        256,         // size_t size
        0,           // cl_uint num_events_in_wait_list
        nullptr,     // cl_evebt *event_wait_list
        nullptr      // cL_event *event
    );

    EXPECT_EQ(CL_SUCCESS, retVal);
    auto dstAddress = myCmdQ.kernelParams.dstPtr;
    auto dstOffset = myCmdQ.kernelParams.dstOffset.x;
    EXPECT_EQ(alignDown(ptrSVM, 4), dstAddress);
    EXPECT_EQ(ptrDiff(ptrSVM, alignDown(ptrSVM, 4)), dstOffset);
}

TEST_F(EnqueueSvmTest, GivenValidParamsWhenFillingMemoryWithBlockingThenSuccessIsReturned) {
    const float pattern[1] = {1.2345f};
    const size_t patternSize = sizeof(pattern);
    auto uEvent = makeReleaseable<UserEvent>();
    cl_event eventWaitList[] = {uEvent.get()};
    retVal = this->pCmdQ->enqueueSVMMemFill(
        ptrSVM,        // void *svm_ptr
        pattern,       // const void *pattern
        patternSize,   // size_t pattern_size
        256,           // size_t size
        1,             // cl_uint num_events_in_wait_list
        eventWaitList, // cl_evebt *event_wait_list
        nullptr        // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    uEvent->setStatus(-1);
}

HWTEST_F(EnqueueSvmTest, GivenGpuHangAndBlockingCallAndValidParamsWhenFillingMemoryThenOutOfResourcesIsReturned) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    const float pattern[1] = {1.2345f};
    const size_t patternSize = sizeof(pattern);

    auto uEvent = makeReleaseable<UserEvent>();
    const cl_uint numOfEvents = 1;
    cl_event eventWaitList[numOfEvents] = {uEvent.get()};

    const auto enqueueResult = mockCommandQueueHw.enqueueSVMMemFill(
        ptrSVM,
        pattern,
        patternSize,
        256,
        numOfEvents,
        eventWaitList,
        nullptr);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);

    uEvent->setStatus(-1);
}

TEST_F(EnqueueSvmTest, GivenRepeatCallsWhenFillingMemoryThenSuccessIsReturnedForEachCall) {
    const float pattern[1] = {1.2345f};
    const size_t patternSize = sizeof(pattern);
    retVal = this->pCmdQ->enqueueSVMMemFill(
        ptrSVM,      // void *svm_ptr
        pattern,     // const void *pattern
        patternSize, // size_t pattern_size
        256,         // size_t size
        0,           // cl_uint num_events_in_wait_list
        nullptr,     // cl_evebt *event_wait_list
        nullptr      // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    retVal = this->pCmdQ->enqueueSVMMemFill(
        ptrSVM,      // void *svm_ptr
        pattern,     // const void *pattern
        patternSize, // size_t pattern_size
        256,         // size_t size
        0,           // cl_uint num_events_in_wait_list
        nullptr,     // cl_evebt *event_wait_list
        nullptr      // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueSvmTest, givenEnqueueSVMMemFillWhenPatternAllocationIsObtainedThenItsTypeShouldBeSetToFillPattern) {
    auto &csr = pCmdQ->getGpgpuCommandStreamReceiver();
    if (pDevice->getProductHelper().getCommandBuffersPreallocatedPerCommandQueue() > 0) {
        csr.flushTagUpdate();
        csr.getInternalAllocationStorage()->cleanAllocationList(-1, AllocationUsage::REUSABLE_ALLOCATION);
    }
    ASSERT_TRUE(csr.getAllocationsForReuse().peekIsEmpty());

    const float pattern[1] = {1.2345f};
    const size_t patternSize = sizeof(pattern);
    const size_t size = patternSize;
    retVal = this->pCmdQ->enqueueSVMMemFill(
        ptrSVM,
        pattern,
        patternSize,
        size,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    ASSERT_FALSE(csr.getAllocationsForReuse().peekIsEmpty());

    GraphicsAllocation *patternAllocation = csr.getAllocationsForReuse().peekHead();
    ASSERT_NE(nullptr, patternAllocation);

    EXPECT_EQ(AllocationType::fillPattern, patternAllocation->getAllocationType());
}

TEST_F(EnqueueSvmTest, GivenSvmAllocationWhenEnqueingKernelThenSuccessIsReturned) {
    USE_REAL_FILE_SYSTEM();
    auto svmData = context->getSVMAllocsManager()->getSVMAlloc(ptrSVM);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *svmAllocation = svmData->gpuAllocations.getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    EXPECT_NE(nullptr, ptrSVM);

    std::unique_ptr<MockProgram> program(Program::createBuiltInFromSource<MockProgram>("FillBufferBytes", context, context->getDevices(), &retVal));
    program->build(program->getDevices(), nullptr);
    std::unique_ptr<MockKernel> kernel(Kernel::create<MockKernel>(program.get(), program->getKernelInfoForKernel("FillBufferBytes"), *context->getDevice(0), retVal));

    kernel->setSvmKernelExecInfo(svmAllocation);

    size_t offset = 0;
    size_t size = 1;
    retVal = this->pCmdQ->enqueueKernel(
        kernel.get(),
        1,
        &offset,
        &size,
        &size,
        0,
        nullptr,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(1u, kernel->kernelSvmGfxAllocations.size());
}

TEST_F(EnqueueSvmTest, givenEnqueueTaskBlockedOnUserEventWhenItIsEnqueuedThenSurfacesAreMadeResident) {
    USE_REAL_FILE_SYSTEM();
    auto svmData = context->getSVMAllocsManager()->getSVMAlloc(ptrSVM);
    ASSERT_NE(nullptr, svmData);
    GraphicsAllocation *svmAllocation = svmData->gpuAllocations.getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    EXPECT_NE(nullptr, ptrSVM);

    auto program = clUniquePtr(Program::createBuiltInFromSource<MockProgram>("FillBufferBytes", context, context->getDevices(), &retVal));
    program->build(program->getDevices(), nullptr);
    auto pMultiDeviceKernel = clUniquePtr(MultiDeviceKernel::create<MockKernel>(program.get(), program->getKernelInfosForKernel("FillBufferBytes"), retVal));
    auto kernel = static_cast<MockKernel *>(pMultiDeviceKernel->getKernel(rootDeviceIndex));
    std::vector<Surface *> allSurfaces;
    kernel->getResidency(allSurfaces);
    EXPECT_EQ(1u, allSurfaces.size());

    kernel->setSvmKernelExecInfo(svmAllocation);

    auto uEvent = makeReleaseable<UserEvent>();
    cl_event eventWaitList[] = {uEvent.get()};
    size_t offset = 0;
    size_t size = 1;
    retVal = this->pCmdQ->enqueueKernel(
        kernel,
        1,
        &offset,
        &size,
        &size,
        1,
        eventWaitList,
        nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    kernel->getResidency(allSurfaces);
    EXPECT_EQ(3u, allSurfaces.size());

    for (auto &surface : allSurfaces)
        delete surface;

    EXPECT_EQ(1u, kernel->kernelSvmGfxAllocations.size());
    uEvent->setStatus(-1);
}

TEST_F(EnqueueSvmTest, GivenMultipleThreasWhenAllocatingSvmThenOnlyOneAllocationIsCreated) {
    std::atomic<int> flag(0);
    std::atomic<int> ready(0);
    void *svmPtrs[15] = {};

    auto allocSvm = [&](uint32_t from, uint32_t to) {
        for (uint32_t i = from; i <= to; i++) {
            svmPtrs[i] = context->getSVMAllocsManager()->createSVMAlloc(1, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
            auto svmData = context->getSVMAllocsManager()->getSVMAlloc(svmPtrs[i]);
            ASSERT_NE(nullptr, svmData);
            auto ga = svmData->gpuAllocations.getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
            EXPECT_NE(nullptr, ga);
            EXPECT_EQ(ga->getUnderlyingBuffer(), svmPtrs[i]);
        }
    };

    auto freeSvm = [&](uint32_t from, uint32_t to) {
        for (uint32_t i = from; i <= to; i++) {
            context->getSVMAllocsManager()->freeSVMAlloc(svmPtrs[i]);
        }
    };

    auto asyncFcn = [&](bool alloc, uint32_t from, uint32_t to) {
        flag++;
        while (flag < 3)
            ;
        if (alloc) {
            allocSvm(from, to);
        }
        freeSvm(from, to);
        ready++;
    };

    EXPECT_EQ(1u, context->getSVMAllocsManager()->getNumAllocs());

    allocSvm(10, 14);

    auto t1 = std::unique_ptr<std::thread>(new std::thread(asyncFcn, true, 0, 4));
    auto t2 = std::unique_ptr<std::thread>(new std::thread(asyncFcn, true, 5, 9));
    auto t3 = std::unique_ptr<std::thread>(new std::thread(asyncFcn, false, 10, 14));

    while (ready < 3) {
        std::this_thread::yield();
    }

    EXPECT_EQ(1u, context->getSVMAllocsManager()->getNumAllocs());
    t1->join();
    t2->join();
    t3->join();
}

TEST_F(EnqueueSvmTest, GivenValidParamsWhenMigratingMemoryThenSuccessIsReturned) {
    const void *svmPtrs[] = {ptrSVM};
    retVal = this->pCmdQ->enqueueSVMMigrateMem(
        1,       // cl_uint num_svm_pointers
        svmPtrs, // const void **svm_pointers
        nullptr, // const size_t *sizes
        0,       // const cl_mem_migration_flags flags
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_event *event_wait_list
        nullptr  // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

HWTEST_F(EnqueueSvmTest, GivenGpuHangAndBlockingCallAndValidParamsWhenMigratingMemoryThenOutOfResourcesIsReturned) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context, device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    const void *svmPtrs[] = {ptrSVM};
    const auto enqueueResult = mockCommandQueueHw.enqueueSVMMigrateMem(
        1,       // cl_uint num_svm_pointers
        svmPtrs, // const void **svm_pointers
        nullptr, // const size_t *sizes
        0,       // const cl_mem_migration_flags flags
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_event *event_wait_list
        nullptr  // cL_event *event
    );

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

HWTEST_F(EnqueueSvmTest, WhenMigratingMemoryThenSvmMigrateMemCommandTypeIsUsed) {
    MockCommandQueueHw<FamilyType> commandQueue{context, pClDevice, nullptr};
    const void *svmPtrs[] = {ptrSVM};
    retVal = commandQueue.enqueueSVMMigrateMem(
        1,       // cl_uint num_svm_pointers
        svmPtrs, // const void **svm_pointers
        nullptr, // const size_t *sizes
        0,       // const cl_mem_migration_flags flags
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_event *event_wait_list
        nullptr  // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    uint32_t expectedCommandType = CL_COMMAND_SVM_MIGRATE_MEM;
    EXPECT_EQ(expectedCommandType, commandQueue.lastCommandType);
}

TEST(CreateSvmAllocTests, givenVariousSvmAllocationPropertiesWhenAllocatingSvmThenSvmIsCorrectlyAllocated) {
    DebugManagerStateRestore dbgRestore;
    SVMAllocsManager::SvmAllocationProperties svmAllocationProperties;

    for (auto isLocalMemorySupported : ::testing::Bool()) {
        debugManager.flags.EnableLocalMemory.set(isLocalMemorySupported);
        auto mockDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        auto mockContext = std::make_unique<MockContext>(mockDevice.get());

        for (auto isReadOnly : ::testing::Bool()) {
            for (auto isHostPtrReadOnly : ::testing::Bool()) {
                svmAllocationProperties.readOnly = isReadOnly;
                svmAllocationProperties.hostPtrReadOnly = isHostPtrReadOnly;

                auto ptrSVM = mockContext->getSVMAllocsManager()->createSVMAlloc(256, svmAllocationProperties, mockContext->getRootDeviceIndices(), mockContext->getDeviceBitfields());
                EXPECT_NE(nullptr, ptrSVM);
                mockContext->getSVMAllocsManager()->freeSVMAlloc(ptrSVM);
            }
        }
    }
}

struct EnqueueSvmTestLocalMemory : public ClDeviceFixture,
                                   public ::testing::Test {
    void SetUp() override {
        dbgRestore = std::make_unique<DebugManagerStateRestore>();
        debugManager.flags.EnableLocalMemory.set(1);

        ClDeviceFixture::setUp();
        context = std::make_unique<MockContext>(pClDevice, true);
        size = 256;
        svmPtr = context->getSVMAllocsManager()->createSVMAlloc(size, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
        ASSERT_NE(nullptr, svmPtr);
        mockSvmManager = reinterpret_cast<MockSVMAllocsManager *>(context->getSVMAllocsManager());
    }

    void TearDown() override {
        context->getSVMAllocsManager()->freeSVMAlloc(svmPtr);
        context.reset(nullptr);
        ClDeviceFixture::tearDown();
    }

    cl_int retVal = CL_SUCCESS;
    void *svmPtr = nullptr;
    size_t size;
    MockSVMAllocsManager *mockSvmManager;
    std::unique_ptr<DebugManagerStateRestore> dbgRestore;
    std::unique_ptr<MockContext> context;
    HardwareParse hwParse;
};

HWTEST_F(EnqueueSvmTestLocalMemory, givenWriteInvalidateRegionFlagWhenMappingSvmThenMapIsSuccessfulAndReadOnlyFlagIsFalse) {
    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);
    uintptr_t offset = 64;
    void *regionSvmPtr = ptrOffset(svmPtr, offset);
    size_t regionSize = 64;
    retVal = queue.enqueueSVMMap(
        CL_TRUE,
        CL_MAP_WRITE_INVALIDATE_REGION,
        regionSvmPtr,
        regionSize,
        0,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto svmMap = mockSvmManager->svmMapOperations.get(regionSvmPtr);
    EXPECT_FALSE(svmMap->readOnlyMap);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenGpuHangAndBlockingCallAndWriteInvalidateRegionFlagWhenMappingSvmThenOutOfResourcesIsReturned) {
    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);
    queue.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    uintptr_t offset = 64;
    void *regionSvmPtr = ptrOffset(svmPtr, offset);
    size_t regionSize = 64;

    const auto enqueueResult = queue.enqueueSVMMap(
        CL_TRUE,
        CL_MAP_WRITE_INVALIDATE_REGION,
        regionSvmPtr,
        regionSize,
        0,
        nullptr,
        nullptr,
        false);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, queue.waitForAllEnginesCalledCount);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenMapWriteFlagWhenMappingSvmThenMapIsSuccessfulAndReadOnlyFlagIsFalse) {
    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);
    uintptr_t offset = 64;
    void *regionSvmPtr = ptrOffset(svmPtr, offset);
    size_t regionSize = 64;
    retVal = queue.enqueueSVMMap(
        CL_TRUE,
        CL_MAP_WRITE,
        regionSvmPtr,
        regionSize,
        0,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto svmMap = mockSvmManager->svmMapOperations.get(regionSvmPtr);
    EXPECT_FALSE(svmMap->readOnlyMap);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenMapReadFlagWhenMappingSvmThenMapIsSuccessfulAndReadOnlyFlagIsTrue) {
    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);
    uintptr_t offset = 64;
    void *regionSvmPtr = ptrOffset(svmPtr, offset);
    size_t regionSize = 64;
    retVal = queue.enqueueSVMMap(
        CL_TRUE,
        CL_MAP_READ,
        regionSvmPtr,
        regionSize,
        0,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto svmMap = mockSvmManager->svmMapOperations.get(regionSvmPtr);
    EXPECT_TRUE(svmMap->readOnlyMap);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenMapReadAndWriteFlagWhenMappingSvmThenDontSetReadOnlyProperty) {
    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);
    uintptr_t offset = 64;
    void *regionSvmPtr = ptrOffset(svmPtr, offset);
    size_t regionSize = 64;
    retVal = queue.enqueueSVMMap(
        CL_TRUE,
        CL_MAP_READ | CL_MAP_WRITE,
        regionSvmPtr,
        regionSize,
        0,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto svmMap = mockSvmManager->svmMapOperations.get(regionSvmPtr);
    EXPECT_FALSE(svmMap->readOnlyMap);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenSvmAllocWithoutFlagsWhenMappingSvmThenMapIsSuccessfulAndReadOnlyFlagIsTrue) {
    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);
    uintptr_t offset = 64;
    void *regionSvmPtr = ptrOffset(svmPtr, offset);
    size_t regionSize = 64;
    retVal = queue.enqueueSVMMap(
        CL_TRUE,
        0,
        regionSvmPtr,
        regionSize,
        0,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto svmMap = mockSvmManager->svmMapOperations.get(regionSvmPtr);
    EXPECT_FALSE(svmMap->readOnlyMap);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenEnabledLocalMemoryWhenEnqueueMapValidSvmPtrThenExpectSingleWalker) {
    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);
    LinearStream &stream = queue.getCS(0x1000);

    cl_event event = nullptr;

    uintptr_t offset = 64;
    void *regionSvmPtr = ptrOffset(svmPtr, offset);
    size_t regionSize = 64;
    retVal = queue.enqueueSVMMap(
        CL_FALSE,
        CL_MAP_READ,
        regionSvmPtr,
        regionSize,
        0,
        nullptr,
        &event,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto svmMap = mockSvmManager->svmMapOperations.get(regionSvmPtr);
    ASSERT_NE(nullptr, svmMap);
    EXPECT_EQ(regionSvmPtr, svmMap->regionSvmPtr);
    EXPECT_EQ(svmPtr, svmMap->baseSvmPtr);
    EXPECT_EQ(regionSize, svmMap->regionSize);
    EXPECT_EQ(offset, svmMap->offset);
    EXPECT_TRUE(svmMap->readOnlyMap);

    queue.flush();
    hwParse.parseCommands<FamilyType>(stream);
    auto walkerCount = hwParse.getCommandWalkerCount<FamilyType>();
    EXPECT_EQ(1u, walkerCount);

    constexpr cl_command_type expectedCmd = CL_COMMAND_SVM_MAP;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);

    clReleaseEvent(event);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenEnabledLocalMemoryWhenEnqueueMapSvmPtrTwiceThenExpectSingleWalker) {
    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);
    LinearStream &stream = queue.getCS(0x1000);

    uintptr_t offset = 64;
    void *regionSvmPtr = ptrOffset(svmPtr, offset);
    size_t regionSize = 64;
    retVal = queue.enqueueSVMMap(
        CL_FALSE,
        CL_MAP_WRITE,
        regionSvmPtr,
        regionSize,
        0,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    auto svmMap = mockSvmManager->svmMapOperations.get(regionSvmPtr);
    ASSERT_NE(nullptr, svmMap);
    EXPECT_EQ(regionSvmPtr, svmMap->regionSvmPtr);
    EXPECT_EQ(svmPtr, svmMap->baseSvmPtr);
    EXPECT_EQ(regionSize, svmMap->regionSize);
    EXPECT_EQ(offset, svmMap->offset);
    EXPECT_FALSE(svmMap->readOnlyMap);

    EXPECT_EQ(1u, mockSvmManager->svmMapOperations.getNumMapOperations());

    cl_event event = nullptr;
    retVal = queue.enqueueSVMMap(
        CL_FALSE,
        CL_MAP_WRITE,
        regionSvmPtr,
        regionSize,
        0,
        nullptr,
        &event,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockSvmManager->svmMapOperations.getNumMapOperations());

    queue.flush();
    hwParse.parseCommands<FamilyType>(stream);
    auto walkerCount = hwParse.getCommandWalkerCount<FamilyType>();
    EXPECT_EQ(1u, walkerCount);

    constexpr cl_command_type expectedCmd = CL_COMMAND_SVM_MAP;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);

    clReleaseEvent(event);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenEnabledLocalMemoryAndBlockingCallAndGpuHangOnSecondMapWhenEnqueueMapSvmPtrTwiceThenSecondCallReturnsOutOfresources) {
    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::ready;

    uintptr_t offset = 64;
    void *regionSvmPtr = ptrOffset(svmPtr, offset);
    size_t regionSize = 64;

    const auto firstMapResult = mockCommandQueueHw.enqueueSVMMap(
        CL_TRUE,
        CL_MAP_WRITE,
        regionSvmPtr,
        regionSize,
        0,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, firstMapResult);

    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;
    const auto secondMapResult = mockCommandQueueHw.enqueueSVMMap(
        CL_TRUE,
        CL_MAP_WRITE,
        regionSvmPtr,
        regionSize,
        0,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, secondMapResult);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenEnabledLocalMemoryWhenNoMappedSvmPtrThenExpectNoUnmapCopyKernel) {
    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);
    LinearStream &stream = queue.getCS(0x1000);

    cl_event event = nullptr;
    retVal = queue.enqueueSVMUnmap(
        svmPtr,
        0,
        nullptr,
        &event,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    queue.flush();
    hwParse.parseCommands<FamilyType>(stream);
    auto walkerCount = hwParse.getCommandWalkerCount<FamilyType>();
    EXPECT_EQ(0u, walkerCount);

    constexpr cl_command_type expectedCmd = CL_COMMAND_SVM_UNMAP;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);

    clReleaseEvent(event);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenEnabledLocalMemoryAndGpuHangAndBlockingCallWhenUnmappingThenReturnOutOfResources) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;

    const auto enqueueResult = mockCommandQueueHw.enqueueSVMUnmap(
        svmPtr,
        0,
        nullptr,
        nullptr,
        false);

    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueResult);
    EXPECT_EQ(1, mockCommandQueueHw.waitForAllEnginesCalledCount);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenEnabledLocalMemoryWhenMappedSvmRegionIsReadOnlyThenExpectNoUnmapCopyKernel) {

    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);
    LinearStream &stream = queue.getCS(0x1000);

    retVal = queue.enqueueSVMMap(
        CL_FALSE,
        CL_MAP_READ,
        svmPtr,
        size,
        0,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockSvmManager->svmMapOperations.getNumMapOperations());
    auto svmMap = mockSvmManager->svmMapOperations.get(svmPtr);
    ASSERT_NE(nullptr, svmMap);

    queue.flush();
    size_t offset = stream.getUsed();
    hwParse.parseCommands<FamilyType>(stream);
    auto walkerCount = hwParse.getCommandWalkerCount<FamilyType>();
    EXPECT_EQ(1u, walkerCount);
    hwParse.tearDown();

    cl_event event = nullptr;
    retVal = queue.enqueueSVMUnmap(
        svmPtr,
        0,
        nullptr,
        &event,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, mockSvmManager->svmMapOperations.getNumMapOperations());

    queue.flush();
    hwParse.parseCommands<FamilyType>(stream, offset);
    walkerCount = hwParse.getCommandWalkerCount<FamilyType>();
    EXPECT_EQ(0u, walkerCount);

    constexpr cl_command_type expectedCmd = CL_COMMAND_SVM_UNMAP;
    cl_command_type actualCmd = castToObjectOrAbort<Event>(event)->getCommandType();
    EXPECT_EQ(expectedCmd, actualCmd);

    clReleaseEvent(event);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenEnabledLocalMemoryAndBlockingCallAndGpuHangForUnmapWhenUnmapingThenOutOfResourcesIsReturnedFromUnmap) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    std::unique_ptr<ClDevice> device(new MockClDevice{MockClDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr)});
    cl_queue_properties props = {};

    MockCommandQueueHw<FamilyType> mockCommandQueueHw(context.get(), device.get(), &props);
    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::ready;

    const auto enqueueMapResult = mockCommandQueueHw.enqueueSVMMap(
        CL_FALSE,
        CL_MAP_READ,
        svmPtr,
        size,
        0,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, enqueueMapResult);

    mockCommandQueueHw.waitForAllEnginesReturnValue = WaitStatus::gpuHang;
    const auto enqueueUnmapResult = mockCommandQueueHw.enqueueSVMUnmap(
        svmPtr,
        0,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueUnmapResult);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenNonReadOnlyMapWhenUnmappingThenSetAubTbxWritableBeforeUnmapEnqueue) {
    class MyQueue : public MockCommandQueueHw<FamilyType> {
      public:
        using MockCommandQueueHw<FamilyType>::MockCommandQueueHw;

        void enqueueHandlerHook(const unsigned int commandType, const MultiDispatchInfo &dispatchInfo) override {
            waitUntilCompleteCalled++;
            if (allocationToVerify) {
                EXPECT_TRUE(allocationToVerify->isAubWritable(GraphicsAllocation::defaultBank));
                EXPECT_TRUE(allocationToVerify->isTbxWritable(GraphicsAllocation::defaultBank));
            }
        }

        uint32_t waitUntilCompleteCalled = 0;
        GraphicsAllocation *allocationToVerify = nullptr;
    };

    MyQueue myQueue(context.get(), pClDevice, nullptr);

    retVal = myQueue.enqueueSVMMap(CL_TRUE, CL_MAP_WRITE, svmPtr, size, 0, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto gpuAllocation = mockSvmManager->getSVMAlloc(svmPtr)->gpuAllocations.getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());
    myQueue.allocationToVerify = gpuAllocation;

    gpuAllocation->setAubWritable(false, GraphicsAllocation::defaultBank);
    gpuAllocation->setTbxWritable(false, GraphicsAllocation::defaultBank);

    EXPECT_EQ(1u, myQueue.waitUntilCompleteCalled);
    retVal = myQueue.enqueueSVMUnmap(svmPtr, 0, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, myQueue.waitUntilCompleteCalled);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenReadOnlyMapWhenUnmappingThenDontResetAubTbxWritable) {
    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);

    retVal = queue.enqueueSVMMap(CL_TRUE, CL_MAP_READ, svmPtr, size, 0, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto gpuAllocation = mockSvmManager->getSVMAlloc(svmPtr)->gpuAllocations.getGraphicsAllocation(context->getDevice(0)->getRootDeviceIndex());

    gpuAllocation->setAubWritable(false, GraphicsAllocation::defaultBank);
    gpuAllocation->setTbxWritable(false, GraphicsAllocation::defaultBank);

    retVal = queue.enqueueSVMUnmap(svmPtr, 0, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_FALSE(gpuAllocation->isAubWritable(GraphicsAllocation::defaultBank));
    EXPECT_FALSE(gpuAllocation->isTbxWritable(GraphicsAllocation::defaultBank));
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenEnabledLocalMemoryWhenMappedSvmRegionIsWritableThenExpectMapAndUnmapCopyKernel) {
    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);
    LinearStream &stream = queue.getCS(0x1000);

    cl_event eventMap = nullptr;
    retVal = queue.enqueueSVMMap(
        CL_FALSE,
        CL_MAP_WRITE,
        svmPtr,
        size,
        0,
        nullptr,
        &eventMap,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockSvmManager->svmMapOperations.getNumMapOperations());
    auto svmMap = mockSvmManager->svmMapOperations.get(svmPtr);
    ASSERT_NE(nullptr, svmMap);

    cl_event eventUnmap = nullptr;
    retVal = queue.enqueueSVMUnmap(
        svmPtr,
        0,
        nullptr,
        &eventUnmap,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, mockSvmManager->svmMapOperations.getNumMapOperations());

    queue.flush();
    hwParse.parseCommands<FamilyType>(stream);
    auto walkerCount = hwParse.getCommandWalkerCount<FamilyType>();
    EXPECT_EQ(2u, walkerCount);

    constexpr cl_command_type expectedMapCmd = CL_COMMAND_SVM_MAP;
    cl_command_type actualMapCmd = castToObjectOrAbort<Event>(eventMap)->getCommandType();
    EXPECT_EQ(expectedMapCmd, actualMapCmd);

    constexpr cl_command_type expectedUnmapCmd = CL_COMMAND_SVM_UNMAP;
    cl_command_type actualUnmapCmd = castToObjectOrAbort<Event>(eventUnmap)->getCommandType();
    EXPECT_EQ(expectedUnmapCmd, actualUnmapCmd);

    clReleaseEvent(eventMap);
    clReleaseEvent(eventUnmap);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenGpuHangAndBlockingCallAndEnabledLocalMemoryWhenMappedSvmRegionIsWritableThenUnmapReturnsOutOfResources) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.MakeEachEnqueueBlocking.set(true);

    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);
    queue.waitForAllEnginesReturnValue = WaitStatus::ready;

    const auto enqueueMapResult = queue.enqueueSVMMap(
        CL_TRUE,
        CL_MAP_WRITE,
        svmPtr,
        size,
        0,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, enqueueMapResult);

    queue.waitForAllEnginesReturnValue = WaitStatus::gpuHang;
    const auto enqueueUnmapResult = queue.enqueueSVMUnmap(
        svmPtr,
        0,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_OUT_OF_RESOURCES, enqueueUnmapResult);
    EXPECT_EQ(2, queue.waitForAllEnginesCalledCount);
}

HWTEST_F(EnqueueSvmTestLocalMemory, givenEnabledLocalMemoryWhenMappedSvmRegionAndNoEventIsUsedIsWritableThenExpectMapAndUnmapCopyKernelAnNo) {
    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);
    LinearStream &stream = queue.getCS(0x1000);

    retVal = queue.enqueueSVMMap(
        CL_FALSE,
        CL_MAP_WRITE,
        svmPtr,
        size,
        0,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, mockSvmManager->svmMapOperations.getNumMapOperations());
    auto svmMap = mockSvmManager->svmMapOperations.get(svmPtr);
    ASSERT_NE(nullptr, svmMap);

    retVal = queue.enqueueSVMUnmap(
        svmPtr,
        0,
        nullptr,
        nullptr,
        false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, mockSvmManager->svmMapOperations.getNumMapOperations());

    queue.flush();
    hwParse.parseCommands<FamilyType>(stream);
    auto walkerCount = hwParse.getCommandWalkerCount<FamilyType>();
    EXPECT_EQ(2u, walkerCount);
}

struct StatelessMockAlignedMallocMemoryManagerEnqueueSvmTestLocalMemory : public EnqueueSvmTestLocalMemory {
    void SetUp() override {
        if (is32bit) {
            GTEST_SKIP();
        }
        EnqueueSvmTestLocalMemory::SetUp();

        alignedMemoryManager = std::make_unique<MockAlignMallocMemoryManager>(*pClExecutionEnvironment, true);

        memoryManagerBackup = mockSvmManager->memoryManager;
        mockSvmManager->memoryManager = alignedMemoryManager.get();

        size = static_cast<size_t>(4ull * MemoryConstants::gigaByte);
    }

    void TearDown() override {
        if (is32bit) {
            GTEST_SKIP();
        }

        mockSvmManager->memoryManager = memoryManagerBackup;
        EnqueueSvmTestLocalMemory::TearDown();
    }

  private:
    std::unique_ptr<MockAlignMallocMemoryManager> alignedMemoryManager;
    MemoryManager *memoryManagerBackup = nullptr;
};

HWTEST_F(StatelessMockAlignedMallocMemoryManagerEnqueueSvmTestLocalMemory, given4gbBufferAndIsForceStatelessIsFalseWhenEnqueueSvmMapCallThenStatelessIsUsed) {
    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);

    EBuiltInOps::Type copyBuiltIn = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToBuffer>(true, queue.getHeaplessModeEnabled());

    auto builtIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(queue.getDevice().getExecutionEnvironment()->rootDeviceEnvironments[queue.getDevice().getRootDeviceIndex()].get(), builtIns);

    // substitute original builder with mock builder
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        copyBuiltIn,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuilder(*builtIns, queue.getClDevice())));

    auto svmPtr4gb = mockSvmManager->createSVMAlloc(static_cast<size_t>(4ull * MemoryConstants::gigaByte), {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
    EXPECT_NE(svmPtr4gb, nullptr);

    auto mockBuilder = static_cast<MockBuilder *>(&BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        copyBuiltIn,
        *pClDevice));

    EXPECT_FALSE(mockBuilder->wasBuildDispatchInfosWithBuiltinOpParamsCalled);
    retVal = queue.enqueueSVMMap(CL_TRUE, CL_MAP_READ, svmPtr4gb, size, 0, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockBuilder->wasBuildDispatchInfosWithBuiltinOpParamsCalled);

    retVal = queue.enqueueSVMUnmap(svmPtr4gb, 0, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // restore original builder and retrieve mock builder
    auto newBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        copyBuiltIn,
        std::move(oldBuilder));
    EXPECT_EQ(mockBuilder, newBuilder.get());

    mockSvmManager->freeSVMAlloc(svmPtr4gb);
}

HWTEST_F(StatelessMockAlignedMallocMemoryManagerEnqueueSvmTestLocalMemory, given4gbBufferAndIsForceStatelessIsFalseWhenEnqueueSvmUnMapCallThenStatelessIsUsed) {
    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);

    EBuiltInOps::Type copyBuiltIn = EBuiltInOps::adjustBuiltinType<EBuiltInOps::copyBufferToBuffer>(true, queue.getHeaplessModeEnabled());

    auto builtIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(queue.getDevice().getExecutionEnvironment()->rootDeviceEnvironments[queue.getDevice().getRootDeviceIndex()].get(), builtIns);

    auto svmPtr4gb = mockSvmManager->createSVMAlloc(static_cast<size_t>(4ull * MemoryConstants::gigaByte), {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
    EXPECT_NE(svmPtr4gb, nullptr);

    retVal = queue.enqueueSVMMap(CL_TRUE, CL_MAP_WRITE, svmPtr4gb, size, 0, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // substitute original builder with mock builder
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        copyBuiltIn,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuilder(*builtIns, queue.getClDevice())));

    auto mockBuilder = static_cast<MockBuilder *>(&BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        copyBuiltIn,
        *pClDevice));

    EXPECT_FALSE(mockBuilder->wasBuildDispatchInfosWithBuiltinOpParamsCalled);
    retVal = queue.enqueueSVMUnmap(svmPtr4gb, 0, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockBuilder->wasBuildDispatchInfosWithBuiltinOpParamsCalled);

    // restore original builder and retrieve mock builder
    auto newBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        copyBuiltIn,
        std::move(oldBuilder));
    EXPECT_EQ(mockBuilder, newBuilder.get());

    mockSvmManager->freeSVMAlloc(svmPtr4gb);
}

HWTEST_F(StatelessMockAlignedMallocMemoryManagerEnqueueSvmTestLocalMemory, given4gbBufferAndIsForceStatelessIsFalseWhenEnqueueSvmMemFillCallThenStatelessIsUsed) {
    MockCommandQueueHw<FamilyType> queue(context.get(), pClDevice, nullptr);

    EBuiltInOps::Type copyBuiltIn = EBuiltInOps::adjustBuiltinType<EBuiltInOps::fillBuffer>(true, queue.getHeaplessModeEnabled());

    auto builtIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(queue.getDevice().getExecutionEnvironment()->rootDeviceEnvironments[queue.getDevice().getRootDeviceIndex()].get(), builtIns);

    auto svmPtr4gb = mockSvmManager->createSVMAlloc(static_cast<size_t>(4ull * MemoryConstants::gigaByte), {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
    EXPECT_NE(svmPtr4gb, nullptr);

    retVal = queue.enqueueSVMMap(CL_TRUE, CL_MAP_WRITE, svmPtr4gb, size, 0, nullptr, nullptr, false);
    EXPECT_EQ(CL_SUCCESS, retVal);

    // substitute original builder with mock builder
    auto oldBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        copyBuiltIn,
        std::unique_ptr<NEO::BuiltinDispatchInfoBuilder>(new MockBuilder(*builtIns, queue.getClDevice())));

    auto mockBuilder = static_cast<MockBuilder *>(&BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(
        copyBuiltIn,
        *pClDevice));

    EXPECT_FALSE(mockBuilder->wasBuildDispatchInfosWithBuiltinOpParamsCalled);
    constexpr float pattern[1] = {1.2345f};
    constexpr size_t patternSize = sizeof(pattern);
    retVal = queue.enqueueSVMMemFill(svmPtr4gb, pattern, patternSize, static_cast<size_t>(4ull * MemoryConstants::gigaByte), 0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(mockBuilder->wasBuildDispatchInfosWithBuiltinOpParamsCalled);

    // restore original builder and retrieve mock builder
    auto newBuilder = pClExecutionEnvironment->setBuiltinDispatchInfoBuilder(
        rootDeviceIndex,
        copyBuiltIn,
        std::move(oldBuilder));
    EXPECT_EQ(mockBuilder, newBuilder.get());

    mockSvmManager->freeSVMAlloc(svmPtr4gb);
}

template <typename GfxFamily>
struct FailCsr : public CommandStreamReceiverHw<GfxFamily> {
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw;

    bool createAllocationForHostSurface(HostPtrSurface &surface, bool requiresL3Flush) override {
        return CL_FALSE;
    }
};

HWTEST_F(EnqueueSvmTest, whenInternalAllocationsAreMadeResidentThenOnlyNonSvmAllocationsAreAdded) {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, context->getRootDeviceIndices(), context->getDeviceBitfields());
    unifiedMemoryProperties.device = pDevice;
    auto allocationSize = 4096u;
    auto svmManager = this->context->getSVMAllocsManager();
    EXPECT_NE(0u, svmManager->getNumAllocs());
    auto unifiedMemoryPtr = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
    EXPECT_NE(nullptr, unifiedMemoryPtr);
    EXPECT_EQ(2u, svmManager->getNumAllocs());

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();

    auto &residentAllocations = commandStreamReceiver.getResidencyAllocations();
    EXPECT_EQ(0u, residentAllocations.size());

    svmManager->makeInternalAllocationsResident(commandStreamReceiver, static_cast<uint32_t>(InternalMemoryType::deviceUnifiedMemory));

    // only unified memory allocation is made resident
    EXPECT_EQ(1u, residentAllocations.size());
    EXPECT_EQ(residentAllocations[0]->getGpuAddress(), castToUint64(unifiedMemoryPtr));

    svmManager->freeSVMAlloc(unifiedMemoryPtr);
}

HWTEST_F(EnqueueSvmTest, whenInternalAllocationsAreAddedToResidencyContainerThenOnlyExpectedAllocationsAreAdded) {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, context->getRootDeviceIndices(), context->getDeviceBitfields());
    unifiedMemoryProperties.device = pDevice;
    auto allocationSize = 4096u;
    auto svmManager = this->context->getSVMAllocsManager();
    EXPECT_NE(0u, svmManager->getNumAllocs());
    auto unifiedMemoryPtr = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
    EXPECT_NE(nullptr, unifiedMemoryPtr);
    EXPECT_EQ(2u, svmManager->getNumAllocs());

    ResidencyContainer residencyContainer;
    EXPECT_EQ(0u, residencyContainer.size());

    svmManager->addInternalAllocationsToResidencyContainer(pDevice->getRootDeviceIndex(),
                                                           residencyContainer,
                                                           static_cast<uint32_t>(InternalMemoryType::deviceUnifiedMemory));

    // only unified memory allocation is added to residency container
    EXPECT_EQ(1u, residencyContainer.size());
    EXPECT_EQ(residencyContainer[0]->getGpuAddress(), castToUint64(unifiedMemoryPtr));

    svmManager->freeSVMAlloc(unifiedMemoryPtr);
}

HWTEST_F(EnqueueSvmTest, whenInternalAllocationIsTriedToBeAddedTwiceToResidencyContainerThenItIsAdded) {
    SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, context->getRootDeviceIndices(), context->getDeviceBitfields());
    unifiedMemoryProperties.device = pDevice;
    auto allocationSize = 4096u;
    auto svmManager = this->context->getSVMAllocsManager();
    EXPECT_NE(0u, svmManager->getNumAllocs());
    auto unifiedMemoryPtr = svmManager->createUnifiedMemoryAllocation(allocationSize, unifiedMemoryProperties);
    EXPECT_NE(nullptr, unifiedMemoryPtr);
    EXPECT_EQ(2u, svmManager->getNumAllocs());

    ResidencyContainer residencyContainer;
    EXPECT_EQ(0u, residencyContainer.size());

    svmManager->addInternalAllocationsToResidencyContainer(pDevice->getRootDeviceIndex(),
                                                           residencyContainer,
                                                           static_cast<uint32_t>(InternalMemoryType::deviceUnifiedMemory));

    // only unified memory allocation is added to residency container
    EXPECT_EQ(1u, residencyContainer.size());
    EXPECT_EQ(residencyContainer[0]->getGpuAddress(), castToUint64(unifiedMemoryPtr));

    svmManager->addInternalAllocationsToResidencyContainer(pDevice->getRootDeviceIndex(),
                                                           residencyContainer,
                                                           static_cast<uint32_t>(InternalMemoryType::deviceUnifiedMemory));
    EXPECT_EQ(2u, residencyContainer.size());

    svmManager->freeSVMAlloc(unifiedMemoryPtr);
}

struct CreateHostUnifiedMemoryAllocationTest : public ::testing::Test {
    void SetUp() override {
        device0 = context.pRootDevice0;
        device1 = context.pRootDevice1;
        device2 = context.pRootDevice2;
        svmManager = context.getSVMAllocsManager();
        EXPECT_EQ(0u, svmManager->getNumAllocs());
    }
    const size_t allocationSize = 4096u;
    const uint32_t numDevices = 3u;
    MockDefaultContext context;
    MockClDevice *device2;
    MockClDevice *device1;
    MockClDevice *device0;
    SVMAllocsManager *svmManager = nullptr;
};

HWTEST_F(CreateHostUnifiedMemoryAllocationTest,
         whenCreatingHostUnifiedMemoryAllocationThenOneAllocDataIsCreatedWithOneGraphicsAllocationPerDevice) {

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, context.getRootDeviceIndices(), context.getDeviceBitfields());

    EXPECT_EQ(0u, svmManager->getNumAllocs());
    auto unifiedMemoryPtr = svmManager->createHostUnifiedMemoryAllocation(allocationSize,
                                                                          unifiedMemoryProperties);
    EXPECT_NE(nullptr, unifiedMemoryPtr);
    EXPECT_EQ(1u, svmManager->getNumAllocs());

    auto allocData = svmManager->getSVMAlloc(unifiedMemoryPtr);
    EXPECT_EQ(numDevices, allocData->gpuAllocations.getGraphicsAllocations().size());

    for (uint32_t i = 0; i < allocData->gpuAllocations.getGraphicsAllocations().size(); i++) {
        auto alloc = allocData->gpuAllocations.getGraphicsAllocation(i);
        EXPECT_EQ(i, alloc->getRootDeviceIndex());
    }

    svmManager->freeSVMAlloc(unifiedMemoryPtr);
}

HWTEST_F(CreateHostUnifiedMemoryAllocationTest,
         whenCreatingMultiGraphicsAllocationThenGraphicsAllocationPerDeviceIsCreated) {

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, context.getRootDeviceIndices(), context.getDeviceBitfields());

    auto alignedSize = alignUp<size_t>(allocationSize, MemoryConstants::pageSize64k);
    auto memoryManager = context.getMemoryManager();
    auto allocationType = AllocationType::bufferHostMemory;
    auto maxRootDeviceIndex = numDevices - 1u;

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0u);
    rootDeviceIndices.pushUnique(1u);
    rootDeviceIndices.pushUnique(2u);

    auto rootDeviceIndex = rootDeviceIndices.at(0);
    auto deviceBitfield = device0->getDeviceBitfield();
    AllocationProperties allocationProperties{rootDeviceIndex,
                                              true,
                                              alignedSize,
                                              allocationType,
                                              deviceBitfield.count() > 1,
                                              deviceBitfield.count() > 1,
                                              deviceBitfield};
    allocationProperties.flags.shareable = unifiedMemoryProperties.allocationFlags.flags.shareable;

    SvmAllocationData allocData(maxRootDeviceIndex);

    void *unifiedMemoryPtr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, allocationProperties, allocData.gpuAllocations);

    EXPECT_NE(nullptr, unifiedMemoryPtr);
    EXPECT_EQ(numDevices, allocData.gpuAllocations.getGraphicsAllocations().size());

    for (auto rootDeviceIndex = 0u; rootDeviceIndex <= maxRootDeviceIndex; rootDeviceIndex++) {
        auto alloc = allocData.gpuAllocations.getGraphicsAllocation(rootDeviceIndex);

        EXPECT_NE(nullptr, alloc);
        EXPECT_EQ(rootDeviceIndex, alloc->getRootDeviceIndex());
    }

    for (auto gpuAllocation : allocData.gpuAllocations.getGraphicsAllocations()) {
        memoryManager->freeGraphicsMemory(gpuAllocation);
    }
}

HWTEST_F(CreateHostUnifiedMemoryAllocationTest,
         whenCreatingMultiGraphicsAllocationForSpecificRootDeviceIndicesThenOnlyGraphicsAllocationPerSpecificRootDeviceIndexIsCreated) {

    NEO::SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::hostUnifiedMemory, 1, context.getRootDeviceIndices(), context.getDeviceBitfields());

    auto alignedSize = alignUp<size_t>(allocationSize, MemoryConstants::pageSize64k);
    auto memoryManager = context.getMemoryManager();
    auto allocationType = AllocationType::bufferHostMemory;
    auto maxRootDeviceIndex = numDevices - 1u;

    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(0u);
    rootDeviceIndices.pushUnique(2u);

    auto noProgramedRootDeviceIndex = 1u;
    auto rootDeviceIndex = rootDeviceIndices.at(0);
    auto deviceBitfield = device0->getDeviceBitfield();
    AllocationProperties allocationProperties{rootDeviceIndex,
                                              true,
                                              alignedSize,
                                              allocationType,
                                              deviceBitfield.count() > 1,
                                              deviceBitfield.count() > 1,
                                              deviceBitfield};
    allocationProperties.flags.shareable = unifiedMemoryProperties.allocationFlags.flags.shareable;

    SvmAllocationData allocData(maxRootDeviceIndex);

    void *unifiedMemoryPtr = memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, allocationProperties, allocData.gpuAllocations);

    EXPECT_NE(nullptr, unifiedMemoryPtr);
    EXPECT_EQ(numDevices, allocData.gpuAllocations.getGraphicsAllocations().size());

    for (auto rootDeviceIndex = 0u; rootDeviceIndex <= maxRootDeviceIndex; rootDeviceIndex++) {
        auto alloc = allocData.gpuAllocations.getGraphicsAllocation(rootDeviceIndex);

        if (rootDeviceIndex == noProgramedRootDeviceIndex) {
            EXPECT_EQ(nullptr, alloc);
        } else {
            EXPECT_NE(nullptr, alloc);
            EXPECT_EQ(rootDeviceIndex, alloc->getRootDeviceIndex());
        }
    }

    for (auto gpuAllocation : allocData.gpuAllocations.getGraphicsAllocations()) {
        memoryManager->freeGraphicsMemory(gpuAllocation);
    }
}

struct MemoryAllocationTypeArray {
    const InternalMemoryType allocationType[3] = {InternalMemoryType::hostUnifiedMemory,
                                                  InternalMemoryType::deviceUnifiedMemory,
                                                  InternalMemoryType::sharedUnifiedMemory};
};

struct UpdateResidencyContainerMultipleDevicesTest : public ::testing::WithParamInterface<std::tuple<InternalMemoryType, InternalMemoryType>>,
                                                     public ::testing::Test {
    void SetUp() override {
        device = context.pRootDevice0;
        subDevice0 = context.pSubDevice00;
        subDevice1 = context.pSubDevice01;

        peerDevice = context.pRootDevice1;
        peerSubDevice0 = context.pSubDevice10;
        peerSubDevice1 = context.pSubDevice11;

        svmManager = context.getSVMAllocsManager();
        EXPECT_EQ(0u, svmManager->getNumAllocs());
    }
    MockUnrestrictiveContextMultiGPU context;
    MockClDevice *device;
    ClDevice *subDevice0 = nullptr;
    ClDevice *subDevice1 = nullptr;
    MockClDevice *peerDevice;
    ClDevice *peerSubDevice0 = nullptr;
    ClDevice *peerSubDevice1 = nullptr;
    SVMAllocsManager *svmManager = nullptr;
    const uint32_t numRootDevices = 2;
    const uint32_t maxRootDeviceIndex = numRootDevices - 1;
};

HWTEST_F(UpdateResidencyContainerMultipleDevicesTest,
         givenNoAllocationsCreatedThenNoInternalAllocationsAreAddedToResidencyContainer) {
    ResidencyContainer residencyContainer;
    EXPECT_EQ(0u, residencyContainer.size());
    svmManager->addInternalAllocationsToResidencyContainer(device->getDevice().getRootDeviceIndex(),
                                                           residencyContainer,
                                                           static_cast<uint32_t>(InternalMemoryType::deviceUnifiedMemory));
    EXPECT_EQ(0u, residencyContainer.size());
}

HWTEST_P(UpdateResidencyContainerMultipleDevicesTest, givenAllocationThenItIsAddedToContainerOnlyIfMaskMatches) {
    uint32_t pCmdBuffer[1024];
    MockGraphicsAllocation gfxAllocation(device->getDevice().getRootDeviceIndex(),
                                         static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));

    InternalMemoryType type = std::get<0>(GetParam());
    uint32_t mask = static_cast<uint32_t>(std::get<1>(GetParam()));

    SvmAllocationData allocData(maxRootDeviceIndex);
    allocData.gpuAllocations.addAllocation(&gfxAllocation);
    allocData.memoryType = type;
    allocData.device = &device->getDevice();

    svmManager->insertSVMAlloc(allocData);
    EXPECT_EQ(1u, svmManager->getNumAllocs());

    ResidencyContainer residencyContainer;
    EXPECT_EQ(0u, residencyContainer.size());

    svmManager->addInternalAllocationsToResidencyContainer(device->getDevice().getRootDeviceIndex(),
                                                           residencyContainer,
                                                           mask);

    if (mask == static_cast<uint32_t>(type)) {
        EXPECT_EQ(1u, residencyContainer.size());
        EXPECT_EQ(residencyContainer[0]->getGpuAddress(), gfxAllocation.getGpuAddress());
    } else {
        EXPECT_EQ(0u, residencyContainer.size());
    }
}

HWTEST_P(UpdateResidencyContainerMultipleDevicesTest,
         whenUsingRootDeviceIndexGreaterThanMultiGraphicsAllocationSizeThenNoAllocationsAreAdded) {
    uint32_t pCmdBuffer[1024];
    MockGraphicsAllocation gfxAllocation(device->getDevice().getRootDeviceIndex(),
                                         static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    SvmAllocationData allocData(maxRootDeviceIndex);
    allocData.gpuAllocations.addAllocation(&gfxAllocation);
    allocData.memoryType = InternalMemoryType::deviceUnifiedMemory;
    allocData.device = &device->getDevice();
    allocData.setAllocId(1u);

    uint32_t pCmdBufferPeer[1024];
    MockGraphicsAllocation gfxAllocationPeer(peerDevice->getDevice().getRootDeviceIndex(),
                                             (void *)pCmdBufferPeer, sizeof(pCmdBufferPeer));
    SvmAllocationData allocDataPeer(maxRootDeviceIndex);
    allocDataPeer.gpuAllocations.addAllocation(&gfxAllocationPeer);
    allocDataPeer.memoryType = InternalMemoryType::deviceUnifiedMemory;
    allocDataPeer.device = &peerDevice->getDevice();
    allocDataPeer.setAllocId(2u);
    svmManager->insertSVMAlloc(allocData);
    svmManager->insertSVMAlloc(allocDataPeer);
    EXPECT_EQ(2u, svmManager->getNumAllocs());

    ResidencyContainer residencyContainer;
    EXPECT_EQ(0u, residencyContainer.size());
    svmManager->addInternalAllocationsToResidencyContainer(numRootDevices + 1,
                                                           residencyContainer,
                                                           static_cast<uint32_t>(InternalMemoryType::deviceUnifiedMemory));
    EXPECT_EQ(0u, residencyContainer.size());

    svmManager->addInternalAllocationsToResidencyContainer(device->getDevice().getRootDeviceIndex(),
                                                           residencyContainer,
                                                           static_cast<uint32_t>(InternalMemoryType::deviceUnifiedMemory));
    EXPECT_EQ(1u, residencyContainer.size());
    EXPECT_EQ(residencyContainer[0]->getGpuAddress(), gfxAllocation.getGpuAddress());
}

MemoryAllocationTypeArray memoryTypeArray;
INSTANTIATE_TEST_SUITE_P(UpdateResidencyContainerMultipleDevicesTests,
                         UpdateResidencyContainerMultipleDevicesTest,
                         ::testing::Combine(
                             ::testing::ValuesIn(memoryTypeArray.allocationType),
                             ::testing::ValuesIn(memoryTypeArray.allocationType)));

HWTEST_F(UpdateResidencyContainerMultipleDevicesTest,
         whenInternalAllocationsAreAddedToResidencyContainerThenOnlyAllocationsFromSameDeviceAreAdded) {

    uint32_t pCmdBuffer[1024];
    MockGraphicsAllocation gfxAllocation(device->getDevice().getRootDeviceIndex(),
                                         static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    SvmAllocationData allocData(maxRootDeviceIndex);
    allocData.gpuAllocations.addAllocation(&gfxAllocation);
    allocData.memoryType = InternalMemoryType::deviceUnifiedMemory;
    allocData.device = &device->getDevice();
    allocData.setAllocId(1u);

    uint32_t pCmdBufferPeer[1024];
    MockGraphicsAllocation gfxAllocationPeer(peerDevice->getDevice().getRootDeviceIndex(),
                                             (void *)pCmdBufferPeer, sizeof(pCmdBufferPeer));
    SvmAllocationData allocDataPeer(maxRootDeviceIndex);
    allocDataPeer.gpuAllocations.addAllocation(&gfxAllocationPeer);
    allocDataPeer.memoryType = InternalMemoryType::deviceUnifiedMemory;
    allocDataPeer.device = &peerDevice->getDevice();
    allocDataPeer.setAllocId(2u);

    svmManager->insertSVMAlloc(allocData);
    svmManager->insertSVMAlloc(allocDataPeer);
    EXPECT_EQ(2u, svmManager->getNumAllocs());

    ResidencyContainer residencyContainer;
    EXPECT_EQ(0u, residencyContainer.size());
    svmManager->addInternalAllocationsToResidencyContainer(device->getDevice().getRootDeviceIndex(),
                                                           residencyContainer,
                                                           static_cast<uint32_t>(InternalMemoryType::deviceUnifiedMemory));
    EXPECT_EQ(1u, residencyContainer.size());
    EXPECT_EQ(residencyContainer[0]->getGpuAddress(), gfxAllocation.getGpuAddress());
}

HWTEST_F(UpdateResidencyContainerMultipleDevicesTest,
         givenSharedAllocationWithNullDevicePointerThenAllocationIsAddedToResidencyContainer) {

    uint32_t pCmdBuffer[1024];
    MockGraphicsAllocation gfxAllocation(device->getDevice().getRootDeviceIndex(), static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    SvmAllocationData allocData(maxRootDeviceIndex);
    allocData.gpuAllocations.addAllocation(&gfxAllocation);
    allocData.memoryType = InternalMemoryType::sharedUnifiedMemory;
    allocData.device = nullptr;

    svmManager->insertSVMAlloc(allocData);
    EXPECT_EQ(1u, svmManager->getNumAllocs());

    ResidencyContainer residencyContainer;
    EXPECT_EQ(0u, residencyContainer.size());
    svmManager->addInternalAllocationsToResidencyContainer(device->getDevice().getRootDeviceIndex(),
                                                           residencyContainer,
                                                           static_cast<uint32_t>(InternalMemoryType::sharedUnifiedMemory));
    EXPECT_EQ(1u, residencyContainer.size());
    EXPECT_EQ(residencyContainer[0]->getGpuAddress(), gfxAllocation.getGpuAddress());
}

HWTEST_F(UpdateResidencyContainerMultipleDevicesTest,
         givenSharedAllocationWithNonNullDevicePointerAndDifferentDeviceToOnePassedToResidencyCallThenAllocationIsNotAddedToResidencyContainer) {

    uint32_t pCmdBuffer[1024];
    MockGraphicsAllocation gfxAllocation(peerDevice->getDevice().getRootDeviceIndex(),
                                         static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    SvmAllocationData allocData(maxRootDeviceIndex);
    allocData.gpuAllocations.addAllocation(&gfxAllocation);
    allocData.memoryType = InternalMemoryType::sharedUnifiedMemory;
    allocData.device = &peerDevice->getDevice();

    svmManager->insertSVMAlloc(allocData);
    EXPECT_EQ(1u, svmManager->getNumAllocs());

    ResidencyContainer residencyContainer;
    EXPECT_EQ(0u, residencyContainer.size());
    svmManager->addInternalAllocationsToResidencyContainer(device->getDevice().getRootDeviceIndex(),
                                                           residencyContainer,
                                                           static_cast<uint32_t>(InternalMemoryType::sharedUnifiedMemory));
    EXPECT_EQ(0u, residencyContainer.size());
}

HWTEST_F(UpdateResidencyContainerMultipleDevicesTest,
         givenAllocationsFromSubDevicesBelongingToTheSameTargetDeviceThenTheyAreAddedToTheResidencyContainer) {

    uint32_t pCmdBuffer[1024];
    MockGraphicsAllocation gfxAllocation(device->getDevice().getRootDeviceIndex(),
                                         static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    SvmAllocationData allocData0(maxRootDeviceIndex);
    allocData0.gpuAllocations.addAllocation(&gfxAllocation);
    allocData0.memoryType = InternalMemoryType::deviceUnifiedMemory;
    allocData0.device = &subDevice0->getDevice();
    allocData0.setAllocId(1u);

    uint32_t pCmdBufferPeer[1024];
    MockGraphicsAllocation gfxAllocationPeer(device->getDevice().getRootDeviceIndex(),
                                             (void *)pCmdBufferPeer, sizeof(pCmdBufferPeer));
    SvmAllocationData allocData1(maxRootDeviceIndex);
    allocData1.gpuAllocations.addAllocation(&gfxAllocationPeer);
    allocData1.memoryType = InternalMemoryType::deviceUnifiedMemory;
    allocData1.device = &subDevice1->getDevice();
    allocData1.setAllocId(2u);

    svmManager->insertSVMAlloc(allocData0);
    svmManager->insertSVMAlloc(allocData1);
    EXPECT_EQ(2u, svmManager->getNumAllocs());

    ResidencyContainer residencyContainer;
    EXPECT_EQ(0u, residencyContainer.size());
    svmManager->addInternalAllocationsToResidencyContainer(device->getDevice().getRootDeviceIndex(),
                                                           residencyContainer,
                                                           static_cast<uint32_t>(InternalMemoryType::deviceUnifiedMemory));
    EXPECT_EQ(2u, residencyContainer.size());
}

HWTEST_F(UpdateResidencyContainerMultipleDevicesTest,
         givenAllocationsFromSubDevicesNotBelongingToTheSameTargetDeviceThenTheyAreNotAddedToTheResidencyContainer) {

    uint32_t pCmdBuffer[1024];
    MockGraphicsAllocation gfxAllocation(device->getDevice().getRootDeviceIndex(),
                                         static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    SvmAllocationData allocData0(maxRootDeviceIndex);
    allocData0.gpuAllocations.addAllocation(&gfxAllocation);
    allocData0.memoryType = InternalMemoryType::deviceUnifiedMemory;
    allocData0.device = &subDevice0->getDevice();
    allocData0.setAllocId(1u);

    uint32_t pCmdBufferPeer[1024];
    MockGraphicsAllocation gfxAllocationPeer(device->getDevice().getRootDeviceIndex(),
                                             (void *)pCmdBufferPeer, sizeof(pCmdBufferPeer));
    SvmAllocationData allocData1(maxRootDeviceIndex);
    allocData1.gpuAllocations.addAllocation(&gfxAllocationPeer);
    allocData1.memoryType = InternalMemoryType::deviceUnifiedMemory;
    allocData1.device = &subDevice1->getDevice();
    allocData1.setAllocId(2u);

    svmManager->insertSVMAlloc(allocData0);
    svmManager->insertSVMAlloc(allocData1);
    EXPECT_EQ(2u, svmManager->getNumAllocs());

    ResidencyContainer residencyContainer;
    EXPECT_EQ(0u, residencyContainer.size());
    svmManager->addInternalAllocationsToResidencyContainer(peerDevice->getDevice().getRootDeviceIndex(),
                                                           residencyContainer,
                                                           static_cast<uint32_t>(InternalMemoryType::deviceUnifiedMemory));
    EXPECT_EQ(0u, residencyContainer.size());
}

HWTEST_F(EnqueueSvmTest, GivenDstHostPtrWhenHostPtrAllocationCreationFailsThenReturnOutOfResource) {
    char dstHostPtr[260];
    void *pDstSVM = dstHostPtr;
    void *pSrcSVM = ptrSVM;
    MockCommandQueueHw<FamilyType> cmdQ(context, pClDevice, nullptr);
    auto failCsr = std::make_unique<FailCsr<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    CommandStreamReceiver *oldCommandStreamReceiver = &cmdQ.getGpgpuCommandStreamReceiver();
    cmdQ.gpgpuEngine->commandStreamReceiver = failCsr.get();
    retVal = cmdQ.enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
    cmdQ.gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
}

HWTEST_F(EnqueueSvmTest, GivenSrcHostPtrAndSizeZeroWhenHostPtrAllocationCreationFailsThenReturnOutOfResource) {
    char srcHostPtr[260];
    void *pDstSVM = ptrSVM;
    void *pSrcSVM = srcHostPtr;
    MockCommandQueueHw<FamilyType> cmdQ(context, pClDevice, nullptr);
    auto failCsr = std::make_unique<FailCsr<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    CommandStreamReceiver *oldCommandStreamReceiver = &cmdQ.getGpgpuCommandStreamReceiver();
    cmdQ.gpgpuEngine->commandStreamReceiver = failCsr.get();
    retVal = cmdQ.enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
    cmdQ.gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
}

HWTEST_F(EnqueueSvmTest, givenDstHostPtrAndSrcHostPtrWhenHostPtrAllocationCreationFailsThenReturnOutOfResource) {
    char dstHostPtr[260];
    char srcHostPtr[260];
    void *pDstSVM = dstHostPtr;
    void *pSrcSVM = srcHostPtr;
    MockCommandQueueHw<FamilyType> cmdQ(context, pClDevice, nullptr);
    auto failCsr = std::make_unique<FailCsr<FamilyType>>(*pDevice->getExecutionEnvironment(), pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    CommandStreamReceiver *oldCommandStreamReceiver = &cmdQ.getGpgpuCommandStreamReceiver();
    cmdQ.gpgpuEngine->commandStreamReceiver = failCsr.get();
    retVal = cmdQ.enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr, // cL_event *event
        nullptr  // CommandStreamReceiver* csrParam
    );
    EXPECT_EQ(CL_OUT_OF_RESOURCES, retVal);
    cmdQ.gpgpuEngine->commandStreamReceiver = oldCommandStreamReceiver;
}

TEST_F(EnqueueSvmTest, givenPageFaultManagerWhenEnqueueMemcpyThenAllocIsDecommitted) {
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    mockMemoryManager->pageFaultManager.reset(new MockPageFaultManager());
    auto memoryManager = context->getMemoryManager();
    context->memoryManager = mockMemoryManager.get();
    auto srcSvm = context->getSVMAllocsManager()->createSVMAlloc(256, {}, context->getRootDeviceIndices(), context->getDeviceBitfields());
    mockMemoryManager->getPageFaultManager()->insertAllocation(srcSvm, 256, context->getSVMAllocsManager(), context->getSpecialQueue(pDevice->getRootDeviceIndex()), {});
    mockMemoryManager->getPageFaultManager()->insertAllocation(ptrSVM, 256, context->getSVMAllocsManager(), context->getSpecialQueue(pDevice->getRootDeviceIndex()), {});
    EXPECT_EQ(static_cast<MockPageFaultManager *>(mockMemoryManager->getPageFaultManager())->transferToCpuCalled, 0);

    this->pCmdQ->enqueueSVMMemcpy(false, ptrSVM, srcSvm, 256, 0, nullptr, nullptr, nullptr);

    EXPECT_EQ(static_cast<MockPageFaultManager *>(mockMemoryManager->getPageFaultManager())->allowMemoryAccessCalled, 0);
    EXPECT_EQ(static_cast<MockPageFaultManager *>(mockMemoryManager->getPageFaultManager())->protectMemoryCalled, 2);
    EXPECT_EQ(static_cast<MockPageFaultManager *>(mockMemoryManager->getPageFaultManager())->transferToCpuCalled, 0);
    EXPECT_EQ(static_cast<MockPageFaultManager *>(mockMemoryManager->getPageFaultManager())->transferToGpuCalled, 2);

    context->getSVMAllocsManager()->freeSVMAlloc(srcSvm);
    context->memoryManager = memoryManager;
}

TEST_F(EnqueueSvmTest, givenPageFaultManagerWhenEnqueueMemFillThenAllocIsDecommitted) {
    char pattern[256];
    auto mockMemoryManager = std::make_unique<MockMemoryManager>();
    mockMemoryManager->pageFaultManager.reset(new MockPageFaultManager());
    auto memoryManager = context->getMemoryManager();
    context->memoryManager = mockMemoryManager.get();
    mockMemoryManager->getPageFaultManager()->insertAllocation(ptrSVM, 256, context->getSVMAllocsManager(), context->getSpecialQueue(0u), {});
    EXPECT_EQ(static_cast<MockPageFaultManager *>(mockMemoryManager->getPageFaultManager())->transferToCpuCalled, 0);
    EXPECT_EQ(static_cast<MockPageFaultManager *>(mockMemoryManager->getPageFaultManager())->protectMemoryCalled, 0);

    pCmdQ->enqueueSVMMemFill(ptrSVM, &pattern, 256, 256, 0, nullptr, nullptr);

    EXPECT_EQ(static_cast<MockPageFaultManager *>(mockMemoryManager->getPageFaultManager())->allowMemoryAccessCalled, 0);
    EXPECT_EQ(static_cast<MockPageFaultManager *>(mockMemoryManager->getPageFaultManager())->protectMemoryCalled, 1);
    EXPECT_EQ(static_cast<MockPageFaultManager *>(mockMemoryManager->getPageFaultManager())->transferToCpuCalled, 0);
    EXPECT_EQ(static_cast<MockPageFaultManager *>(mockMemoryManager->getPageFaultManager())->transferToGpuCalled, 1);

    context->memoryManager = memoryManager;
}

HWTEST_F(EnqueueSvmTest, givenCopyFromMappedPtrToSvmAllocWhenCallingSvmMemcpyThenReuseMappedAllocations) {
    constexpr size_t size = 1u;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    {
        auto [buffer, mappedPtr] = createBufferAndMapItOnGpu();
        std::ignore = buffer;
        EXPECT_EQ(0u, csr.createAllocationForHostSurfaceCalled);
        retVal = this->pCmdQ->enqueueSVMMemcpy(
            false,     // cl_bool  blocking_copy
            ptrSVM,    // void *dst_ptr
            mappedPtr, // const void *src_ptr
            size,      // size_t size
            0,         // cl_uint num_events_in_wait_list
            nullptr,   // cl_evebt *event_wait_list
            nullptr,   // cL_event *event
            nullptr    // CommandStreamReceiver* csrParam
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(0u, csr.createAllocationForHostSurfaceCalled);
    }
    {
        auto notMappedPtr = std::make_unique<char[]>(size);
        EXPECT_EQ(0u, csr.createAllocationForHostSurfaceCalled);
        retVal = this->pCmdQ->enqueueSVMMemcpy(
            false,              // cl_bool  blocking_copy
            ptrSVM,             // void *dst_ptr
            notMappedPtr.get(), // const void *src_ptr
            size,               // size_t size
            0,                  // cl_uint num_events_in_wait_list
            nullptr,            // cl_evebt *event_wait_list
            nullptr,            // cL_event *event
            nullptr             // CommandStreamReceiver* csrParam
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(1u, csr.createAllocationForHostSurfaceCalled);
    }
}

HWTEST_F(EnqueueSvmTest, givenCopyFromSvmAllocToMappedPtrWhenCallingSvmMemcpyThenReuseMappedAllocations) {
    constexpr size_t size = 1u;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    {
        auto [buffer, mappedPtr] = createBufferAndMapItOnGpu();
        std::ignore = buffer;
        EXPECT_EQ(0u, csr.createAllocationForHostSurfaceCalled);
        retVal = this->pCmdQ->enqueueSVMMemcpy(
            false,     // cl_bool  blocking_copy
            mappedPtr, // void *dst_ptr
            ptrSVM,    // const void *src_ptr
            size,      // size_t size
            0,         // cl_uint num_events_in_wait_list
            nullptr,   // cl_evebt *event_wait_list
            nullptr,   // cL_event *event
            nullptr    // CommandStreamReceiver* csrParam
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(0u, csr.createAllocationForHostSurfaceCalled);
    }
    {
        auto notMappedPtr = std::make_unique<char[]>(size);
        EXPECT_EQ(0u, csr.createAllocationForHostSurfaceCalled);
        retVal = this->pCmdQ->enqueueSVMMemcpy(
            false,              // cl_bool  blocking_copy
            notMappedPtr.get(), // void *dst_ptr
            ptrSVM,             // const void *src_ptr
            size,               // size_t size
            0,                  // cl_uint num_events_in_wait_list
            nullptr,            // cl_evebt *event_wait_list
            nullptr,            // cL_event *event
            nullptr             // CommandStreamReceiver* csrParam
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(1u, csr.createAllocationForHostSurfaceCalled);
    }
}

HWTEST_F(EnqueueSvmTest, givenCopyFromMappedPtrToMappedPtrWhenCallingSvmMemcpyThenReuseMappedAllocations) {
    constexpr size_t size = 1u;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();

    {
        auto [buffer1, mappedPtr1] = createBufferAndMapItOnGpu();
        auto [buffer2, mappedPtr2] = createBufferAndMapItOnGpu();
        std::ignore = buffer1;
        std::ignore = buffer2;
        EXPECT_EQ(0u, csr.createAllocationForHostSurfaceCalled);
        retVal = this->pCmdQ->enqueueSVMMemcpy(
            false,      // cl_bool  blocking_copy
            mappedPtr2, // void *dst_ptr
            mappedPtr1, // const void *src_ptr
            size,       // size_t size
            0,          // cl_uint num_events_in_wait_list
            nullptr,    // cl_evebt *event_wait_list
            nullptr,    // cL_event *event
            nullptr     // CommandStreamReceiver* csrParam
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(0u, csr.createAllocationForHostSurfaceCalled);
    }
    {
        auto [buffer, mappedPtr] = createBufferAndMapItOnGpu();
        std::ignore = buffer;
        auto notMappedPtr = std::make_unique<char[]>(size);
        EXPECT_EQ(0u, csr.createAllocationForHostSurfaceCalled);
        retVal = this->pCmdQ->enqueueSVMMemcpy(
            false,              // cl_bool  blocking_copy
            mappedPtr,          // void *dst_ptr
            notMappedPtr.get(), // const void *src_ptr
            size,               // size_t size
            0,                  // cl_uint num_events_in_wait_list
            nullptr,            // cl_evebt *event_wait_list
            nullptr,            // cL_event *event
            nullptr             // CommandStreamReceiver* csrParam
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(1u, csr.createAllocationForHostSurfaceCalled);
    }
    {
        auto notMappedPtr = std::make_unique<char[]>(size);
        auto [buffer, mappedPtr] = createBufferAndMapItOnGpu();
        std::ignore = buffer;
        EXPECT_EQ(1u, csr.createAllocationForHostSurfaceCalled);
        retVal = this->pCmdQ->enqueueSVMMemcpy(
            false,              // cl_bool  blocking_copy
            notMappedPtr.get(), // void *dst_ptr
            mappedPtr,          // const void *src_ptr
            size,               // size_t size
            0,                  // cl_uint num_events_in_wait_list
            nullptr,            // cl_evebt *event_wait_list
            nullptr,            // cL_event *event
            nullptr             // CommandStreamReceiver* csrParam
        );
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_EQ(2u, csr.createAllocationForHostSurfaceCalled);
    }
}

struct StagingBufferTest : public EnqueueSvmTest {
    void SetUp() override {
        EnqueueSvmTest::SetUp();
        SVMAllocsManager::UnifiedMemoryProperties unifiedMemoryProperties(InternalMemoryType::deviceUnifiedMemory, 1, context->getRootDeviceIndices(), context->getDeviceBitfields());
        unifiedMemoryProperties.device = pDevice;
        svmManager = this->context->getSVMAllocsManager();

        dstPtr = svmManager->createUnifiedMemoryAllocation(copySize, unifiedMemoryProperties);
        srcPtr = new unsigned char[copySize];
    }

    void TearDown() override {
        svmManager = this->context->getSVMAllocsManager();
        svmManager->freeSVMAlloc(dstPtr);
        delete[] srcPtr;
        EnqueueSvmTest::TearDown();
    }

    static constexpr size_t stagingBufferSize = MemoryConstants::pageSize;
    static constexpr size_t copySize = stagingBufferSize * 4;
    static constexpr size_t expectedNumOfCopies = copySize / stagingBufferSize;

    SVMAllocsManager *svmManager;
    void *dstPtr;
    unsigned char *srcPtr;
};

HWTEST_F(StagingBufferTest, givenInOrderCmdQueueWhenEnqueueStagingBufferMemcpyNonBlockingThenCopySuccessfull) {
    constexpr cl_command_type expectedLastCmd = CL_COMMAND_SVM_MEMCPY;

    cl_event event;
    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    auto initialUsmAllocs = svmManager->getNumAllocs();
    retVal = myCmdQ.enqueueStagingBufferMemcpy(
        false,    // cl_bool blocking_copy
        dstPtr,   // void *dst_ptr
        srcPtr,   // const void *src_ptr
        copySize, // size_t size
        &event    // cl_event *event
    );
    auto pEvent = (Event *)event;
    auto numOfStagingBuffers = svmManager->getNumAllocs() - initialUsmAllocs;
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(myCmdQ.flushCalled);
    EXPECT_EQ(1u, numOfStagingBuffers);
    EXPECT_EQ(expectedNumOfCopies, myCmdQ.enqueueSVMMemcpyCalledCount);
    EXPECT_EQ(0u, myCmdQ.finishCalledCount);
    EXPECT_EQ(expectedLastCmd, myCmdQ.lastCommandType);
    EXPECT_EQ(expectedLastCmd, pEvent->getCommandType());

    clReleaseEvent(event);
}

HWTEST_F(StagingBufferTest, givenOutOfOrderCmdQueueWhenEnqueueStagingBufferMemcpyNonBlockingThenCopySuccessfull) {
    cl_event event;
    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    myCmdQ.setOoqEnabled();

    auto initialUsmAllocs = svmManager->getNumAllocs();
    retVal = myCmdQ.enqueueStagingBufferMemcpy(
        false,    // cl_bool blocking_copy
        dstPtr,   // void *dst_ptr
        srcPtr,   // const void *src_ptr
        copySize, // size_t size
        &event    // cl_event *event
    );

    auto pEvent = (Event *)event;
    auto numOfStagingBuffers = svmManager->getNumAllocs() - initialUsmAllocs;
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numOfStagingBuffers);
    EXPECT_EQ(expectedNumOfCopies, myCmdQ.enqueueSVMMemcpyCalledCount);
    EXPECT_EQ(0u, myCmdQ.finishCalledCount);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_BARRIER), myCmdQ.lastCommandType);
    EXPECT_EQ(static_cast<cl_command_type>(CL_COMMAND_SVM_MEMCPY), pEvent->getCommandType());

    clReleaseEvent(event);
}

HWTEST_F(StagingBufferTest, givenOutOfOrderCmdQueueWhenEnqueueStagingBufferMemcpyNonBlockingWithSingleTransferThenNoBarrierEnqueued) {
    constexpr cl_command_type expectedLastCmd = CL_COMMAND_SVM_MEMCPY;

    cl_event event;
    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    myCmdQ.setOoqEnabled();

    auto initialUsmAllocs = svmManager->getNumAllocs();
    retVal = myCmdQ.enqueueStagingBufferMemcpy(
        false,             // cl_bool blocking_copy
        dstPtr,            // void *dst_ptr
        srcPtr,            // const void *src_ptr
        stagingBufferSize, // size_t size
        &event             // cl_event *event
    );

    auto pEvent = (Event *)event;
    auto numOfStagingBuffers = svmManager->getNumAllocs() - initialUsmAllocs;
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numOfStagingBuffers);
    EXPECT_EQ(1u, myCmdQ.enqueueSVMMemcpyCalledCount);
    EXPECT_EQ(0u, myCmdQ.finishCalledCount);
    EXPECT_EQ(expectedLastCmd, myCmdQ.lastCommandType);
    EXPECT_EQ(expectedLastCmd, pEvent->getCommandType());

    clReleaseEvent(event);
}

HWTEST_F(StagingBufferTest, givenEnqueueStagingBufferMemcpyWhenTaskCountNotReadyThenCopySuccessfullAndBuffersNotReused) {
    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    auto initialUsmAllocs = svmManager->getNumAllocs();
    auto &csr = myCmdQ.getGpgpuCommandStreamReceiver();
    *csr.getTagAddress() = csr.peekTaskCount();
    retVal = myCmdQ.enqueueStagingBufferMemcpy(
        false,    // cl_bool blocking_copy
        dstPtr,   // void *dst_ptr
        srcPtr,   // const void *src_ptr
        copySize, // size_t size
        nullptr   // cl_event *event
    );
    auto numOfStagingBuffers = svmManager->getNumAllocs() - initialUsmAllocs;
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedNumOfCopies, numOfStagingBuffers);
    *csr.getTagAddress() = csr.peekTaskCount();
}

HWTEST_F(StagingBufferTest, givenCmdQueueWhenEnqueueStagingBufferMemcpyBlockingThenCopySuccessfullAndFinishCalled) {
    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    auto initialUsmAllocs = svmManager->getNumAllocs();
    retVal = myCmdQ.enqueueStagingBufferMemcpy(
        true,     // cl_bool blocking_copy
        dstPtr,   // void *dst_ptr
        srcPtr,   // const void *src_ptr
        copySize, // size_t size
        nullptr   // cl_event *event
    );
    auto numOfStagingBuffers = svmManager->getNumAllocs() - initialUsmAllocs;
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numOfStagingBuffers);
    EXPECT_EQ(1u, myCmdQ.finishCalledCount);

    retVal = myCmdQ.enqueueStagingBufferMemcpy(
        true,     // cl_bool blocking_copy
        dstPtr,   // void *dst_ptr
        srcPtr,   // const void *src_ptr
        copySize, // size_t size
        nullptr   // cl_event *event
    );
    numOfStagingBuffers = svmManager->getNumAllocs() - initialUsmAllocs;
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numOfStagingBuffers);
    EXPECT_EQ(2u, myCmdQ.finishCalledCount);
}

HWTEST_F(StagingBufferTest, givenCmdQueueWhenEnqueueStagingBufferWithInvalidBufferThenReturnFailure) {
    auto dstPtr = nullptr;
    auto srcPtr = new unsigned char[copySize];

    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    retVal = myCmdQ.enqueueStagingBufferMemcpy(
        false,    // cl_bool blocking_copy
        dstPtr,   // void *dst_ptr
        srcPtr,   // const void *src_ptr
        copySize, // size_t size
        nullptr   // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);

    delete[] srcPtr;
}

HWTEST_F(StagingBufferTest, givenCmdQueueWithProfilingWhenEnqueueStagingBufferMemcpyThenTimestampsSetCorrectly) {
    cl_event event;
    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    myCmdQ.setProfilingEnabled();
    retVal = myCmdQ.enqueueStagingBufferMemcpy(
        false,    // cl_bool blocking_copy
        dstPtr,   // void *dst_ptr
        srcPtr,   // const void *src_ptr
        copySize, // size_t size
        &event    // cl_event *event
    );
    auto pEvent = (Event *)event;

    // A small adjustment to give end timestamp a valid value instead of mocked value
    TimeStampData tsData{};
    pClDevice->getDevice().getOSTime()->getGpuCpuTime(&tsData);
    if (pEvent->getTimestampPacketNodes()) {
        auto node = pEvent->getTimestampPacketNodes()->peekNodes()[0];
        auto contextEnd = ptrOffset(node->getCpuBase(), node->getGlobalEndOffset());
        *reinterpret_cast<typename FamilyType::TimestampPacketType *>(contextEnd) = static_cast<typename FamilyType::TimestampPacketType>(tsData.gpuTimeStamp);
    } else {
        HwTimeStamps *timeStamps = static_cast<TagNode<HwTimeStamps> *>(pEvent->getHwTimeStampNode())->tagForCpuAccess;
        timeStamps->contextEndTS = tsData.gpuTimeStamp;
        timeStamps->globalEndTS = tsData.gpuTimeStamp;
    }

    EXPECT_FALSE(pEvent->isCPUProfilingPath());
    EXPECT_TRUE(pEvent->isProfilingEnabled());
    uint64_t queue, submit, start, end;
    pEvent->getEventProfilingInfo(CL_PROFILING_COMMAND_QUEUED, sizeof(uint64_t), &queue, 0);
    pEvent->getEventProfilingInfo(CL_PROFILING_COMMAND_SUBMIT, sizeof(uint64_t), &submit, 0);
    pEvent->getEventProfilingInfo(CL_PROFILING_COMMAND_START, sizeof(uint64_t), &start, 0);
    pEvent->getEventProfilingInfo(CL_PROFILING_COMMAND_END, sizeof(uint64_t), &end, 0);
    EXPECT_GE(queue, 0ull);
    EXPECT_GE(submit, queue);
    EXPECT_GE(start, submit);
    EXPECT_GE(end, start);
    clReleaseEvent(event);
}

HWTEST_F(StagingBufferTest, givenIsValidForStagingBufferCopyWhenSrcIsUnMappedThenReturnTrue) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableCopyWithStagingBuffers.set(1);
    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    EXPECT_TRUE(myCmdQ.isValidForStagingBufferCopy(pClDevice->getDevice(), dstPtr, srcPtr, stagingBufferSize, false));
}

HWTEST_F(StagingBufferTest, givenIsValidForStagingBufferCopyWhenSrcIsMappedThenReturnFalse) {
    DebugManagerStateRestore restore{};
    debugManager.flags.EnableCopyWithStagingBuffers.set(1);
    MockCommandQueueHw<FamilyType> myCmdQ(context, pClDevice, 0);
    auto [buffer, mappedPtr] = createBufferAndMapItOnGpu();
    EXPECT_FALSE(myCmdQ.isValidForStagingBufferCopy(pClDevice->getDevice(), dstPtr, mappedPtr, buffer->getSize(), false));
}
