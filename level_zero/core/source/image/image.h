/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/cmdcontainer.h"

#include "level_zero/core/source/device/device.h"
#include <level_zero/ze_image.h>

struct _ze_image_handle_t {};

namespace NEO {
struct ImageInfo;
}

namespace L0 {

struct Image : _ze_image_handle_t {
    template <typename Type>
    struct Allocator {
        static Image *allocate() { return new Type(); }
    };

    virtual ~Image() = default;
    virtual ze_result_t destroy() = 0;

    static Image *create(uint32_t productFamily, Device *device, const ze_image_desc_t *desc);

    virtual NEO::GraphicsAllocation *getAllocation() = 0;
    virtual void decoupleAllocation(NEO::CommandContainer &commandContainer) = 0;
    virtual void copySurfaceStateToSSH(void *surfaceStateHeap,
                                       const uint32_t surfaceStateOffset) = 0;
    virtual void copyRedescribedSurfaceStateToSSH(void *surfaceStateHeap, const uint32_t surfaceStateOffset) = 0;
    virtual size_t getSizeInBytes() = 0;
    virtual NEO::ImageInfo getImageInfo() = 0;
    virtual ze_image_desc_t getImageDesc() = 0;

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
