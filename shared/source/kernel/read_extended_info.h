/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "kernel_descriptor.h"
#include "patch_shared.h"

#include <memory>

namespace NEO {
void readExtendedInfo(std::unique_ptr<ExtendedInfoBase> &extendedInfo, const iOpenCL::SPatchExecutionEnvironment &execEnv);
} // namespace NEO
