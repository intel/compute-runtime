/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.h"
#include "opencl/source/event/event.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clEnqueueMigrateMemObjectsTests;

TEST_F(clEnqueueMigrateMemObjectsTests, GivenNullCommandQueueWhenMigratingMemObjThenInvalidCommandQueueErrorIsReturned) {
    unsigned int bufferSize = 16;
    auto pHostMem = new unsigned char[bufferSize];
    memset(pHostMem, 0xaa, bufferSize);

    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto buffer = clCreateBuffer(
        pContext,
        flags,
        bufferSize,
        pHostMem,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    cl_event eventReturned = nullptr;
    auto result = clEnqueueMigrateMemObjects(
        nullptr,
        1,
        &buffer,
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        nullptr,
        &eventReturned);
    EXPECT_EQ(CL_INVALID_COMMAND_QUEUE, result);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] pHostMem;

    clReleaseEvent(eventReturned);
}

TEST_F(clEnqueueMigrateMemObjectsTests, GivenValidInputsWhenMigratingMemObjThenSuccessIsReturned) {
    unsigned int bufferSize = 16;
    auto pHostMem = new unsigned char[bufferSize];
    memset(pHostMem, 0xaa, bufferSize);

    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto buffer = clCreateBuffer(
        pContext,
        flags,
        bufferSize,
        pHostMem,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    cl_event eventReturned = nullptr;
    auto result = clEnqueueMigrateMemObjects(
        pCommandQueue,
        1,
        &buffer,
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        nullptr,
        &eventReturned);
    EXPECT_EQ(CL_SUCCESS, result);

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] pHostMem;

    clReleaseEvent(eventReturned);
}

TEST_F(clEnqueueMigrateMemObjectsTests, GivenNullMemObjsWhenMigratingMemObjThenInvalidValueErrorIsReturned) {

    cl_event eventReturned = nullptr;
    auto result = clEnqueueMigrateMemObjects(
        pCommandQueue,
        1,
        nullptr,
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        nullptr,
        &eventReturned);
    EXPECT_EQ(CL_INVALID_VALUE, result);
}

TEST_F(clEnqueueMigrateMemObjectsTests, GivenZeroMemObjectsWhenMigratingMemObjsThenInvalidValueErrorIsReturned) {

    cl_event eventReturned = nullptr;
    auto result = clEnqueueMigrateMemObjects(
        pCommandQueue,
        0,
        nullptr,
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        nullptr,
        &eventReturned);
    EXPECT_EQ(CL_INVALID_VALUE, result);
}

TEST_F(clEnqueueMigrateMemObjectsTests, GivenNonZeroEventsAndNullWaitlistWhenMigratingMemObjThenInvalidWaitListErrorIsReturned) {

    cl_event eventReturned = nullptr;
    auto result = clEnqueueMigrateMemObjects(
        pCommandQueue,
        0,
        nullptr,
        CL_MIGRATE_MEM_OBJECT_HOST,
        2,
        nullptr,
        &eventReturned);
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, result);
}

TEST_F(clEnqueueMigrateMemObjectsTests, GivenZeroEventsAndNonNullWaitlistWhenMigratingMemObjsThenInvalidWaitListErrorIsReturned) {

    cl_event eventReturned = nullptr;
    Event event(pCommandQueue, CL_COMMAND_MIGRATE_MEM_OBJECTS, 0, 0);

    auto result = clEnqueueMigrateMemObjects(
        pCommandQueue,
        0,
        nullptr,
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        (cl_event *)&event,
        &eventReturned);
    EXPECT_EQ(CL_INVALID_EVENT_WAIT_LIST, result);
}

TEST_F(clEnqueueMigrateMemObjectsTests, GivenValidFlagsWhenMigratingMemObjsThenSuccessIsReturned) {
    unsigned int bufferSize = 16;
    auto pHostMem = new unsigned char[bufferSize];
    memset(pHostMem, 0xaa, bufferSize);

    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto buffer = clCreateBuffer(
        pContext,
        flags,
        bufferSize,
        pHostMem,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    cl_mem_migration_flags validFlags[] = {0, CL_MIGRATE_MEM_OBJECT_HOST, CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED, CL_MIGRATE_MEM_OBJECT_HOST | CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED};

    for (auto validFlag : validFlags) {
        cl_event eventReturned = nullptr;
        auto result = clEnqueueMigrateMemObjects(
            pCommandQueue,
            1,
            &buffer,
            validFlag,
            0,
            nullptr,
            &eventReturned);
        EXPECT_EQ(CL_SUCCESS, result);
        clReleaseEvent(eventReturned);
    }

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] pHostMem;
}

TEST_F(clEnqueueMigrateMemObjectsTests, GivenInvalidFlagsWhenMigratingMemObjsThenInvalidValueErrorIsReturned) {
    unsigned int bufferSize = 16;
    auto pHostMem = new unsigned char[bufferSize];
    memset(pHostMem, 0xaa, bufferSize);

    cl_mem_flags flags = CL_MEM_USE_HOST_PTR;
    auto buffer = clCreateBuffer(
        pContext,
        flags,
        bufferSize,
        pHostMem,
        &retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, buffer);

    cl_mem_migration_flags invalidFlags[] = {(cl_mem_migration_flags)0xffffffff, CL_MIGRATE_MEM_OBJECT_HOST | (1 << 10), CL_MIGRATE_MEM_OBJECT_CONTENT_UNDEFINED | (1 << 10), (cl_mem_migration_flags)12345};

    for (auto invalidFlag : invalidFlags) {
        cl_event eventReturned = nullptr;
        auto result = clEnqueueMigrateMemObjects(
            pCommandQueue,
            1,
            &buffer,
            invalidFlag,
            0,
            nullptr,
            &eventReturned);
        EXPECT_EQ(CL_INVALID_VALUE, result);
        clReleaseEvent(eventReturned);
    }

    retVal = clReleaseMemObject(buffer);
    EXPECT_EQ(CL_SUCCESS, retVal);

    delete[] pHostMem;
}

TEST_F(clEnqueueMigrateMemObjectsTests, GivenInvalidMemObjectWhenMigratingMemObjsThenInvalidMemObjectErrorIsReturned) {
    cl_event eventReturned = nullptr;

    auto result = clEnqueueMigrateMemObjects(
        pCommandQueue,
        1,
        reinterpret_cast<cl_mem *>(pCommandQueue),
        CL_MIGRATE_MEM_OBJECT_HOST,
        0,
        nullptr,
        &eventReturned);

    EXPECT_EQ(CL_INVALID_MEM_OBJECT, result);
}
