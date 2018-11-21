/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/mem_obj/image.h"
#include "runtime/platform/platform.h"
#include "unit_tests/helpers/hw_info_helper.h"
#include "unit_tests/mocks/mock_context.h"
#include "CL/cl.h"
#include <cassert>
#include <cstdio>

struct Image1dDefaults {
    enum { flags = 0 };
    static const cl_image_format imageFormat;
    static const cl_image_desc imageDesc;
    static void *hostPtr;
    static OCLRT::Context *context;
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
    using Context = OCLRT::Context;
    using Image = OCLRT::Image;
    using MockContext = OCLRT::MockContext;

    static Image *create(Context *context = Traits::context, const cl_image_desc *imgDesc = &Traits::imageDesc,
                         const cl_image_format *imgFormat = &Traits::imageFormat) {
        auto retVal = CL_INVALID_VALUE;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(Traits::flags, imgFormat);
        auto image = Image::create(
            context,
            Traits::flags,
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

template <typename FamilyType>
class ImageClearColorFixture {
  public:
    using GmmHelper = OCLRT::GmmHelper;
    using MockContext = OCLRT::MockContext;
    using Image = OCLRT::Image;
    using ImageHw = OCLRT::ImageHw<FamilyType>;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    using AUXILIARY_SURFACE_MODE = typename FamilyType::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    void SetUp() {
        hwInfoHelper.hwInfo.capabilityTable.ftrRenderCompressedImages = true;

        OCLRT::platformImpl.reset();
        OCLRT::constructPlatform()->peekExecutionEnvironment()->initGmm(&hwInfoHelper.hwInfo);

        surfaceState = RENDER_SURFACE_STATE::sInit();
        surfaceState.setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    }
    void TearDown() {
    }

    ImageHw *createImageHw() {
        image.reset(ImageHelper<Image2dDefaults>::create(&context));
        return static_cast<ImageHw *>(image.get());
    }

    RENDER_SURFACE_STATE surfaceState;
    HwInfoHelper hwInfoHelper;

  protected:
    MockContext context;

    std::unique_ptr<Image> image;
    ImageHw *imageHw = nullptr;
};
