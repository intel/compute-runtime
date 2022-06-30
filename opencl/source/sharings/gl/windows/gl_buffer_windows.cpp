/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/get_info.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/extensions/public/cl_gl_private_intel.h"
#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/sharings/gl/gl_buffer.h"
#include "opencl/source/sharings/gl/windows/gl_sharing_windows.h"

#include "config.h"

using namespace NEO;

Buffer *GlBuffer::createSharedGlBuffer(Context *context, cl_mem_flags flags, unsigned int bufferId, cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_SUCCESS);
    CL_GL_BUFFER_INFO bufferInfo = {0};
    bufferInfo.bufferName = bufferId;

    GLSharingFunctionsWindows *sharingFunctions = context->getSharing<GLSharingFunctionsWindows>();
    if (sharingFunctions->acquireSharedBufferINTEL(&bufferInfo) == GL_FALSE) {
        errorCode.set(CL_INVALID_GL_OBJECT);
        return nullptr;
    }

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
    auto sharingFunctions = static_cast<GLSharingFunctionsWindows *>(this->sharingFunctions);

    CL_GL_BUFFER_INFO bufferInfo = {};
    bufferInfo.bufferName = this->clGlObjectId;
    sharingFunctions->acquireSharedBufferINTEL(&bufferInfo);

    auto graphicsAllocation = updateData.memObject->getGraphicsAllocation(updateData.rootDeviceIndex);

    updateData.sharedHandle = bufferInfo.globalShareHandle;
    updateData.synchronizationStatus = SynchronizeStatus::ACQUIRE_SUCCESFUL;
    graphicsAllocation->setAllocationOffset(bufferInfo.bufferOffset);

    const auto currentSharedHandle = graphicsAllocation->peekSharedHandle();
    if (currentSharedHandle != updateData.sharedHandle) {
        updateData.updateData = new CL_GL_BUFFER_INFO(bufferInfo);
    }
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
    auto sharingFunctions = static_cast<GLSharingFunctionsWindows *>(this->sharingFunctions);

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
    auto sharingFunctions = static_cast<GLSharingFunctionsWindows *>(this->sharingFunctions);

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
    GLSharingFunctionsWindows *sharingFunctions = context->getSharing<GLSharingFunctionsWindows>();
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
                                           AllocationType::SHARED_BUFFER,
                                           false, // isMultiStorageAllocation
                                           context->getDeviceBitfieldForAllocation(context->getDevice(0)->getRootDeviceIndex())};
        // couldn't find allocation for reuse - create new
        graphicsAllocation =
            context->getMemoryManager()->createGraphicsAllocationFromSharedHandle(bufferInfo.globalShareHandle, properties, true, false);
    }

    if (!graphicsAllocation) {
        return nullptr;
    }
    graphicsAllocation->incReuseCount(); // decremented in releaseReusedGraphicsAllocation() called from MemObj destructor

    if (!reusedAllocation) {
        sharingFunctions->graphicsAllocationsForGlBufferReuse.push_back(std::make_pair(bufferId, graphicsAllocation));
        if (bufferInfo.pGmmResInfo) {
            DEBUG_BREAK_IF(graphicsAllocation->getDefaultGmm() != nullptr);
            auto gmmHelper = context->getDevice(0)->getRootDeviceEnvironment().getGmmHelper();
            graphicsAllocation->setDefaultGmm(new Gmm(gmmHelper, bufferInfo.pGmmResInfo));
        }
    }

    return graphicsAllocation;
}

void GlBuffer::releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) {
    auto sharingFunctions = static_cast<GLSharingFunctionsWindows *>(this->sharingFunctions);
    CL_GL_BUFFER_INFO bufferInfo = {};
    bufferInfo.bufferName = this->clGlObjectId;
    sharingFunctions->releaseSharedBufferINTEL(&bufferInfo);
}
