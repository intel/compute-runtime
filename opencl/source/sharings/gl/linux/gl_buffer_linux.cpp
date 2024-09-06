/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/extensions/public/cl_gl_private_intel.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/sharings/gl/gl_buffer.h"
#include "opencl/source/sharings/gl/linux/gl_sharing_linux.h"

#include "config.h"

using namespace NEO;

Buffer *GlBuffer::createSharedGlBuffer(Context *context, cl_mem_flags flags, unsigned int bufferId, cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_SUCCESS);

    /* Prepare flush & export request */
    struct mesa_glinterop_export_in objIn = {};
    struct mesa_glinterop_export_out objOut = {};
    struct mesa_glinterop_flush_out flushOut = {};
    int fenceFd = -1;

    objIn.version = 2;
    objIn.target = GL_ARRAY_BUFFER;
    objIn.obj = bufferId;

    switch (flags) {
    case CL_MEM_READ_ONLY:
        objIn.access = MESA_GLINTEROP_ACCESS_READ_ONLY;
        break;
    case CL_MEM_WRITE_ONLY:
        objIn.access = MESA_GLINTEROP_ACCESS_WRITE_ONLY;
        break;
    case CL_MEM_READ_WRITE:
        objIn.access = MESA_GLINTEROP_ACCESS_READ_WRITE;
        break;
    default:
        errorCode.set(CL_INVALID_VALUE);
        return nullptr;
    }

    flushOut.version = 1;
    flushOut.fence_fd = &fenceFd;

    objOut.version = 2;

    /* Call MESA interop */
    GLSharingFunctionsLinux *sharingFunctions = context->getSharing<GLSharingFunctionsLinux>();
    bool success;
    int retValue;

    success = sharingFunctions->flushObjectsAndWait(1, &objIn, &flushOut, &retValue);
    if (success) {
        retValue = sharingFunctions->exportObject(&objIn, &objOut);
    }

    if (!success || (retValue != MESA_GLINTEROP_SUCCESS) || (objOut.version != 2)) {
        switch (retValue) {
        case MESA_GLINTEROP_INVALID_DISPLAY:
        case MESA_GLINTEROP_INVALID_CONTEXT:
            errorCode.set(CL_INVALID_CONTEXT);
            break;
        case MESA_GLINTEROP_INVALID_OBJECT:
            errorCode.set(CL_INVALID_GL_OBJECT);
            break;
        case MESA_GLINTEROP_OUT_OF_HOST_MEMORY:
            errorCode.set(CL_OUT_OF_HOST_MEMORY);
            break;
        case MESA_GLINTEROP_OUT_OF_RESOURCES:
        default:
            errorCode.set(CL_OUT_OF_RESOURCES);
            break;
        }

        return nullptr;
    }

    /* Map result for rest of the function */
    CL_GL_BUFFER_INFO bufferInfo = {};
    bufferInfo.bufferName = bufferId;
    bufferInfo.globalShareHandle = static_cast<unsigned int>(objOut.dmabuf_fd);
    bufferInfo.bufferSize = static_cast<GLint>(objOut.buf_size);
    bufferInfo.bufferOffset = static_cast<GLint>(objOut.buf_offset);

    auto graphicsAllocation = GlBuffer::createGraphicsAllocation(context, bufferId, bufferInfo);
    if (!graphicsAllocation) {
        errorCode.set(CL_INVALID_GL_OBJECT);
        return nullptr;
    }

    auto glHandler = new GlBuffer(sharingFunctions, bufferId);
    auto rootDeviceIndex = graphicsAllocation->getRootDeviceIndex();
    auto multiGraphicsAllocation = MultiGraphicsAllocation(rootDeviceIndex);
    multiGraphicsAllocation.addAllocation(graphicsAllocation);

    return Buffer::createSharedBuffer(context, flags, glHandler, std::move(multiGraphicsAllocation));
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
    const auto memObject = updateData->memObject;
    if (currentSharedHandle != updateData->sharedHandle) {
        const auto bufferInfo = std::unique_ptr<CL_GL_BUFFER_INFO>(static_cast<CL_GL_BUFFER_INFO *>(updateData->updateData));

        auto oldGraphicsAllocation = memObject->getGraphicsAllocation(updateData->rootDeviceIndex);
        popGraphicsAllocationFromReuse(oldGraphicsAllocation);

        Context *context = memObject->getContext();
        auto newGraphicsAllocation = createGraphicsAllocation(context, clGlObjectId, *bufferInfo);
        if (newGraphicsAllocation == nullptr) {
            updateData->synchronizationStatus = SynchronizeStatus::SYNCHRONIZE_ERROR;
            memObject->removeGraphicsAllocation(updateData->rootDeviceIndex);
        } else {
            updateData->synchronizationStatus = SynchronizeStatus::ACQUIRE_SUCCESFUL;
            memObject->resetGraphicsAllocation(newGraphicsAllocation);
        }

        if (updateData->synchronizationStatus == SynchronizeStatus::ACQUIRE_SUCCESFUL) {
            memObject->getGraphicsAllocation(updateData->rootDeviceIndex)->setAllocationOffset(bufferInfo->bufferOffset);
        }
    }
}

void GlBuffer::popGraphicsAllocationFromReuse(GraphicsAllocation *graphicsAllocation) {
    auto sharingFunctions = static_cast<GLSharingFunctionsLinux *>(this->sharingFunctions);

    std::unique_lock<std::mutex> lock(sharingFunctions->mutex);

    auto &graphicsAllocations = sharingFunctions->graphicsAllocationsForGlBufferReuse;
    auto foundIter = std::find_if(graphicsAllocations.begin(), graphicsAllocations.end(),
                                  [&graphicsAllocation](const std::pair<unsigned int, GraphicsAllocation *> &entry) {
                                      return entry.second == graphicsAllocation;
                                  });
    if (foundIter != graphicsAllocations.end()) {
        std::iter_swap(foundIter, graphicsAllocations.end() - 1);
        graphicsAllocations.pop_back();
    }
    graphicsAllocation->decReuseCount();
}

void GlBuffer::releaseReusedGraphicsAllocation() {
    auto sharingFunctions = static_cast<GLSharingFunctionsLinux *>(this->sharingFunctions);

    std::unique_lock<std::mutex> lock(sharingFunctions->mutex);

    auto &allocationsVector = sharingFunctions->graphicsAllocationsForGlBufferReuse;
    auto itEnd = allocationsVector.end();
    for (auto it = allocationsVector.begin(); it != itEnd; it++) {
        if (it->first == clGlObjectId) {
            it->second->decReuseCount();
            if (it->second->peekReuseCount() == 0) {
                std::iter_swap(it, itEnd - 1);
                allocationsVector.pop_back();
            }
            break;
        }
    }
}

GraphicsAllocation *GlBuffer::createGraphicsAllocation(Context *context, unsigned int bufferId, _tagCLGLBufferInfo &bufferInfo) {
    GLSharingFunctionsLinux *sharingFunctions = context->getSharing<GLSharingFunctionsLinux>();
    auto &allocationsVector = sharingFunctions->graphicsAllocationsForGlBufferReuse;
    GraphicsAllocation *graphicsAllocation = nullptr;
    bool reusedAllocation = false;

    std::unique_lock<std::mutex> lock(sharingFunctions->mutex);
    auto endIter = allocationsVector.end();

    auto foundIter =
        std::find_if(allocationsVector.begin(), endIter,
                     [&bufferId](const std::pair<unsigned int, GraphicsAllocation *> &entry) { return entry.first == bufferId; });

    if (foundIter != endIter) {
        graphicsAllocation = foundIter->second;
        reusedAllocation = true;
    }

    if (!graphicsAllocation) {
        AllocationProperties properties = {context->getDevice(0)->getRootDeviceIndex(),
                                           false, // allocateMemory
                                           0u,    // size
                                           AllocationType::sharedBuffer,
                                           false, // isMultiStorageAllocation
                                           context->getDeviceBitfieldForAllocation(context->getDevice(0)->getRootDeviceIndex())};
        // couldn't find allocation for reuse - create new
        MemoryManager::OsHandleData osHandleData{bufferInfo.globalShareHandle};
        graphicsAllocation =
            context->getMemoryManager()->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, false, nullptr);
    }

    if (!graphicsAllocation) {
        return nullptr;
    }
    graphicsAllocation->incReuseCount(); // decremented in releaseReusedGraphicsAllocation() called from MemObj destructor

    if (!reusedAllocation) {
        sharingFunctions->graphicsAllocationsForGlBufferReuse.push_back(std::make_pair(bufferId, graphicsAllocation));
        if (bufferInfo.pGmmResInfo) {
            DEBUG_BREAK_IF(graphicsAllocation->getDefaultGmm() != nullptr);
            auto helper = context->getDevice(0)->getRootDeviceEnvironment().getGmmHelper();
            graphicsAllocation->setDefaultGmm(new Gmm(helper, bufferInfo.pGmmResInfo));
        } else {
            auto helper = context->getDevice(0)->getRootDeviceEnvironment().getGmmHelper();
            StorageInfo storageInfo = {};
            GmmRequirements gmmRequirements{};

            graphicsAllocation->setDefaultGmm(new Gmm(helper,
                                                      nullptr, bufferInfo.bufferSize, 1, GMM_RESOURCE_USAGE_UNKNOWN, storageInfo, gmmRequirements));
        }
    }

    return graphicsAllocation;
}

void GlBuffer::releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) {
    auto memoryManager = memObject->getMemoryManager();
    memoryManager->closeSharedHandle(memObject->getGraphicsAllocation(rootDeviceIndex));
}

void GlBuffer::callReleaseResource(bool createOrDestroy) {}
