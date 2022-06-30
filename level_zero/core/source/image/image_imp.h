/*
 * Copyright (C) 2020-2021 Intel Corporation
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

    ze_result_t createView(Device *device, const ze_image_desc_t *desc, ze_image_handle_t *pImage) override;

    ze_result_t getMemoryProperties(ze_image_memory_properties_exp_t *pMemoryProperties) override {
        pMemoryProperties->rowPitch = imgInfo.rowPitch;
        pMemoryProperties->slicePitch = imgInfo.slicePitch;
        pMemoryProperties->size = imgInfo.surfaceFormat->ImageElementSizeInBytes;

        return ZE_RESULT_SUCCESS;
    }

  protected:
    bool isImageView = false;
    Device *device = nullptr;
    NEO::ImageInfo imgInfo = {};
    NEO::GraphicsAllocation *allocation = nullptr;
    ze_image_desc_t imageFormatDesc = {};
};
} // namespace L0
