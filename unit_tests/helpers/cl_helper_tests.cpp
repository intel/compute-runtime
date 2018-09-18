/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gtest/gtest.h"
#include "runtime/helpers/cl_helper.h"

#include <array>

TEST(ClHelper, whenCallGetStringWithCmdTypeFunctionThenGetProperCmdTypeAsString) {
    std::array<std::string, 30> expected = {{"CL_COMMAND_NDRANGE_KERNEL",
                                             "CL_COMMAND_TASK",
                                             "CL_COMMAND_NATIVE_KERNEL",
                                             "CL_COMMAND_READ_BUFFER",
                                             "CL_COMMAND_WRITE_BUFFER",
                                             "CL_COMMAND_COPY_BUFFER",
                                             "CL_COMMAND_READ_IMAGE",
                                             "CL_COMMAND_WRITE_IMAGE",
                                             "CL_COMMAND_COPY_IMAGE",
                                             "CL_COMMAND_COPY_IMAGE_TO_BUFFER",
                                             "CL_COMMAND_COPY_BUFFER_TO_IMAGE",
                                             "CL_COMMAND_MAP_BUFFER",
                                             "CL_COMMAND_MAP_IMAGE",
                                             "CL_COMMAND_UNMAP_MEM_OBJECT",
                                             "CL_COMMAND_MARKER",
                                             "CL_COMMAND_ACQUIRE_GL_OBJECTS",
                                             "CL_COMMAND_RELEASE_GL_OBJECTS",
                                             "CL_COMMAND_READ_BUFFER_RECT",
                                             "CL_COMMAND_WRITE_BUFFER_RECT",
                                             "CL_COMMAND_COPY_BUFFER_RECT",
                                             "CL_COMMAND_USER",
                                             "CL_COMMAND_BARRIER",
                                             "CL_COMMAND_MIGRATE_MEM_OBJECTS",
                                             "CL_COMMAND_FILL_BUFFER",
                                             "CL_COMMAND_FILL_IMAGE",
                                             "CL_COMMAND_SVM_FREE",
                                             "CL_COMMAND_SVM_MEMCPY",
                                             "CL_COMMAND_SVM_MEMFILL",
                                             "CL_COMMAND_SVM_MAP",
                                             "CL_COMMAND_SVM_UNMAP"}};

    for (int i = CL_COMMAND_NDRANGE_KERNEL; i <= CL_COMMAND_SVM_UNMAP; i++) {
        EXPECT_STREQ(expected[i - CL_COMMAND_NDRANGE_KERNEL].c_str(), OCLRT::cmdTypetoString(i).c_str());
    }

    std::stringstream stream;
    stream << "CMD_UNKNOWN:" << (cl_command_type)-1;

    EXPECT_STREQ(stream.str().c_str(), OCLRT::cmdTypetoString(-1).c_str());

    EXPECT_STREQ("CL_COMMAND_GL_FENCE_SYNC_OBJECT_KHR", OCLRT::cmdTypetoString(CL_COMMAND_GL_FENCE_SYNC_OBJECT_KHR).c_str());
}