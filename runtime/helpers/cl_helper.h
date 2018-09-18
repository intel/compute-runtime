/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "CL/cl.h"
#include "CL/cl_gl_ext.h"

namespace OCLRT {
inline const std::string cmdTypetoString(cl_command_type cmd) {
    switch (cmd) {
    case CL_COMMAND_NDRANGE_KERNEL:
        return "CL_COMMAND_NDRANGE_KERNEL";
    case CL_COMMAND_TASK:
        return "CL_COMMAND_TASK";
    case CL_COMMAND_NATIVE_KERNEL:
        return "CL_COMMAND_NATIVE_KERNEL";
    case CL_COMMAND_READ_BUFFER:
        return "CL_COMMAND_READ_BUFFER";
    case CL_COMMAND_WRITE_BUFFER:
        return "CL_COMMAND_WRITE_BUFFER";
    case CL_COMMAND_COPY_BUFFER:
        return "CL_COMMAND_COPY_BUFFER";
    case CL_COMMAND_READ_IMAGE:
        return "CL_COMMAND_READ_IMAGE";
    case CL_COMMAND_WRITE_IMAGE:
        return "CL_COMMAND_WRITE_IMAGE";
    case CL_COMMAND_COPY_IMAGE:
        return "CL_COMMAND_COPY_IMAGE";
    case CL_COMMAND_COPY_IMAGE_TO_BUFFER:
        return "CL_COMMAND_COPY_IMAGE_TO_BUFFER";
    case CL_COMMAND_COPY_BUFFER_TO_IMAGE:
        return "CL_COMMAND_COPY_BUFFER_TO_IMAGE";
    case CL_COMMAND_MAP_BUFFER:
        return "CL_COMMAND_MAP_BUFFER";
    case CL_COMMAND_MAP_IMAGE:
        return "CL_COMMAND_MAP_IMAGE";
    case CL_COMMAND_UNMAP_MEM_OBJECT:
        return "CL_COMMAND_UNMAP_MEM_OBJECT";
    case CL_COMMAND_MARKER:
        return "CL_COMMAND_MARKER";
    case CL_COMMAND_ACQUIRE_GL_OBJECTS:
        return "CL_COMMAND_ACQUIRE_GL_OBJECTS";
    case CL_COMMAND_RELEASE_GL_OBJECTS:
        return "CL_COMMAND_RELEASE_GL_OBJECTS";
    case CL_COMMAND_READ_BUFFER_RECT:
        return "CL_COMMAND_READ_BUFFER_RECT";
    case CL_COMMAND_WRITE_BUFFER_RECT:
        return "CL_COMMAND_WRITE_BUFFER_RECT";
    case CL_COMMAND_COPY_BUFFER_RECT:
        return "CL_COMMAND_COPY_BUFFER_RECT";
    case CL_COMMAND_USER:
        return "CL_COMMAND_USER";
    case CL_COMMAND_BARRIER:
        return "CL_COMMAND_BARRIER";
    case CL_COMMAND_MIGRATE_MEM_OBJECTS:
        return "CL_COMMAND_MIGRATE_MEM_OBJECTS";
    case CL_COMMAND_FILL_BUFFER:
        return "CL_COMMAND_FILL_BUFFER";
    case CL_COMMAND_FILL_IMAGE:
        return "CL_COMMAND_FILL_IMAGE";
    case CL_COMMAND_SVM_FREE:
        return "CL_COMMAND_SVM_FREE";
    case CL_COMMAND_SVM_MEMCPY:
        return "CL_COMMAND_SVM_MEMCPY";
    case CL_COMMAND_SVM_MEMFILL:
        return "CL_COMMAND_SVM_MEMFILL";
    case CL_COMMAND_SVM_MAP:
        return "CL_COMMAND_SVM_MAP";
    case CL_COMMAND_SVM_UNMAP:
        return "CL_COMMAND_SVM_UNMAP";
    case CL_COMMAND_GL_FENCE_SYNC_OBJECT_KHR:
        return "CL_COMMAND_GL_FENCE_SYNC_OBJECT_KHR";
    default: {
        std::string returnString("CMD_UNKNOWN:" + std::to_string((cl_command_type)cmd));
        return returnString;
    }
    }
}
} // namespace OCLRT