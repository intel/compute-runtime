/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/image/image_imp.h"

#include "shared/source/memory_manager/memory_manager.h"

#include "igfxfmid.h"

namespace L0 {

ImageAllocatorFn imageFactory[IGFX_MAX_PRODUCT] = {};

ImageImp::~ImageImp() {
    if (this->device != nullptr) {
        this->device->getNEODevice()->getMemoryManager()->freeGraphicsMemory(this->allocation);
    }
}

ze_result_t ImageImp::destroy() {
    delete this;
    return ZE_RESULT_SUCCESS;
}

Image *Image::create(uint32_t productFamily, Device *device, const ze_image_desc_t *desc) {
    ImageAllocatorFn allocator = nullptr;
    if (productFamily < IGFX_MAX_PRODUCT) {
        allocator = imageFactory[productFamily];
    }

    ImageImp *image = nullptr;
    if (allocator) {
        image = static_cast<ImageImp *>((*allocator)());
        if (!image->initialize(device, desc)) {
            image->destroy();
            image = nullptr;
        }
    }

    return image;
}

bool ImageImp::initialize(Device *device, const ze_image_desc_t *desc) {
    return true;
}
} // namespace L0
