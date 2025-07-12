/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/mem_obj/image.h"

#include "memory_properties_flags.h"

namespace NEO {

struct MockImageBase : public Image {
    using Image::associatedMemObject;
    using Image::imageDesc;
    using Image::imageFormat;
    using Image::is3DUAVOrRTV;
    MockGraphicsAllocation *graphicsAllocation = nullptr;

    MockImageBase(uint32_t rootDeviceIndex)
        : Image(nullptr, MemoryProperties(), cl_mem_flags{}, 0, 0, nullptr, nullptr, cl_image_format{},
                cl_image_desc{}, false, GraphicsAllocationHelper::toMultiGraphicsAllocation(new MockGraphicsAllocation(rootDeviceIndex, nullptr, 0)), false,
                0, 0, ClSurfaceFormatInfo{}, nullptr),
          graphicsAllocation(static_cast<MockGraphicsAllocation *>(multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex))) {
    }

    MockImageBase() : MockImageBase(0u) {}

    ~MockImageBase() override {
        delete this->graphicsAllocation;
    }

    MockGraphicsAllocation *getAllocation() {
        return graphicsAllocation;
    }

    void setImageArg(void *memory, bool isMediaBlockImage, uint32_t mipLevel, uint32_t rootDeviceIndex) override {}
    void transformImage2dArrayTo3d(void *memory) override {}
    void transformImage3dTo2dArray(void *memory) override {}
    size_t getSize() const override { return 0; }
};

} // namespace NEO
