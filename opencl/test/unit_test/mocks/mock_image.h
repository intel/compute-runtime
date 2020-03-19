/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"

#include "memory_properties_flags.h"

namespace NEO {

struct MockImageBase : public Image {
    using Image::graphicsAllocation;
    using Image::imageDesc;

    MockImageBase() : Image(
                          nullptr, MemoryPropertiesFlags(), cl_mem_flags{}, 0, 0, nullptr, cl_image_format{},
                          cl_image_desc{}, false, new MockGraphicsAllocation(nullptr, 0), false,
                          0, 0, ClSurfaceFormatInfo{}, nullptr) {
    }
    ~MockImageBase() override {
        delete this->graphicsAllocation;
    }

    MockGraphicsAllocation *getAllocation() {
        return static_cast<MockGraphicsAllocation *>(graphicsAllocation);
    }

    void setImageArg(void *memory, bool isMediaBlockImage, uint32_t mipLevel) override {}
    void setMediaImageArg(void *memory) override {}
    void setMediaSurfaceRotation(void *memory) override {}
    void setSurfaceMemoryObjectControlStateIndexToMocsTable(void *memory, uint32_t value) override {}
    void transformImage2dArrayTo3d(void *memory) override {}
    void transformImage3dTo2dArray(void *memory) override {}
};

} // namespace NEO
