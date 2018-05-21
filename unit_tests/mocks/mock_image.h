/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "runtime/mem_obj/image.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

struct MockImageBase : public OCLRT::Image {
    using Image::imageDesc;
    using Image::graphicsAllocation;

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
    size_t getHostPtrRowPitchForMap(uint32_t mipLevel) override { return getHostPtrRowPitch(); };
    size_t getHostPtrSlicePitchForMap(uint32_t mipLevel) override { return getHostPtrSlicePitch(); };
};
