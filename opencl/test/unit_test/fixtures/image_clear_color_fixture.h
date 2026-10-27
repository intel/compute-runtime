/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/source/mem_obj/image.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "gtest/gtest.h"

#include <memory>

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
