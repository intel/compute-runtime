/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface.h"
namespace NEO {
class EuDebugInterfacePrelim : public EuDebugInterface {
  public:
    static constexpr const char *sysFsXeEuDebugFile = "/device/prelim_enable_eudebug";
    uint32_t getParamValue(EuDebugParam param) const override;
    bool isExecQueuePageFaultEnableSupported() override;
    uint32_t getAdditionalParamValue(EuDebugParam param) const;
};
} // namespace NEO
