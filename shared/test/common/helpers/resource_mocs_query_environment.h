/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_product_helper.h"

#include <memory>

namespace NEO {

struct ResourceMocsQueryEnvironment {
    ResourceMocsQueryEnvironment() : productHelperBackup(getRootDeviceEnvironment()) {
        getRootDeviceEnvironment().gmmHelper = std::make_unique<GmmHelper>(getRootDeviceEnvironment());
    }

    RootDeviceEnvironment &getRootDeviceEnvironment() {
        return *executionEnvironment.rootDeviceEnvironments[0];
    }

    GmmHelper *getGmmHelper() {
        return getRootDeviceEnvironment().getGmmHelper();
    }

    MockExecutionEnvironment executionEnvironment{};
    RAIIProductHelperFactory<MockProductHelper> productHelperBackup;
};

} // namespace NEO
