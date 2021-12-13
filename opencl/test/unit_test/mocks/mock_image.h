/*
 * Copyright (C) 2018-2021 Intel Corporation
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
    using Image::imageDesc;
    using Image::imageFormat;
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

    void setImageArg(void *memory, bool isMediaBlockImage, uint32_t mipLevel, uint32_t rootDeviceIndex, bool useGlobalAtomics) override {}
    void setMediaImageArg(void *memory, uint32_t rootDeviceIndex) override {}
    void setMediaSurfaceRotation(void *memory) override {}
    void setSurfaceMemoryObjectControlState(void *memory, uint32_t value) override {}
    void transformImage2dArrayTo3d(void *memory) override {}
    void transformImage3dTo2dArray(void *memory) override {}
};

} // namespace NEO
