/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/mem_obj/mem_obj_helper.h"
#include "gtest/gtest.h"

using namespace OCLRT;

TEST(MemObjHelper, givenValidMemFlagsForBufferWhenFlagsAreCheckedThenTrueIsReturned) {
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY |
                         CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR |
                         CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS;

    EXPECT_TRUE(MemObjHelper::checkMemFlagsForBuffer(flags));
}

TEST(MemObjHelper, givenInvalidMemFlagsForBufferWhenFlagsAreCheckedThenFalseIsReturned) {
    cl_mem_flags flags = (1 << 13) | (1 << 14) | (1 << 30) | (1 << 31);
    EXPECT_FALSE(MemObjHelper::checkExtraMemFlagsForBuffer(flags));
    EXPECT_FALSE(MemObjHelper::checkMemFlagsForBuffer(flags));
}

TEST(MemObjHelper, givenValidMemFlagsForSubBufferWhenFlagsAreCheckedThenTrueIsReturned) {
    cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY |
                         CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS;

    EXPECT_TRUE(MemObjHelper::checkMemFlagsForSubBuffer(flags));
}

TEST(MemObjHelper, givenInValidMemFlagsForSubBufferWhenFlagsAreCheckedThenTrueIsReturned) {
    cl_mem_flags flags = CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR | CL_MEM_USE_HOST_PTR;

    EXPECT_FALSE(MemObjHelper::checkMemFlagsForSubBuffer(flags));
}
