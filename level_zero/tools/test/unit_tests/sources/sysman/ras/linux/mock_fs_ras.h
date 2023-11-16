/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/ras/linux/os_ras_imp.h"
#include "level_zero/tools/source/sysman/ras/ras.h"
#include "level_zero/tools/source/sysman/ras/ras_imp.h"

namespace L0 {
namespace ult {

struct MockRasFsAccess : public FsAccess {
    bool mockRootUser = true;
    bool isRootUser() override {
        return mockRootUser;
    }
    MockRasFsAccess() = default;
};

} // namespace ult
} // namespace L0
