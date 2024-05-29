/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/product_helper.h"

namespace NEO {
bool GmmHelper::deferMOCSToPatIndex() const {
    return this->rootDeviceEnvironment.getProductHelper().deferMOCSToPatIndex();
}
} // namespace NEO