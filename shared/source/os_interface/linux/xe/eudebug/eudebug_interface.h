/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/xe/eudebug/eudebug_wrappers.h"

#include <memory>
#include <string>
namespace NEO {

class EuDebugInterface {
  public:
    static std::unique_ptr<EuDebugInterface> create(const std::string &sysFsPciPath);
    virtual uint32_t getParamValue(EuDebugParam param) const = 0;
    virtual bool isExecQueuePageFaultEnableSupported() { return false; };
    virtual ~EuDebugInterface() = default;
};

enum class EuDebugInterfaceType : uint32_t {
    upstream,
    prelim,
    maxValue
};

using EuDebugInterfaceCreateFunctionType = std::unique_ptr<EuDebugInterface> (*)();
extern const char *eudebugSysfsEntry[static_cast<uint32_t>(EuDebugInterfaceType::maxValue)];
extern EuDebugInterfaceCreateFunctionType eudebugInterfaceFactory[static_cast<uint32_t>(EuDebugInterfaceType::maxValue)];

class EnableEuDebugInterface {
  public:
    EnableEuDebugInterface(EuDebugInterfaceType eudebugInterfaceType, const char *sysfsEntry, EuDebugInterfaceCreateFunctionType createFunc) {
        eudebugSysfsEntry[static_cast<uint32_t>(eudebugInterfaceType)] = sysfsEntry;
        eudebugInterfaceFactory[static_cast<uint32_t>(eudebugInterfaceType)] = createFunc;
    }
};
} // namespace NEO
