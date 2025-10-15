/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/sip_kernel_type.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/os_library.h"

#include <memory>
#include <string>
#include <vector>

namespace NEO {
class Device;

class SipExternalLib : NonCopyableAndNonMovableClass {
  public:
    virtual ~SipExternalLib() {}
    static SipExternalLib *getSipExternalLibInstance();
    virtual int getSipKernelBinary(NEO::Device &device, SipKernelType type, std::vector<char> &retBinary, std::vector<char> &stateSaveAreaHeader) = 0;
    virtual bool createRegisterDescriptorMap() = 0;
    virtual SIP::regset_desc *getRegsetDescFromMap(uint32_t type) = 0;
    virtual size_t getStateSaveAreaSize() const = 0;
};

} // namespace NEO
