/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/product_helper.h"

namespace NEO {
extern ProductHelperCreateFunctionType productHelperFactory[IGFX_MAX_PRODUCT];

template <typename MockProductHelper>
class RAIIProductHelperFactory {
  public:
    PRODUCT_FAMILY productFamily;

    ProductHelperCreateFunctionType createProductHelper;
    std::unique_ptr<ProductHelper> productHelperBackup;
    MockProductHelper *mockProductHelper;
    RootDeviceEnvironment &rootDeviceEnvironment;

    static std::unique_ptr<ProductHelper> create() {

        return std::unique_ptr<ProductHelper>(new MockProductHelper());
    }

    ProductHelperCreateFunctionType createMockProductHelper = create;

    RAIIProductHelperFactory(RootDeviceEnvironment &rootDeviceEnvironment) : rootDeviceEnvironment(rootDeviceEnvironment) {

        this->productFamily = rootDeviceEnvironment.getHardwareInfo()->platform.eProductFamily;
        createProductHelper = productHelperFactory[productFamily];
        productHelperFactory[productFamily] = createMockProductHelper;
        productHelperBackup = createMockProductHelper();
        rootDeviceEnvironment.productHelper.swap(productHelperBackup);
        mockProductHelper = static_cast<MockProductHelper *>(rootDeviceEnvironment.productHelper.get());
    }

    ~RAIIProductHelperFactory() {
        productHelperFactory[productFamily] = createProductHelper;
        rootDeviceEnvironment.productHelper.swap(productHelperBackup);
    }
};
} // namespace NEO