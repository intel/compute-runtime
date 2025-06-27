/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/helpers/cl_memory_properties_helpers.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "CL/cl.h"

struct Image1dDefaults {
    enum { flags = 0 };
    static const cl_image_format imageFormat;
    static const cl_image_desc imageDesc;
    static void *hostPtr;
    static NEO::Context *context;
};

struct Image1dBufferDefaults : public Image1dDefaults {
    static const cl_image_desc imageDesc;
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

struct ImageWithoutHostPtr : public Image1dDefaults {
    enum { flags = 0 };
    static void *hostPtr;
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

struct LuminanceImage : public ImageReadOnly<Image2dDefaults> {
    static const cl_image_format imageFormat;
};

template <typename Traits>
struct ImageHelperUlt {
    using Context = NEO::Context;
    using Image = NEO::Image;
    using MockContext = NEO::MockContext;

    static Image *create(Context *context = Traits::context, const cl_image_desc *imgDesc = &Traits::imageDesc,
                         const cl_image_format *imgFormat = &Traits::imageFormat) {
        auto retVal = CL_INVALID_VALUE;
        auto surfaceFormat = Image::getSurfaceFormatFromTable(Traits::flags, imgFormat, context->getDevice(0)->getHardwareInfo().capabilityTable.supportsOcl21Features);
        auto image = Image::create(
            context,
            NEO::ClMemoryPropertiesHelper::createMemoryProperties(Traits::flags, 0, 0, &context->getDevice(0)->getDevice()),
            Traits::flags,
            0,
            surfaceFormat,
            imgDesc,
            Traits::hostPtr,
            retVal);

        return image;
    }
};

template <typename Traits = Image1dDefaults>
struct Image1dHelperUlt : public ImageHelperUlt<Traits> {
};

template <typename Traits = Image1dBufferDefaults>
struct Image1dBufferHelperUlt : public ImageHelperUlt<Traits> {
};

template <typename Traits = Image2dDefaults>
struct Image2dHelperUlt : public ImageHelperUlt<Traits> {
};

template <typename Traits = Image3dDefaults>
struct Image3dHelperUlt : public ImageHelperUlt<Traits> {
};

template <typename Traits = Image2dArrayDefaults>
struct Image2dArrayHelperUlt : public ImageHelperUlt<Traits> {
};

template <typename Traits = Image1dArrayDefaults>
struct Image1dArrayHelperUlt : public ImageHelperUlt<Traits> {
};

struct ImageClearColorFixture : ::testing::Test {
    using MockContext = NEO::MockContext;
    using Image = NEO::Image;

    template <typename FamilyType>
    void setUpImpl() {
        hardwareInfo.capabilityTable.ftrRenderCompressedImages = true;

        NEO::platformsImpl->clear();
        NEO::constructPlatform()->peekExecutionEnvironment()->prepareRootDeviceEnvironments(1u);
        NEO::platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hardwareInfo);
        NEO::platform()->peekExecutionEnvironment()->rootDeviceEnvironments[0]->initGmm();
    }

    template <typename FamilyType>
    typename FamilyType::RENDER_SURFACE_STATE getSurfaceState() {
        using AUXILIARY_SURFACE_MODE = typename FamilyType::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;
        auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
        surfaceState.setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
        return surfaceState;
    }

    NEO::HardwareInfo hardwareInfo = *NEO::defaultHwInfo;
    MockContext context;
    std::unique_ptr<Image> image;
};
