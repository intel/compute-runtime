/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"

#include "CL/cl.h"

#include <memory>

struct EnqueueTraits {
    static cl_uint numEventsInWaitList;
    static const cl_event *eventWaitList;
    static cl_event *event;
};

struct EnqueueCopyBufferTraits : public EnqueueTraits {
    static const size_t srcOffset;
    static const size_t dstOffset;
    static const size_t size;
    static cl_command_type cmdType;
};

template <typename T = EnqueueCopyBufferTraits>
struct EnqueueCopyBufferHelper {
    typedef T Traits;
    using Buffer = NEO::Buffer;
    using CommandQueue = NEO::CommandQueue;

    static cl_int enqueueCopyBuffer(CommandQueue *pCmdQ,
                                    Buffer *srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create()).get(),
                                    Buffer *dstBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create()).get(),
                                    size_t srcOffset = Traits::srcOffset,
                                    size_t dstOffset = Traits::dstOffset,
                                    size_t size = Traits::size,
                                    cl_uint numEventsInWaitList = Traits::numEventsInWaitList,
                                    const cl_event *eventWaitList = Traits::eventWaitList,
                                    cl_event *event = Traits::event) {

        cl_int retVal = pCmdQ->enqueueCopyBuffer(
            srcBuffer,
            dstBuffer,
            srcOffset,
            dstOffset,
            size,
            numEventsInWaitList,
            eventWaitList,
            event);
        return retVal;
    }

    static cl_int enqueue(CommandQueue *pCmdQ, void *placeholder = nullptr) {
        return enqueueCopyBuffer(pCmdQ);
    }
};

struct EnqueueCopyBufferToImageTraits : public EnqueueTraits {
    static const size_t srcOffset;
    static const size_t dstOrigin[3];
    static const size_t region[3];
    static cl_command_type cmdType;
};

template <typename T = EnqueueCopyBufferToImageTraits>
struct EnqueueCopyBufferToImageHelper {
    typedef T Traits;
    using Buffer = NEO::Buffer;
    using Image = NEO::Image;
    using CommandQueue = NEO::CommandQueue;

    static cl_int enqueueCopyBufferToImage(CommandQueue *pCmdQ,
                                           Buffer *srcBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create()).get(),
                                           Image *dstImage = nullptr,
                                           const size_t srcOffset = Traits::srcOffset,
                                           const size_t dstOrigin[3] = Traits::dstOrigin,
                                           const size_t region[3] = Traits::region,
                                           cl_uint numEventsInWaitList = Traits::numEventsInWaitList,
                                           const cl_event *eventWaitList = Traits::eventWaitList,
                                           cl_event *event = Traits::event) {
        auto &context = pCmdQ->getContext();
        std::unique_ptr<Image> dstImageDelete(dstImage ? nullptr : Image2dHelper<>::create(&context));
        dstImage = dstImage ? dstImage : dstImageDelete.get();

        size_t regionOut[3] = {
            region[0] == static_cast<size_t>(-1) ? dstImage->getImageDesc().image_width : region[0],
            region[1] == static_cast<size_t>(-1) ? dstImage->getImageDesc().image_height : region[1],
            region[2] == static_cast<size_t>(-1) ? (dstImage->getImageDesc().image_depth > 0 ? dstImage->getImageDesc().image_depth : 1) : region[2],
        };

        return pCmdQ->enqueueCopyBufferToImage(srcBuffer,
                                               dstImage,
                                               srcOffset,
                                               dstOrigin,
                                               regionOut,
                                               numEventsInWaitList,
                                               eventWaitList,
                                               event);
    }

    static cl_int enqueue(CommandQueue *pCmdQ, void *placeholder = nullptr) {
        return enqueueCopyBufferToImage(pCmdQ);
    }
};

struct EnqueueCopyImageToBufferTraits : public EnqueueTraits {
    static const size_t srcOrigin[3];
    static const size_t region[3];
    static const size_t dstOffset;
    static cl_command_type cmdType;
};

template <typename T = EnqueueCopyImageToBufferTraits>
struct EnqueueCopyImageToBufferHelper {
    typedef T Traits;
    using Buffer = NEO::Buffer;
    using Image = NEO::Image;
    using CommandQueue = NEO::CommandQueue;

    static cl_int enqueueCopyImageToBuffer(CommandQueue *pCmdQ,
                                           Image *srcImage = nullptr,
                                           Buffer *dstBuffer = std::unique_ptr<Buffer>(BufferHelper<>::create()).get(),
                                           const size_t srcOrigin[3] = Traits::srcOrigin,
                                           const size_t region[3] = Traits::region,
                                           const size_t dstOffset = Traits::dstOffset,
                                           cl_uint numEventsInWaitList = Traits::numEventsInWaitList,
                                           const cl_event *eventWaitList = Traits::eventWaitList,
                                           cl_event *event = Traits::event) {
        auto &context = pCmdQ->getContext();
        std::unique_ptr<Image> srcImageDelete(srcImage ? nullptr : Image2dHelper<>::create(&context));
        srcImage = srcImage ? srcImage : srcImageDelete.get();

        size_t regionIn[3] = {
            region[0] == static_cast<size_t>(-1) ? srcImage->getImageDesc().image_width : region[0],
            region[1] == static_cast<size_t>(-1) ? srcImage->getImageDesc().image_height : region[1],
            region[2] == static_cast<size_t>(-1) ? (srcImage->getImageDesc().image_depth > 0 ? srcImage->getImageDesc().image_depth : 1) : region[2],
        };

        return pCmdQ->enqueueCopyImageToBuffer(srcImage,
                                               dstBuffer,
                                               srcOrigin,
                                               regionIn,
                                               dstOffset,
                                               numEventsInWaitList,
                                               eventWaitList,
                                               event);
    }

    static cl_int enqueue(CommandQueue *pCmdQ, void *placeholder = nullptr) {
        return enqueueCopyImageToBuffer(pCmdQ);
    }
};

struct EnqueueCopyImageTraits : public EnqueueTraits {
    static const size_t srcOrigin[3];
    static const size_t dstOrigin[3];
    static const size_t region[3];
    static cl_command_type cmdType;
};

template <typename T = EnqueueCopyImageTraits>
struct EnqueueCopyImageHelper {
    typedef T Traits;
    using Image = NEO::Image;
    using CommandQueue = NEO::CommandQueue;

    static cl_int enqueueCopyImage(CommandQueue *pCmdQ,
                                   Image *srcImage = nullptr,
                                   Image *dstImage = nullptr,
                                   const size_t srcOrigin[3] = Traits::srcOrigin,
                                   const size_t dstOrigin[3] = Traits::dstOrigin,
                                   const size_t region[3] = Traits::region,
                                   cl_uint numEventsInWaitList = Traits::numEventsInWaitList,
                                   const cl_event *eventWaitList = Traits::eventWaitList,
                                   cl_event *event = Traits::event) {
        auto &context = pCmdQ->getContext();
        std::unique_ptr<Image> srcImageDelete(srcImage ? nullptr : Image2dHelper<>::create(&context));
        std::unique_ptr<Image> dstImageDelete(dstImage ? nullptr : Image2dHelper<>::create(&context));
        srcImage = srcImage ? srcImage : srcImageDelete.get();
        dstImage = dstImage ? dstImage : dstImageDelete.get();

        size_t regionOut[3] = {
            region[0] == static_cast<size_t>(-1) ? srcImage->getImageDesc().image_width : region[0],
            region[1] == static_cast<size_t>(-1) ? srcImage->getImageDesc().image_height : region[1],
            region[2] == static_cast<size_t>(-1) ? (srcImage->getImageDesc().image_depth > 0 ? srcImage->getImageDesc().image_depth : 1) : region[2],
        };

        return pCmdQ->enqueueCopyImage(srcImage,
                                       dstImage,
                                       srcOrigin,
                                       dstOrigin,
                                       regionOut,
                                       numEventsInWaitList,
                                       eventWaitList,
                                       event);
    }

    static cl_int enqueue(CommandQueue *pCmdQ, void *placeholder = nullptr) {
        return enqueueCopyImage(pCmdQ);
    }
};

struct EnqueueFillBufferTraits : public EnqueueTraits {
    static const float pattern[1];
    static const size_t patternSize;
    static const size_t offset;
    static const size_t size;
    static cl_command_type cmdType;
};

template <typename T = EnqueueFillBufferTraits>
struct EnqueueFillBufferHelper {
    typedef T Traits;
    using Buffer = NEO::Buffer;
    using CommandQueue = NEO::CommandQueue;

    static cl_int enqueueFillBuffer(CommandQueue *pCmdQ,
                                    Buffer *buffer = std::unique_ptr<Buffer>(BufferHelper<>::create()).get(),
                                    cl_uint numEventsInWaitList = Traits::numEventsInWaitList,
                                    const cl_event *eventWaitList = Traits::eventWaitList,
                                    cl_event *event = Traits::event) {

        return pCmdQ->enqueueFillBuffer(buffer,
                                        Traits::pattern,
                                        Traits::patternSize,
                                        Traits::offset,
                                        Traits::size,
                                        numEventsInWaitList,
                                        eventWaitList,
                                        event);
    }

    static cl_int enqueue(CommandQueue *pCmdQ, void *placeholder = nullptr) {
        return enqueueFillBuffer(pCmdQ);
    }
};

struct EnqueueFillImageTraits : public EnqueueTraits {
    static const float fillColor[4];
    static const size_t origin[3];
    static const size_t region[3];
    static cl_command_type cmdType;
};

template <typename T = EnqueueFillImageTraits>
struct EnqueueFillImageHelper {
    typedef T Traits;
    using Image = NEO::Image;
    using CommandQueue = NEO::CommandQueue;

    static cl_int enqueueFillImage(CommandQueue *pCmdQ,
                                   Image *image = nullptr,
                                   const void *fillColor = Traits::fillColor,
                                   const size_t *origin = Traits::origin,
                                   const size_t *region = Traits::region,
                                   cl_uint numEventsInWaitList = Traits::numEventsInWaitList,
                                   const cl_event *eventWaitList = Traits::eventWaitList,
                                   cl_event *event = Traits::event) {
        auto &context = pCmdQ->getContext();
        std::unique_ptr<Image> imageDelete(image ? nullptr : Image2dHelper<>::create(&context));
        image = image ? image : imageDelete.get();

        size_t regionOut[3] = {
            region[0] == static_cast<size_t>(-1) ? image->getImageDesc().image_width : region[0],
            region[1] == static_cast<size_t>(-1) ? image->getImageDesc().image_height : region[1],
            region[2] == static_cast<size_t>(-1) ? (image->getImageDesc().image_depth > 0 ? image->getImageDesc().image_depth : 1) : region[2],
        };

        cl_int retVal = pCmdQ->enqueueFillImage(image,
                                                fillColor,
                                                origin,
                                                regionOut,
                                                numEventsInWaitList,
                                                eventWaitList,
                                                event);
        return retVal;
    }
    static cl_int enqueue(CommandQueue *pCmdQ, void *placeholder = nullptr) {
        return enqueueFillImage(pCmdQ);
    }
};

struct EnqueueKernelTraits : public EnqueueTraits {
    static const cl_uint workDim;
    static const size_t globalWorkOffset[3];
    static const size_t globalWorkSize[3];
    static const size_t *localWorkSize;
    static cl_command_type cmdType;
};

template <typename T = EnqueueKernelTraits>
struct EnqueueKernelHelper {
    typedef T Traits;
    using CommandQueue = NEO::CommandQueue;
    using Kernel = NEO::Kernel;

    static cl_int enqueueKernel(CommandQueue *pCmdQ,
                                Kernel *kernel,
                                cl_uint workDim = Traits::workDim,
                                const size_t *globalWorkOffset = Traits::globalWorkOffset,
                                const size_t *globalWorkSize = Traits::globalWorkSize,
                                const size_t *localWorkSize = Traits::localWorkSize,
                                cl_uint numEventsInWaitList = Traits::numEventsInWaitList,
                                const cl_event *eventWaitList = Traits::eventWaitList,
                                cl_event *event = Traits::event) {

        return pCmdQ->enqueueKernel(kernel,
                                    workDim,
                                    globalWorkOffset,
                                    globalWorkSize,
                                    localWorkSize,
                                    numEventsInWaitList,
                                    eventWaitList,
                                    event);
    }
};

struct EnqueueMapBufferTraits : public EnqueueTraits {
    static const cl_bool blocking;
    static const cl_mem_flags flags;
    static const size_t offset;
    static const size_t sizeInBytes;
    static cl_int *errcodeRet;
    static cl_command_type cmdType;
};

template <typename T = EnqueueMapBufferTraits>
struct EnqueueMapBufferHelper {
    typedef T Traits;
    using Buffer = NEO::Buffer;
    using CommandQueue = NEO::CommandQueue;

    static void *enqueueMapBuffer(CommandQueue *pCmdQ,
                                  Buffer *buffer = std::unique_ptr<Buffer>(BufferHelper<>::create()).get(),
                                  cl_bool blockingMap = Traits::blocking,
                                  cl_mem_flags flags = Traits::flags,
                                  size_t offset = Traits::offset,
                                  size_t size = Traits::sizeInBytes,
                                  cl_uint numEventsInWaitList = Traits::numEventsInWaitList,
                                  const cl_event *eventWaitList = Traits::eventWaitList,
                                  cl_event *event = Traits::event,
                                  cl_int *errcodeRet = Traits::errcodeRet) {

        size = size == static_cast<size_t>(-1) ? buffer->getSize() : size;

        auto retCode = CL_SUCCESS;

        auto retPtr = pCmdQ->enqueueMapBuffer(buffer,
                                              blockingMap,
                                              flags,
                                              offset,
                                              size,
                                              numEventsInWaitList,
                                              eventWaitList,
                                              event,
                                              retCode);

        if (errcodeRet) {
            *errcodeRet = retCode;
        }
        return retPtr;
    }

    static cl_int enqueue(CommandQueue *pCmdQ, Buffer *buffer = nullptr) {
        auto retVal = CL_SUCCESS;
        enqueueMapBuffer(pCmdQ,
                         buffer ? buffer : std::unique_ptr<Buffer>(BufferHelper<>::create()).get(),
                         Traits::blocking,
                         Traits::flags,
                         Traits::offset,
                         Traits::sizeInBytes,
                         Traits::numEventsInWaitList,
                         Traits::eventWaitList,
                         Traits::event,
                         &retVal);
        return retVal;
    }
};

struct EnqueueReadBufferTraits : public EnqueueTraits {
    static const cl_bool blocking;
    static const size_t offset;
    static const size_t sizeInBytes;
    static void *hostPtr;
    static cl_command_type cmdType;
    static NEO::GraphicsAllocation *mapAllocation;
};

template <typename T = EnqueueReadBufferTraits>
struct EnqueueReadBufferHelper {
    typedef T Traits;
    using Buffer = NEO::Buffer;
    using CommandQueue = NEO::CommandQueue;

    static cl_int enqueueReadBuffer(CommandQueue *pCmdQ,
                                    Buffer *buffer = std::unique_ptr<Buffer>(BufferHelper<>::create()).get(),
                                    cl_bool blockingRead = Traits::blocking,
                                    size_t offset = Traits::offset,
                                    size_t size = Traits::sizeInBytes,
                                    void *ptr = Traits::hostPtr,
                                    NEO::GraphicsAllocation *mapAllocation = Traits::mapAllocation,
                                    cl_uint numEventsInWaitList = Traits::numEventsInWaitList,
                                    const cl_event *eventWaitList = Traits::eventWaitList,
                                    cl_event *event = Traits::event) {

        size = size == static_cast<size_t>(-1) ? buffer->getSize() : size;

        cl_int retVal = pCmdQ->enqueueReadBuffer(buffer,
                                                 blockingRead,
                                                 offset,
                                                 size,
                                                 ptr,
                                                 mapAllocation,
                                                 numEventsInWaitList,
                                                 eventWaitList,
                                                 event);
        return retVal;
    }

    static cl_int enqueue(CommandQueue *pCmdQ) {
        return enqueueReadBuffer(pCmdQ);
    }

    static cl_int enqueue(CommandQueue *pCmdQ, Buffer *buffer) {
        return enqueueReadBuffer(pCmdQ, buffer);
    }
};

struct EnqueueReadImageTraits : public EnqueueTraits {
    static const cl_bool blocking;
    static const size_t origin[3];
    static const size_t region[3];
    static const size_t rowPitch;
    static const size_t slicePitch;
    static void *hostPtr;
    static cl_command_type cmdType;
    static NEO::GraphicsAllocation *mapAllocation;
};

template <typename T = EnqueueReadImageTraits>
struct EnqueueReadImageHelper {
    typedef T Traits;
    using Image = NEO::Image;
    using CommandQueue = NEO::CommandQueue;

    static cl_int enqueueReadImage(CommandQueue *pCmdQ,
                                   Image *image = nullptr,
                                   cl_bool blockingRead = Traits::blocking,
                                   const size_t *origin = Traits::origin,
                                   const size_t *region = Traits::region,
                                   size_t rowPitch = Traits::rowPitch,
                                   size_t slicePitch = Traits::slicePitch,
                                   void *ptr = Traits::hostPtr,
                                   NEO::GraphicsAllocation *mapAllocation = Traits::mapAllocation,
                                   cl_uint numEventsInWaitList = Traits::numEventsInWaitList,
                                   const cl_event *eventWaitList = Traits::eventWaitList,
                                   cl_event *event = Traits::event) {
        auto &context = pCmdQ->getContext();
        std::unique_ptr<Image> imageDelete(image ? nullptr : Image2dHelper<>::create(&context));
        image = image ? image : imageDelete.get();

        size_t regionOut[3] = {
            region[0] == static_cast<size_t>(-1) ? image->getImageDesc().image_width : region[0],
            region[1] == static_cast<size_t>(-1) ? image->getImageDesc().image_height : region[1],
            region[2] == static_cast<size_t>(-1) ? (image->getImageDesc().image_depth > 0 ? image->getImageDesc().image_depth : 1) : region[2],
        };
        return pCmdQ->enqueueReadImage(image,
                                       blockingRead,
                                       origin,
                                       regionOut,
                                       rowPitch,
                                       slicePitch,
                                       ptr,
                                       mapAllocation,
                                       numEventsInWaitList,
                                       eventWaitList,
                                       event);
    }

    static cl_int enqueue(CommandQueue *pCmdQ, void *placeholder = nullptr) {
        return enqueueReadImage(pCmdQ);
    }
};

struct EnqueueWriteBufferTraits : public EnqueueTraits {
    static const bool zeroCopy;
    static const cl_bool blocking;
    static const size_t offset;
    static const size_t sizeInBytes;
    static void *hostPtr;
    static cl_command_type cmdType;
    static NEO::GraphicsAllocation *mapAllocation;
};

template <typename T = EnqueueWriteBufferTraits>
struct EnqueueWriteBufferHelper {
    typedef T Traits;
    using Buffer = NEO::Buffer;
    using CommandQueue = NEO::CommandQueue;

    static cl_int enqueueWriteBuffer(CommandQueue *pCmdQ,
                                     Buffer *buffer = std::unique_ptr<Buffer>(BufferHelper<>::create()).get(),
                                     cl_bool blockingWrite = Traits::blocking,
                                     size_t offset = Traits::offset,
                                     size_t size = Traits::sizeInBytes,
                                     void *ptr = Traits::hostPtr,
                                     NEO::GraphicsAllocation *mapAllocation = Traits::mapAllocation,
                                     cl_uint numEventsInWaitList = Traits::numEventsInWaitList,
                                     const cl_event *eventWaitList = Traits::eventWaitList,
                                     cl_event *event = Traits::event) {

        size = size == static_cast<size_t>(-1) ? buffer->getSize() : size;

        cl_int retVal = pCmdQ->enqueueWriteBuffer(buffer,
                                                  blockingWrite,
                                                  offset,
                                                  size,
                                                  ptr,
                                                  mapAllocation,
                                                  numEventsInWaitList,
                                                  eventWaitList,
                                                  event);
        return retVal;
    }

    static cl_int enqueue(CommandQueue *pCmdQ) {
        return enqueueWriteBuffer(pCmdQ);
    }

    static cl_int enqueue(CommandQueue *pCmdQ, Buffer *buffer) {
        return enqueueWriteBuffer(pCmdQ, buffer);
    }
};

struct EnqueueWriteBufferRectTraits : public EnqueueTraits {
    static const bool zeroCopy;
    static const cl_bool blocking;
    static const size_t bufferOrigin[3];
    static const size_t hostOrigin[3];
    static const size_t region[3];
    static size_t bufferRowPitch;
    static size_t bufferSlicePitch;
    static size_t hostRowPitch;
    static size_t hostSlicePitch;
    static void *hostPtr;
    static cl_command_type cmdType;
};

template <typename T = EnqueueWriteBufferRectTraits>
struct EnqueueWriteBufferRectHelper {
    typedef T Traits;
    using Buffer = NEO::Buffer;
    using CommandQueue = NEO::CommandQueue;

    static cl_int enqueueWriteBufferRect(CommandQueue *pCmdQ,
                                         Buffer *buffer = std::unique_ptr<Buffer>(BufferHelper<>::create()).get(),
                                         cl_bool blockingWrite = Traits::blocking,
                                         const size_t *bufferOrigin = Traits::bufferOrigin,
                                         const size_t *hostOrigin = Traits::hostOrigin,
                                         const size_t *region = Traits::region,
                                         size_t bufferRowPitch = Traits::bufferRowPitch,
                                         size_t bufferSlicePitch = Traits::bufferSlicePitch,
                                         size_t hostRowPitch = Traits::hostRowPitch,
                                         size_t hostSlicePitch = Traits::hostSlicePitch,
                                         void *hostPtr = Traits::hostPtr,
                                         cl_uint numEventsInWaitList = Traits::numEventsInWaitList,
                                         const cl_event *eventWaitList = Traits::eventWaitList,
                                         cl_event *event = Traits::event) {

        cl_int retVal = pCmdQ->enqueueWriteBufferRect(buffer,
                                                      blockingWrite,
                                                      bufferOrigin,
                                                      hostOrigin,
                                                      region,
                                                      bufferRowPitch,
                                                      bufferSlicePitch,
                                                      hostRowPitch,
                                                      hostSlicePitch,
                                                      hostPtr,
                                                      numEventsInWaitList,
                                                      eventWaitList,
                                                      event);
        return retVal;
    }

    static cl_int enqueue(CommandQueue *pCmdQ) {
        return enqueueWriteBufferRect(pCmdQ);
    }
};

struct EnqueueWriteImageTraits : public EnqueueTraits {
    static const cl_bool blocking;
    static const size_t origin[3];
    static const size_t region[3];
    static const size_t rowPitch;
    static const size_t slicePitch;
    static void *hostPtr;
    static cl_command_type cmdType;
    static NEO::GraphicsAllocation *mapAllocation;
};

template <typename T = EnqueueWriteImageTraits>
struct EnqueueWriteImageHelper {
    typedef T Traits;
    using Image = NEO::Image;
    using CommandQueue = NEO::CommandQueue;

    static cl_int enqueueWriteImage(CommandQueue *pCmdQ,
                                    Image *image = nullptr,
                                    cl_bool blockingRead = Traits::blocking,
                                    const size_t *origin = Traits::origin,
                                    const size_t *region = Traits::region,
                                    size_t rowPitch = Traits::rowPitch,
                                    size_t slicePitch = Traits::slicePitch,
                                    const void *ptr = Traits::hostPtr,
                                    NEO::GraphicsAllocation *mapAllocation = Traits::mapAllocation,
                                    cl_uint numEventsInWaitList = Traits::numEventsInWaitList,
                                    const cl_event *eventWaitList = Traits::eventWaitList,
                                    cl_event *event = Traits::event) {
        auto &context = pCmdQ->getContext();
        std::unique_ptr<Image> imageDelete(image ? nullptr : Image2dHelper<>::create(&context));
        image = image ? image : imageDelete.get();

        size_t regionOut[3] = {
            region[0] == static_cast<size_t>(-1) ? image->getImageDesc().image_width : region[0],
            region[1] == static_cast<size_t>(-1) ? image->getImageDesc().image_height : region[1],
            region[2] == static_cast<size_t>(-1) ? (image->getImageDesc().image_depth > 0 ? image->getImageDesc().image_depth : 1) : region[2],
        };
        return pCmdQ->enqueueWriteImage(image,
                                        blockingRead,
                                        origin,
                                        regionOut,
                                        rowPitch,
                                        slicePitch,
                                        ptr,
                                        mapAllocation,
                                        numEventsInWaitList,
                                        eventWaitList,
                                        event);
    }

    static cl_int enqueue(CommandQueue *pCmdQ, void *placeholder = nullptr) {
        return enqueueWriteImage(pCmdQ);
    }
};
