/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/surface_format_info.h"

#include "level_zero/core/source/image.h"

namespace L0 {

const int ZE_IMAGE_FORMAT_RENDER_LAYOUT_MAX = ZE_IMAGE_FORMAT_LAYOUT_P416;
const int ZE_IMAGE_FORMAT_MEDIA_LAYOUT_OFFSET = ZE_IMAGE_FORMAT_LAYOUT_Y8;
const int ZE_IMAGE_FORMAT_MEDIA_LAYOUT_MAX = ZE_IMAGE_FORMAT_LAYOUT_P416;
const int ZE_IMAGE_FORMAT_LAYOUT_MAX = ZE_IMAGE_FORMAT_MEDIA_LAYOUT_MAX;
const int ZE_IMAGE_FORMAT_TYPE_MAX = ZE_IMAGE_FORMAT_TYPE_FLOAT;
const int ZE_IMAGE_FORMAT_SWIZZLE_MAX = ZE_IMAGE_FORMAT_SWIZZLE_X;

struct ImageFormatDescriptor {
    enum formatType {
        RENDER,
        MEDIA
    };
    uint32_t channelBytes;
    uint32_t bitsPerPixel;
    enum formatType type;

    ImageFormatDescriptor(enum formatType type, uint32_t cBytes, uint32_t bpp = 0)
        : channelBytes(cBytes), type(type) {
        bitsPerPixel = bpp > 0 ? bpp : cBytes * 8;
    }
};

struct ImageImp : public Image {
    using IFD = ImageFormatDescriptor;

    ze_result_t destroy() override;

    virtual bool initialize(Device *device, const ze_image_desc_t *desc);

    const ImageFormatDescriptor formats[ZE_IMAGE_FORMAT_LAYOUT_MAX + 1] = {
        IFD(IFD::RENDER, 1),    // ZE_IMAGE_FORMAT_LAYOUT_8
        IFD(IFD::RENDER, 2),    // ZE_IMAGE_FORMAT_LAYOUT_16
        IFD(IFD::RENDER, 4),    // ZE_IMAGE_FORMAT_LAYOUT_32
        IFD(IFD::RENDER, 2),    // ZE_IMAGE_FORMAT_LAYOUT_8_8
        IFD(IFD::RENDER, 4),    // ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8
        IFD(IFD::RENDER, 4),    // ZE_IMAGE_FORMAT_LAYOUT_16_16
        IFD(IFD::RENDER, 8),    // ZE_IMAGE_FORMAT_LAYOUT_16_16_16_16
        IFD(IFD::RENDER, 8),    // ZE_IMAGE_FORMAT_LAYOUT_32_32
        IFD(IFD::RENDER, 16),   // ZE_IMAGE_FORMAT_LAYOUT_32_32_32_32
        IFD(IFD::RENDER, 4),    // ZE_IMAGE_FORMAT_LAYOUT_10_10_10_2
        IFD(IFD::RENDER, 4),    // ZE_IMAGE_FORMAT_LAYOUT_11_11_10
        IFD(IFD::RENDER, 2),    // ZE_IMAGE_FORMAT_LAYOUT_5_6_5
        IFD(IFD::RENDER, 2),    // ZE_IMAGE_FORMAT_LAYOUT_5_5_5_1
        IFD(IFD::RENDER, 2),    // ZE_IMAGE_FORMAT_LAYOUT_4_4_4_4
        IFD(IFD::MEDIA, 1),     // ZE_IMAGE_FORMAT_LAYOUT_Y8
        IFD(IFD::MEDIA, 1, 12), // ZE_IMAGE_FORMAT_LAYOUT_NV12
        IFD(IFD::MEDIA, 2),     // ZE_IMAGE_FORMAT_LAYOUT_YUYV
        IFD(IFD::MEDIA, 2),     // ZE_IMAGE_FORMAT_LAYOUT_VYUY
        IFD(IFD::MEDIA, 2),     // ZE_IMAGE_FORMAT_LAYOUT_YVYU
        IFD(IFD::MEDIA, 2),     // ZE_IMAGE_FORMAT_LAYOUT_UYVY
        IFD(IFD::MEDIA, 4),     // ZE_IMAGE_FORMAT_LAYOUT_AYUV
        IFD(IFD::MEDIA, 0),     // ZE_IMAGE_FORMAT_LAYOUT_YUAV
        IFD(IFD::MEDIA, 2, 24), // ZE_IMAGE_FORMAT_LAYOUT_P010
        IFD(IFD::MEDIA, 4, 32), // ZE_IMAGE_FORMAT_LAYOUT_Y410
        IFD(IFD::MEDIA, 2, 24), // ZE_IMAGE_FORMAT_LAYOUT_P012
        IFD(IFD::MEDIA, 2, 16), // ZE_IMAGE_FORMAT_LAYOUT_Y16
        IFD(IFD::MEDIA, 2, 24), // ZE_IMAGE_FORMAT_LAYOUT_P016
        IFD(IFD::MEDIA, 2, 32), // ZE_IMAGE_FORMAT_LAYOUT_Y216
        IFD(IFD::MEDIA, 2, 32), // ZE_IMAGE_FORMAT_LAYOUT_P216
        IFD(IFD::MEDIA, 2, 64)  // ZE_IMAGE_FORMAT_LAYOUT_P416
    };

    ImageImp() {}

    ~ImageImp() override = default;

    NEO::GraphicsAllocation *getAllocation() override { return allocation; }

    void decoupleAllocation(NEO::CommandContainer &commandContainer) override {
        commandContainer.getDeallocationContainer().push_back(allocation);
        allocation = NULL;
    }

    size_t getSizeInBytes() override { return imgInfo.size; }
    NEO::ImageInfo getImageInfo() override { return imgInfo; }
    ze_image_desc_t getImageDesc() override {
        return imageFormatDesc;
    }

  protected:
    Device *device = nullptr;
    NEO::ImageInfo imgInfo = {};
    NEO::GraphicsAllocation *allocation = nullptr;
    ze_image_desc_t imageFormatDesc = {};
};

} // namespace L0
