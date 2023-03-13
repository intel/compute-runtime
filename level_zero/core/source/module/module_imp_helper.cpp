/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/module/module_imp.h"

namespace L0 {

void ModuleImp::createBuildExtraOptions(std::string &apiOptions, std::string &internalBuildOptions) {}

bool ModuleImp::verifyBuildOptions(std::string buildOptions) const {
    return true;
}

} // namespace L0
