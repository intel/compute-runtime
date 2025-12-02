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

struct SipLibThreadId {
    uint32_t slice;
    uint32_t subslice;
    uint32_t eu;
    uint32_t thread;
};
class SipExternalLib : NonCopyableAndNonMovableClass {
  public:
    virtual ~SipExternalLib() {}
    static SipExternalLib *getSipExternalLibInstance();
    virtual int getSipKernelBinary(NEO::Device &device, SipKernelType type, std::vector<char> &retBinary, std::vector<char> &stateSaveAreaHeader) = 0;
    virtual bool createRegisterDescriptorMap() = 0;
    virtual SIP::regset_desc *getRegsetDescFromMap(uint32_t type) = 0;
    virtual size_t getStateSaveAreaSize() const = 0;
    virtual bool getSipLibRegisterAccess(void *sipHandle, SipLibThreadId sipThreadId, uint32_t sipRegisterType, uint32_t *registerCount, uint32_t *registerStartOffset) = 0;
    virtual uint32_t getSipLibCommandRegisterType() = 0;
    virtual bool getSlmStartOffset(void *siphandle, SipLibThreadId threadId, uint32_t *startOffset) = 0;
    virtual bool getBarrierStartOffset(void *siphandle, SipLibThreadId threadId, uint32_t *startOffset) = 0;
};

} // namespace NEO
