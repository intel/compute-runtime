/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/ras/linux/os_ras_imp.h"

#include "sysman/linux/fs_access.h"
#include "sysman/ras/ras.h"
#include "sysman/ras/ras_imp.h"

namespace L0 {
namespace ult {

class RasFsAccess : public FsAccess {};
template <>
struct Mock<RasFsAccess> : public RasFsAccess {
    MOCK_METHOD(bool, isRootUser, (), (override));
    bool userIsRoot() {
        return true;
    }
    bool userIsNotRoot() {
        return false;
    }
    Mock<RasFsAccess>() = default;
};

} // namespace ult
} // namespace L0
