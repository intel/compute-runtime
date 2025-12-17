/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/sip_kernel_type.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/os_library.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace NEO {
class Device;

enum class SipRegisterType {
    eInvalid,
    eGRF,
    eAccumulator,
    eFlags,
    eState,
    eControl,
    eNotification,
    eAddress,
    eChannelEnable,
    eMessageControl,
    eStackPointer,
    eIP,
    eTDR,
    ePerformance,
    eFlowControl,
    eDebug,
    eLegacyDebug,
    eMME,
    eScalar,
    eSRF,
    eDirect,
    eRandom,
    eCommand,
};

struct SipLibThreadId {
    uint32_t slice;
    uint32_t subslice;
    uint32_t eu;
    uint32_t thread;
};

struct RegsetDescExt {
    uint16_t num;
    uint16_t bytes;
    uint32_t stride;
    std::array<uint8_t, 16> validMask;
};

class SipExternalLib : NonCopyableAndNonMovableClass {
  public:
    virtual ~SipExternalLib() {}
    static SipExternalLib *getSipExternalLibInstance();
    virtual int getSipKernelBinary(NEO::Device &device, SipKernelType type, std::vector<char> &retBinary, std::vector<char> &stateSaveAreaHeader) = 0;
    virtual bool createRegisterDescriptorMap() = 0;
    virtual SIP::regset_desc *getRegsetDescFromMap(SipRegisterType type) = 0;
    virtual std::optional<RegsetDescExt> getRegsetDescExt(SipRegisterType type) const = 0;
    virtual size_t getStateSaveAreaSize() const = 0;
    virtual bool getSipLibRegisterAccess(void *sipHandle, SipLibThreadId sipThreadId, SipRegisterType type, uint32_t *registerCount, uint32_t *registerStartOffset) = 0;
    virtual bool getSlmStartOffset(void *siphandle, SipLibThreadId threadId, uint32_t *startOffset) = 0;
    virtual bool getBarrierStartOffset(void *siphandle, SipLibThreadId threadId, uint32_t *startOffset) = 0;
};

} // namespace NEO
