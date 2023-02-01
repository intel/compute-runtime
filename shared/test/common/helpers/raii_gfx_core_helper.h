/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {
extern GfxCoreHelperCreateFunctionType gfxCoreHelperFactory[IGFX_MAX_CORE];

template <class MockHelper>
class RAIIGfxCoreHelperFactory {
  public:
    GFXCORE_FAMILY gfxCoreFamily;
    GfxCoreHelperCreateFunctionType createGfxCoreHelper;
    std::unique_ptr<GfxCoreHelper> gfxCoreHelperBackup;
    MockHelper *mockGfxCoreHelper;
    RootDeviceEnvironment &rootDeviceEnvironment;

    static std::unique_ptr<GfxCoreHelper> create() {

        return std::unique_ptr<GfxCoreHelper>(new MockHelper());
    }
    GfxCoreHelperCreateFunctionType createMockGfxCoreHelper = create;

    RAIIGfxCoreHelperFactory(RootDeviceEnvironment &rootDeviceEnvironment) : rootDeviceEnvironment(rootDeviceEnvironment) {
        this->gfxCoreFamily = rootDeviceEnvironment.getHardwareInfo()->platform.eRenderCoreFamily;
        createGfxCoreHelper = gfxCoreHelperFactory[this->gfxCoreFamily];
        gfxCoreHelperFactory[this->gfxCoreFamily] = createMockGfxCoreHelper;
        gfxCoreHelperBackup = createMockGfxCoreHelper();
        rootDeviceEnvironment.gfxCoreHelper.swap(gfxCoreHelperBackup);
        mockGfxCoreHelper = static_cast<MockHelper *>(rootDeviceEnvironment.gfxCoreHelper.get());
    }

    ~RAIIGfxCoreHelperFactory() {
        gfxCoreHelperFactory[this->gfxCoreFamily] = createGfxCoreHelper;
        rootDeviceEnvironment.gfxCoreHelper.swap(gfxCoreHelperBackup);
    }
};
} // namespace NEO