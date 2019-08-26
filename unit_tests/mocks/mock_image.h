/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/mem_obj/image.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

struct MockImageBase : public NEO::Image {
    using Image::graphicsAllocation;
    using Image::imageDesc;

    MockImageBase() : Image(nullptr, cl_mem_flags{}, 0, nullptr, cl_image_format{},
                            cl_image_desc{}, false, new NEO::MockGraphicsAllocation(nullptr, 0), false,
                            0, 0, NEO::SurfaceFormatInfo{}, nullptr) {
    }
    ~MockImageBase() override {
        delete this->graphicsAllocation;
    }

    NEO::MockGraphicsAllocation *getAllocation() {
        return static_cast<NEO::MockGraphicsAllocation *>(graphicsAllocation);
    }

    void setImageArg(void *memory, bool isMediaBlockImage, uint32_t mipLevel) override {}
    void setMediaImageArg(void *memory) override {}
    void setMediaSurfaceRotation(void *memory) override {}
    void setSurfaceMemoryObjectControlStateIndexToMocsTable(void *memory, uint32_t value) override {}
    void transformImage2dArrayTo3d(void *memory) override {}
    void transformImage3dTo2dArray(void *memory) override {}
};
