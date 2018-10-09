/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/command_queue/command_queue_fixture.h"
#include "unit_tests/command_queue/enqueue_map_buffer_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/buffer_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/memory_manager/svm_memory_manager.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "runtime/memory_manager/surface.h"
#include "gtest/gtest.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "test.h"

using namespace OCLRT;

struct EnqueueSvmTest : public DeviceFixture,
                        public CommandQueueHwFixture,
                        public ::testing::Test {
    typedef CommandQueueHwFixture CommandQueueFixture;

    EnqueueSvmTest() {
    }

    void SetUp() override {
        DeviceFixture::SetUp();
        CommandQueueFixture::SetUp(pDevice, 0);
        ptrSVM = context->getSVMAllocsManager()->createSVMAlloc(256);
    }

    void TearDown() override {
        context->getSVMAllocsManager()->freeSVMAlloc(ptrSVM);
        CommandQueueFixture::TearDown();
        DeviceFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    void *ptrSVM = nullptr;
};

TEST_F(EnqueueSvmTest, enqueueSVMMap_InvalidValue) {
    void *svmPtr = nullptr;
    retVal = this->pCmdQ->enqueueSVMMap(
        CL_FALSE,    // cl_bool blocking_map
        CL_MAP_READ, // cl_map_flags map_flags
        svmPtr,      // void *svm_ptr
        0,           // size_t size
        0,           // cl_uint num_events_in_wait_list
        nullptr,     // const cL_event *event_wait_list
        nullptr      // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(EnqueueSvmTest, enqueueSVMMap_Success) {
    retVal = this->pCmdQ->enqueueSVMMap(
        CL_FALSE,    // cl_bool blocking_map
        CL_MAP_READ, // cl_map_flags map_flags
        ptrSVM,      // void *svm_ptr
        256,         // size_t size
        0,           // cl_uint num_events_in_wait_list
        nullptr,     // const cL_event *event_wait_list
        nullptr      // cl_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueSvmTest, enqueueSVMMapBlocking_Success) {
    retVal = this->pCmdQ->enqueueSVMMap(
        CL_TRUE,     // cl_bool blocking_map
        CL_MAP_READ, // cl_map_flags map_flags
        ptrSVM,      // void *svm_ptr
        256,         // size_t size
        0,           // cl_uint num_events_in_wait_list
        nullptr,     // const cL_event *event_wait_list
        nullptr      // cl_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueSvmTest, enqueueSVMMapBlockedOnEvent_Success) {
    UserEvent uEvent;
    cl_event eventWaitList[] = {&uEvent};
    retVal = this->pCmdQ->enqueueSVMMap(
        CL_FALSE,      // cl_bool blocking_map
        CL_MAP_READ,   // cl_map_flags map_flags
        ptrSVM,        // void *svm_ptr
        256,           // size_t size
        1,             // cl_uint num_events_in_wait_list
        eventWaitList, // const cL_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueSvmTest, enqueueSVMUnmap_InvalidValue) {
    void *svmPtr = nullptr;
    retVal = this->pCmdQ->enqueueSVMUnmap(
        svmPtr,  // void *svm_ptr
        0,       // cl_uint num_events_in_wait_list
        nullptr, // const cL_event *event_wait_list
        nullptr  // cl_event *event
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(EnqueueSvmTest, enqueueSVMUnmap_Success) {
    retVal = this->pCmdQ->enqueueSVMUnmap(
        ptrSVM,  // void *svm_ptr
        0,       // cl_uint num_events_in_wait_list
        nullptr, // const cL_event *event_wait_list
        nullptr  // cl_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueSvmTest, enqueueSVMUnmapBlockedOnEvent_Success) {
    UserEvent uEvent;
    cl_event eventWaitList[] = {&uEvent};
    retVal = this->pCmdQ->enqueueSVMUnmap(
        ptrSVM,        // void *svm_ptr
        1,             // cl_uint num_events_in_wait_list
        eventWaitList, // const cL_event *event_wait_list
        nullptr        // cl_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(EnqueueSvmTest, enqueueSVMFreeWithoutCallback_Success) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
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

TEST_F(EnqueueSvmTest, enqueueSVMFreeWithCallback_Success) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    void *svmPtrs[] = {ptrSVM};
    bool callbackWasCalled = false;
    struct ClbHelper {
        ClbHelper(bool &callbackWasCalled)
            : callbackWasCalled(callbackWasCalled) {}

        static void CL_CALLBACK Clb(cl_command_queue queue, cl_uint numSvmPointers, void *svmPointers[], void *usrData) {
            ClbHelper *data = (ClbHelper *)usrData;
            data->callbackWasCalled = true;
        }

        bool &callbackWasCalled;
    } userData(callbackWasCalled);

    retVal = this->pCmdQ->enqueueSVMFree(
        1,              // cl_uint num_svm_pointers
        svmPtrs,        // void *svm_pointers[]
        ClbHelper::Clb, // (CL_CALLBACK  *pfn_free_func) (cl_command_queue queue, cl_uint num_svm_pointers, void *svm_pointers[])
        &userData,      // void *user_data
        0,              // cl_uint num_events_in_wait_list
        nullptr,        // const cl_event *event_wait_list
        nullptr         // cl_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(callbackWasCalled);
}

TEST_F(EnqueueSvmTest, enqueueSVMFreeWithCallbackAndEvent_Success) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    void *svmPtrs[] = {ptrSVM};
    bool callbackWasCalled = false;
    struct ClbHelper {
        ClbHelper(bool &callbackWasCalled)
            : callbackWasCalled(callbackWasCalled) {}

        static void CL_CALLBACK Clb(cl_command_queue queue, cl_uint numSvmPointers, void *svmPointers[], void *usrData) {
            ClbHelper *data = (ClbHelper *)usrData;
            data->callbackWasCalled = true;
        }

        bool &callbackWasCalled;
    } userData(callbackWasCalled);
    cl_event event = nullptr;

    retVal = this->pCmdQ->enqueueSVMFree(
        1,              // cl_uint num_svm_pointers
        svmPtrs,        // void *svm_pointers[]
        ClbHelper::Clb, // (CL_CALLBACK  *pfn_free_func) (cl_command_queue queue, cl_uint num_svm_pointers, void *svm_pointers[])
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

TEST_F(EnqueueSvmTest, enqueueSVMFreeBlockedOnEvent_Success) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
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

TEST_F(EnqueueSvmTest, enqueueSVMMemcpy_InvalidValueDstPtrIsNull) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableAsyncEventsHandler.set(false);
    void *pDstSVM = nullptr;
    void *pSrcSVM = context->getSVMAllocsManager()->createSVMAlloc(256);
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr  // cL_event *event
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    context->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
}

TEST_F(EnqueueSvmTest, enqueueSVMMemcpy_InvalidValueSrcPtrIsNull) {
    void *pDstSVM = ptrSVM;
    void *pSrcSVM = nullptr;
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr  // cL_event *event
    );
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(EnqueueSvmTest, enqueueSVMMemcpy_Success) {
    void *pDstSVM = ptrSVM;
    void *pSrcSVM = context->getSVMAllocsManager()->createSVMAlloc(256);
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr  // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    context->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
}

TEST_F(EnqueueSvmTest, enqueueSVMMemcpyBlocking_Success) {
    void *pDstSVM = ptrSVM;
    void *pSrcSVM = context->getSVMAllocsManager()->createSVMAlloc(256);
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        true,    // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr  // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    context->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
}

TEST_F(EnqueueSvmTest, enqueueSVMMemcpyBlockedOnEvent_Success) {
    void *pDstSVM = ptrSVM;
    void *pSrcSVM = context->getSVMAllocsManager()->createSVMAlloc(256);
    UserEvent uEvent;
    cl_event eventWaitList[] = {&uEvent};
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,         // cl_bool  blocking_copy
        pDstSVM,       // void *dst_ptr
        pSrcSVM,       // const void *src_ptr
        256,           // size_t size
        1,             // cl_uint num_events_in_wait_list
        eventWaitList, // cl_evebt *event_wait_list
        nullptr        // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    context->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
}

TEST_F(EnqueueSvmTest, enqueueSVMMemcpyCoherent_Success) {
    void *pDstSVM = ptrSVM;
    void *pSrcSVM = context->getSVMAllocsManager()->createSVMAlloc(256, true);
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,   // cl_bool  blocking_copy
        pDstSVM, // void *dst_ptr
        pSrcSVM, // const void *src_ptr
        256,     // size_t size
        0,       // cl_uint num_events_in_wait_list
        nullptr, // cl_evebt *event_wait_list
        nullptr  // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    context->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
}

TEST_F(EnqueueSvmTest, enqueueSVMMemcpyCoherentBlockedOnEvent_Success) {
    void *pDstSVM = ptrSVM;
    void *pSrcSVM = context->getSVMAllocsManager()->createSVMAlloc(256, true);
    UserEvent uEvent;
    cl_event eventWaitList[] = {&uEvent};
    retVal = this->pCmdQ->enqueueSVMMemcpy(
        false,         // cl_bool  blocking_copy
        pDstSVM,       // void *dst_ptr
        pSrcSVM,       // const void *src_ptr
        256,           // size_t size
        1,             // cl_uint num_events_in_wait_list
        eventWaitList, // cl_evebt *event_wait_list
        nullptr        // cL_event *event
    );
    EXPECT_EQ(CL_SUCCESS, retVal);
    context->getSVMAllocsManager()->freeSVMAlloc(pSrcSVM);
}

TEST_F(EnqueueSvmTest, enqueueSVMMemFill_InvalidValue) {
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

TEST_F(EnqueueSvmTest, enqueueSVMMemFill_Success) {
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
}

TEST_F(EnqueueSvmTest, enqueueSVMMemFillBlockedOnEvent_Success) {
    const float pattern[1] = {1.2345f};
    const size_t patternSize = sizeof(pattern);
    UserEvent uEvent;
    cl_event eventWaitList[] = {&uEvent};
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
}

TEST_F(EnqueueSvmTest, enqueueSVMMemFillDoubleToReuseAllocation_Success) {
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
    auto &csr = pCmdQ->getDevice().getCommandStreamReceiver();
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

    EXPECT_EQ(GraphicsAllocation::AllocationType::FILL_PATTERN, patternAllocation->getAllocationType());
}

TEST_F(EnqueueSvmTest, enqueueTaskWithKernelExecInfo_success) {
    GraphicsAllocation *pSvmAlloc = context->getSVMAllocsManager()->getSVMAlloc(ptrSVM);
    EXPECT_NE(nullptr, ptrSVM);

    std::unique_ptr<Program> program(Program::create("FillBufferBytes", context, *pDevice, true, &retVal));
    cl_device_id device = pDevice;
    program->build(1, &device, nullptr, nullptr, nullptr, false);
    std::unique_ptr<Kernel> kernel(Kernel::create<MockKernel>(program.get(), *program->getKernelInfo("FillBufferBytes"), &retVal));

    kernel->setKernelExecInfo(pSvmAlloc);

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

    EXPECT_EQ(1u, kernel->getKernelSvmGfxAllocations().size());
}

TEST_F(EnqueueSvmTest, givenEnqueueTaskBlockedOnUserEventWhenItIsEnqueuedThenSurfacesAreMadeResident) {
    GraphicsAllocation *pSvmAlloc = context->getSVMAllocsManager()->getSVMAlloc(ptrSVM);
    EXPECT_NE(nullptr, ptrSVM);

    std::unique_ptr<Program> program(Program::create("FillBufferBytes", context, *pDevice, true, &retVal));
    cl_device_id device = pDevice;
    program->build(1, &device, nullptr, nullptr, nullptr, false);
    std::unique_ptr<Kernel> kernel(Kernel::create<MockKernel>(program.get(), *program->getKernelInfo("FillBufferBytes"), &retVal));

    std::vector<Surface *> allSurfaces;
    kernel->getResidency(allSurfaces);
    EXPECT_EQ(1u, allSurfaces.size());

    kernel->setKernelExecInfo(pSvmAlloc);

    UserEvent uEvent;
    cl_event eventWaitList[] = {&uEvent};
    size_t offset = 0;
    size_t size = 1;
    retVal = this->pCmdQ->enqueueKernel(
        kernel.get(),
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

    EXPECT_EQ(1u, kernel->getKernelSvmGfxAllocations().size());
}

TEST_F(EnqueueSvmTest, concurentMapAccess) {
    std::atomic<int> flag(0);
    std::atomic<int> ready(0);
    void *svmPtrs[15] = {};

    auto allocSvm = [&](uint32_t from, uint32_t to) {
        for (uint32_t i = from; i <= to; i++) {
            svmPtrs[i] = context->getSVMAllocsManager()->createSVMAlloc(1);
            auto ga = context->getSVMAllocsManager()->getSVMAlloc(svmPtrs[i]);
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

TEST_F(EnqueueSvmTest, enqueueSVMMigrateMem_Success) {
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
