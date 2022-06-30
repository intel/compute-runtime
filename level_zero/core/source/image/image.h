/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/ze_api.h>

struct _ze_image_handle_t {};

namespace NEO {
struct ImageInfo;
class GraphicsAllocation;
struct ImageDescriptor;
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

    static ze_result_t create(uint32_t productFamily, Device *device, const ze_image_desc_t *desc, Image **pImage);

    virtual ze_result_t createView(Device *device, const ze_image_desc_t *desc, ze_image_handle_t *pImage) = 0;

    virtual NEO::GraphicsAllocation *getAllocation() = 0;
    virtual void copySurfaceStateToSSH(void *surfaceStateHeap,
                                       const uint32_t surfaceStateOffset,
                                       bool isMediaBlockArg) = 0;
    virtual void copyRedescribedSurfaceStateToSSH(void *surfaceStateHeap, const uint32_t surfaceStateOffset) = 0;
    virtual NEO::ImageInfo getImageInfo() = 0;
    virtual ze_image_desc_t getImageDesc() = 0;
    virtual ze_result_t getMemoryProperties(ze_image_memory_properties_exp_t *pMemoryProperties) = 0;

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
