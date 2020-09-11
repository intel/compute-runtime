/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/surface_format_info.h"

#include "level_zero/core/source/image/image.h"

namespace L0 {

struct ImageImp : public Image {
    ze_result_t destroy() override;

    virtual ze_result_t initialize(Device *device, const ze_image_desc_t *desc) = 0;

    ~ImageImp() override;

    NEO::GraphicsAllocation *getAllocation() override { return allocation; }
    NEO::ImageInfo getImageInfo() override { return imgInfo; }
    ze_image_desc_t getImageDesc() override {
        return imageFormatDesc;
    }
    static NEO::ImageType convertType(const ze_image_type_t type) {
        switch (type) {
        case ZE_IMAGE_TYPE_2D:
            return NEO::ImageType::Image2D;
        case ZE_IMAGE_TYPE_3D:
            return NEO::ImageType::Image3D;
        case ZE_IMAGE_TYPE_2DARRAY:
            return NEO::ImageType::Image2DArray;
        case ZE_IMAGE_TYPE_1D:
            return NEO::ImageType::Image1D;
        case ZE_IMAGE_TYPE_1DARRAY:
            return NEO::ImageType::Image1DArray;
        case ZE_IMAGE_TYPE_BUFFER:
            return NEO::ImageType::Image1DBuffer;
        default:
            break;
        }
        return NEO::ImageType::Invalid;
    }

    static NEO::ImageDescriptor convertDescriptor(const ze_image_desc_t &imageDesc) {
        NEO::ImageDescriptor desc = {};
        desc.fromParent = false;
        desc.imageArraySize = imageDesc.arraylevels;
        desc.imageDepth = imageDesc.depth;
        desc.imageHeight = imageDesc.height;
        desc.imageRowPitch = 0u;
        desc.imageSlicePitch = 0u;
        desc.imageType = convertType(imageDesc.type);
        desc.imageWidth = imageDesc.width;
        desc.numMipLevels = imageDesc.miplevels;
        desc.numSamples = 0u;
        return desc;
    }

  protected:
    Device *device = nullptr;
    NEO::ImageInfo imgInfo = {};
    NEO::GraphicsAllocation *allocation = nullptr;
    ze_image_desc_t imageFormatDesc = {};
};
} // namespace L0
