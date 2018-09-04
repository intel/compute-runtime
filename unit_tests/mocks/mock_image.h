/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/mem_obj/image.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

struct MockImageBase : public OCLRT::Image {
    using Image::graphicsAllocation;
    using Image::imageDesc;

    MockImageBase() : Image(nullptr, cl_mem_flags{}, 0, nullptr, cl_image_format{},
                            cl_image_desc{}, false, new OCLRT::MockGraphicsAllocation(nullptr, 0), false, false,
                            0, 0, OCLRT::SurfaceFormatInfo{}, nullptr) {
    }
    ~MockImageBase() override {
        delete this->graphicsAllocation;
    }

    OCLRT::MockGraphicsAllocation *getAllocation() {
        return static_cast<OCLRT::MockGraphicsAllocation *>(graphicsAllocation);
    }

    void setImageArg(void *memory, bool isMediaBlockImage, uint32_t mipLevel) override {}
    void setMediaImageArg(void *memory) override {}
    void setMediaSurfaceRotation(void *memory) override {}
    void setSurfaceMemoryObjectControlStateIndexToMocsTable(void *memory, uint32_t value) override {}
    void transformImage2dArrayTo3d(void *memory) override {}
    void transformImage3dTo2dArray(void *memory) override {}
};
