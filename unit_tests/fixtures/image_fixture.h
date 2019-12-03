/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/hw_info.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/memory_properties_flags_helpers.h"
#include "runtime/helpers/options.h"
#include "runtime/mem_obj/image.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/mocks/mock_context.h"

#include "CL/cl.h"

#include <cassert>
#include <cstdio>

struct Image1dDefaults {
    enum { flags = 0 };
    static const cl_image_format imageFormat;
    static const cl_image_desc imageDesc;
    static void *hostPtr;
    static NEO::Context *context;
};

struct Image2dDefaults : public Image1dDefaults {
    static const cl_image_desc imageDesc;
};

struct Image3dDefaults : public Image2dDefaults {
    static const cl_image_desc imageDesc;
};

struct Image2dArrayDefaults : public Image2dDefaults {
    static const cl_image_desc imageDesc;
};

struct Image1dArrayDefaults : public Image2dDefaults {
    static const cl_image_desc imageDesc;
};

struct LuminanceImage : public Image2dDefaults {
    static const cl_image_format imageFormat;
};

template <typename BaseClass>
struct ImageUseHostPtr : public BaseClass {
    enum { flags = BaseClass::flags | CL_MEM_USE_HOST_PTR };
};

template <typename BaseClass>
struct ImageReadOnly : public BaseClass {
    enum { flags = BaseClass::flags | CL_MEM_READ_ONLY };
};

template <typename BaseClass>
struct ImageWriteOnly : public BaseClass {
    enum { flags = BaseClass::flags | CL_MEM_WRITE_ONLY };
};

template <typename Traits>
struct ImageHelper {
    using Context = NEO::Context;
    using Image = NEO::Image;
    using MockContext = NEO::MockContext;

    static Image *create(Context *context = Traits::context, const cl_image_desc *imgDesc = &Traits::imageDesc,
                         const cl_image_format *imgFormat = &Traits::imageFormat) {
        auto retVal = CL_INVALID_VALUE;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(Traits::flags, imgFormat);
        auto image = Image::create(
            context,
            NEO::MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(Traits::flags, 0),
            Traits::flags,
            0,
            surfaceFormat,
            imgDesc,
            Traits::hostPtr,
            retVal);

        assert(image != nullptr);
        return image;
    }
};

template <typename Traits = Image1dDefaults>
struct Image1dHelper : public ImageHelper<Traits> {
};

template <typename Traits = Image2dDefaults>
struct Image2dHelper : public ImageHelper<Traits> {
};

template <typename Traits = Image3dDefaults>
struct Image3dHelper : public ImageHelper<Traits> {
};

template <typename Traits = Image2dArrayDefaults>
struct Image2dArrayHelper : public ImageHelper<Traits> {
};

template <typename Traits = Image1dArrayDefaults>
struct Image1dArrayHelper : public ImageHelper<Traits> {
};

struct ImageClearColorFixture : ::testing::Test {
    using MockContext = NEO::MockContext;
    using Image = NEO::Image;

    template <typename FamilyType>
    void setUpImpl() {
        hardwareInfo.capabilityTable.ftrRenderCompressedImages = true;

        NEO::platformImpl.reset();
        NEO::constructPlatform()->peekExecutionEnvironment()->setHwInfo(&hardwareInfo);
        NEO::platform()->peekExecutionEnvironment()->initGmm();
    }

    template <typename FamilyType>
    typename FamilyType::RENDER_SURFACE_STATE getSurfaceState() {
        using AUXILIARY_SURFACE_MODE = typename FamilyType::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;
        auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
        surfaceState.setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
        return surfaceState;
    }

    NEO::HardwareInfo hardwareInfo = **NEO::platformDevices;
    MockContext context;
    std::unique_ptr<Image> image;
};
