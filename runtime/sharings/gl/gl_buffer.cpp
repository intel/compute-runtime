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

#include "config.h"
#include "runtime/gmm_helper/gmm.h"
#include "gl_buffer.h"
#include "runtime/context/context.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/memory_manager/memory_manager.h"
#include "runtime/helpers/get_info.h"
#include "CLGLShr.h"

using namespace OCLRT;

Buffer *GlBuffer::createSharedGlBuffer(Context *context, cl_mem_flags flags, unsigned int bufferId, cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_SUCCESS);
    CL_GL_BUFFER_INFO bufferInfo = {0};
    bufferInfo.bufferName = bufferId;

    GLSharingFunctions *sharingFunctions = context->getSharing<GLSharingFunctions>();
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
    return Buffer::createSharedBuffer(context, flags, glHandler, graphicsAllocation);
}

void GlBuffer::synchronizeObject(UpdateData &updateData) {
    const auto currentSharedHandle = updateData.memObject->getGraphicsAllocation()->peekSharedHandle();
    CL_GL_BUFFER_INFO bufferInfo = {};
    bufferInfo.bufferName = this->clGlObjectId;
    sharingFunctions->acquireSharedBufferINTEL(&bufferInfo);

    updateData.sharedHandle = bufferInfo.globalShareHandle;
    updateData.synchronizationStatus = SynchronizeStatus::ACQUIRE_SUCCESFUL;
    updateData.memObject->getGraphicsAllocation()->allocationOffset = bufferInfo.bufferOffset;

    if (currentSharedHandle != updateData.sharedHandle) {
        updateData.updateData = new CL_GL_BUFFER_INFO(bufferInfo);
    }
}

void GlBuffer::resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData) {
    const auto memObject = updateData->memObject;
    if (currentSharedHandle != updateData->sharedHandle) {
        const auto bufferInfo = std::unique_ptr<CL_GL_BUFFER_INFO>(static_cast<CL_GL_BUFFER_INFO *>(updateData->updateData));

        auto oldGraphicsAllocation = memObject->getGraphicsAllocation();
        popGraphicsAllocationFromReuse(oldGraphicsAllocation);

        Context *context = memObject->getContext();
        auto newGraphicsAllocation = createGraphicsAllocation(context, clGlObjectId, *bufferInfo);
        if (newGraphicsAllocation == nullptr) {
            updateData->synchronizationStatus = SynchronizeStatus::SYNCHRONIZE_ERROR;
        } else {
            updateData->synchronizationStatus = SynchronizeStatus::ACQUIRE_SUCCESFUL;
        }
        memObject->resetGraphicsAllocation(newGraphicsAllocation);

        if (updateData->synchronizationStatus == SynchronizeStatus::ACQUIRE_SUCCESFUL) {
            memObject->getGraphicsAllocation()->allocationOffset = bufferInfo->bufferOffset;
        }
    }
}

void GlBuffer::popGraphicsAllocationFromReuse(GraphicsAllocation *graphicsAllocation) {
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
    GLSharingFunctions *sharingFunctions = context->getSharing<GLSharingFunctions>();
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
        // couldn't find allocation for reuse - create new
        graphicsAllocation =
            context->getMemoryManager()->createGraphicsAllocationFromSharedHandle(bufferInfo.globalShareHandle, true);
    }

    if (!graphicsAllocation) {
        return nullptr;
    }
    graphicsAllocation->incReuseCount(); // decremented in releaseReusedGraphicsAllocation() called from MemObj destructor

    if (!reusedAllocation) {
        sharingFunctions->graphicsAllocationsForGlBufferReuse.push_back(std::make_pair(bufferId, graphicsAllocation));
        if (bufferInfo.pGmmResInfo) {
            DEBUG_BREAK_IF(graphicsAllocation->gmm != nullptr);
            graphicsAllocation->gmm = new Gmm(bufferInfo.pGmmResInfo);
        }
    }

    return graphicsAllocation;
}

void GlBuffer::releaseResource(MemObj *memObject) {
    CL_GL_BUFFER_INFO bufferInfo = {};
    bufferInfo.bufferName = this->clGlObjectId;
    sharingFunctions->releaseSharedBufferINTEL(&bufferInfo);
}
