/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/api/opencl/source/cl_device/cl_device.h"
#include "level_zero/api/opencl/source/context/context.h"
#include "level_zero/api/opencl/source/mem_obj/buffer.h"
#include "level_zero/api/opencl/source/sharings/gl/gl_buffer.h"
#include "level_zero/api/opencl/source/sharings/gl/linux/gl_sharing_linux.h"

#include "config.h"

using namespace NEO;
using namespace NEO::LEO;

namespace NEO {
namespace LEO {

Buffer *GlBuffer::createSharedGlBuffer(Context *context, cl_mem_flags flags, unsigned int bufferId, cl_int *errcodeRet) {
    return nullptr;
}

void GlBuffer::synchronizeObject(UpdateData &updateData) {
    auto sharingFunctions = static_cast<GLSharingFunctionsLinux *>(this->sharingFunctions);

    /* Prepare flush request */
    struct mesa_glinterop_export_in objIn = {};
    struct mesa_glinterop_flush_out syncOut = {};
    int fenceFd = -1;

    objIn.version = 2;
    objIn.target = GL_ARRAY_BUFFER;
    objIn.obj = this->clGlObjectId;

    syncOut.version = 1;
    syncOut.fence_fd = &fenceFd;

    bool success = sharingFunctions->flushObjectsAndWait(1, &objIn, &syncOut);
    updateData.synchronizationStatus = success ? SynchronizeStatus::ACQUIRE_SUCCESFUL : SynchronizeStatus::SYNCHRONIZE_ERROR;
}

void GlBuffer::resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData) {
}

void GlBuffer::popGraphicsAllocationFromReuse(GraphicsAllocation *graphicsAllocation) {
}

void GlBuffer::releaseReusedGraphicsAllocation() {
}

GraphicsAllocation *GlBuffer::createGraphicsAllocation(Context *context, unsigned int bufferId, _tagCLGLBufferInfo &bufferInfo) {
    return nullptr;
}

void GlBuffer::releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) {
}

void GlBuffer::callReleaseResource(bool createOrDestroy) {}

} // namespace LEO
} // namespace NEO
