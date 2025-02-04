/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface.h"

namespace NEO {
class MockEuDebugInterface : public EuDebugInterface {
  public:
    static char sysFsContent;
    static constexpr const char *sysFsXeEuDebugFile = "/mock_eudebug";
    static constexpr uintptr_t sysFsFd = 0xE0DEB0;
    static constexpr EuDebugInterfaceType euDebugInterfaceType = EuDebugInterfaceType::upstream;
    bool isExecQueuePageFaultEnableSupported() override { return pageFaultEnableSupported; };
    uint32_t getParamValue(EuDebugParam param) const override;

    bool pageFaultEnableSupported = false;
};

} // namespace NEO
