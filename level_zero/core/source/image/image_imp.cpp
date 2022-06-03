/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/image/image_imp.h"

#include "shared/source/memory_manager/memory_manager.h"

#include "level_zero/core/source/device/device.h"

#include "igfxfmid.h"

namespace L0 {

ImageAllocatorFn imageFactory[IGFX_MAX_PRODUCT] = {};

ImageImp::~ImageImp() {
    if (!isImageView && this->device != nullptr) {
        this->device->getNEODevice()->getMemoryManager()->freeGraphicsMemory(this->allocation);
    }
}

ze_result_t ImageImp::destroy() {
    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t ImageImp::createView(Device *device, const ze_image_desc_t *desc, ze_image_handle_t *pImage) {
    auto productFamily = device->getNEODevice()->getHardwareInfo().platform.eProductFamily;

    ImageAllocatorFn allocator = nullptr;
    allocator = imageFactory[productFamily];

    ImageImp *image = nullptr;

    image = static_cast<ImageImp *>((*allocator)());
    image->isImageView = true;
    image->allocation = allocation;
    auto result = image->initialize(device, desc);

    if (result != ZE_RESULT_SUCCESS) {
        image->destroy();
        image = nullptr;
    }

    *pImage = image;

    return result;
}

ze_result_t Image::create(uint32_t productFamily, Device *device, const ze_image_desc_t *desc, Image **pImage) {
    ze_result_t result = ZE_RESULT_SUCCESS;
    ImageAllocatorFn allocator = nullptr;
    if (productFamily < IGFX_MAX_PRODUCT) {
        allocator = imageFactory[productFamily];
    }

    ImageImp *image = nullptr;
    if (allocator) {
        image = static_cast<ImageImp *>((*allocator)());
        result = image->initialize(device, desc);
        if (result != ZE_RESULT_SUCCESS) {
            image->destroy();
            image = nullptr;
        }
    } else {
        result = ZE_RESULT_ERROR_UNKNOWN;
    }
    *pImage = image;

    return result;
}
} // namespace L0
