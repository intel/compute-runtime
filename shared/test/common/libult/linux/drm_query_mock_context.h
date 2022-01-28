/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"

using namespace NEO;

struct DrmQueryMockContext {
    const HardwareInfo *hwInfo;
    const RootDeviceEnvironment &rootDeviceEnvironment;
    const bool &failRetTopology;
};
