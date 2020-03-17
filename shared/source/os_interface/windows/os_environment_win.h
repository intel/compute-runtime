/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_environment.h"

#include <memory>

namespace NEO {
class Gdi;

struct OsEnvironmentWin : public OsEnvironment {
    OsEnvironmentWin();
    ~OsEnvironmentWin() override;

    std::unique_ptr<Gdi> gdi;
};
} // namespace NEO