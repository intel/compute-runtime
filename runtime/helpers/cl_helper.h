/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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