/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface.h"

namespace NEO {
class EuDebugInterfaceUpstream : public EuDebugInterface {
  public:
    static constexpr const char *sysFsXeEuDebugFile = "/device/enable_eudebug";

    uint32_t getParamValue(EuDebugParam param) const override;
};

} // namespace NEO
