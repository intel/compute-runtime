/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/core/source/helpers/api_handle_helper.h"

struct _ze_image_handle_t : BaseHandleWithLoaderTranslation<ZEL_HANDLE_IMAGE> {};
static_assert(IsCompliantWithDdiHandlesExt<_ze_image_handle_t>);

namespace NEO {
struct ImageInfo;
class GraphicsAllocation;
struct ImageDescriptor;
struct SurfaceStateInHeapInfo;
} // namespace NEO

namespace L0 {
struct Device;

struct Image : _ze_image_handle_t {
    template <typename Type>
    struct Allocator {
        static Image *allocate() { return new Type(); }
    };

    virtual ~Image() = default;
    virtual ze_result_t destroy() = 0;
    virtual ze_result_t destroyPeerImages(const void *ptr, Device *device) = 0;

    static ze_result_t create(uint32_t productFamily, Device *device, const ze_image_desc_t *desc, Image **pImage);

    virtual ze_result_t createView(Device *device, const ze_image_desc_t *desc, ze_image_handle_t *pImage) = 0;

    virtual NEO::GraphicsAllocation *getAllocation() = 0;
    virtual NEO::GraphicsAllocation *getImplicitArgsAllocation() = 0;
    virtual void copySurfaceStateToSSH(void *surfaceStateHeap,
                                       uint32_t surfaceStateOffset,
                                       uint32_t bindlessSlot,
                                       bool isMediaBlockArg) = 0;
    virtual NEO::ImageInfo getImageInfo() = 0;
    virtual ze_image_desc_t getImageDesc() = 0;
    virtual ze_result_t getMemoryProperties(ze_image_memory_properties_exp_t *pMemoryProperties) = 0;
    virtual ze_result_t allocateBindlessSlot() = 0;
    virtual NEO::SurfaceStateInHeapInfo *getBindlessSlot() = 0;
    virtual ze_result_t getDeviceOffset(uint64_t *deviceOffset) = 0;
    virtual bool isMimickedImage() = 0;

    static ze_result_t getPitchFor2dImage(
        ze_device_handle_t hDevice,
        size_t imageWidth,
        size_t imageHeight,
        unsigned int elementSizeInByte,
        size_t *rowPitch);
    static Image *fromHandle(ze_image_handle_t handle) { return static_cast<Image *>(handle); }

    inline ze_image_handle_t toHandle() { return this; }
};

using ImageAllocatorFn = Image *(*)();
extern ImageAllocatorFn imageFactory[];

template <uint32_t productFamily, typename ImageType>
struct ImagePopulateFactory {
    ImagePopulateFactory() { imageFactory[productFamily] = Image::Allocator<ImageType>::allocate; }
};

} // namespace L0
