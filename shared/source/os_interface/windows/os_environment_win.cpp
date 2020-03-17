/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/os_environment_win.h"

#include "shared/source/os_interface/windows/gdi_interface.h"

namespace NEO {
OsEnvironmentWin::OsEnvironmentWin() : gdi(std::make_unique<Gdi>()){};
OsEnvironmentWin::~OsEnvironmentWin() = default;
} // namespace NEO