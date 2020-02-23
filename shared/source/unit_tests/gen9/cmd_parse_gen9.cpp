/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/gen_common/gen_cmd_parse.h"

#include "gtest/gtest.h"
using GenStruct = NEO::GEN9;
using GenGfxFamily = NEO::SKLFamily;
#include "opencl/test/unit_test/gen_common/cmd_parse_base.inl"
#include "opencl/test/unit_test/gen_common/cmd_parse_base_mi_arb.inl"
#include "opencl/test/unit_test/gen_common/cmd_parse_gpgpu_walker.inl"
#include "opencl/test/unit_test/gen_common/cmd_parse_sip.inl"
#include "opencl/test/unit_test/helpers/hw_parse.h"
#include "opencl/test/unit_test/helpers/hw_parse.inl"

template <>
size_t CmdParse<GenGfxFamily>::getCommandLengthHwSpecific(void *cmd) {
    {
        auto pCmd = genCmdCast<GPGPU_WALKER *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MEDIA_VFE_STATE *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MEDIA_STATE_FLUSH *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<GPGPU_CSR_BASE_ADDRESS *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<STATE_SIP *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    return 0;
}

template <>
const char *CmdParse<GenGfxFamily>::getCommandNameHwSpecific(void *cmd) {
    if (nullptr != genCmdCast<GPGPU_WALKER *>(cmd)) {
        return "GPGPU_WALKER";
    }

    if (nullptr != genCmdCast<MEDIA_INTERFACE_DESCRIPTOR_LOAD *>(cmd)) {
        return "MEDIA_INTERFACE_DESCRIPTOR_LOAD";
    }

    if (nullptr != genCmdCast<MEDIA_VFE_STATE *>(cmd)) {
        return "MEDIA_VFE_STATE";
    }

    if (nullptr != genCmdCast<MEDIA_STATE_FLUSH *>(cmd)) {
        return "MEDIA_STATE_FLUSH";
    }

    if (nullptr != genCmdCast<GPGPU_CSR_BASE_ADDRESS *>(cmd)) {
        return "GPGPU_CSR_BASE_ADDRESS";
    }

    if (nullptr != genCmdCast<STATE_SIP *>(cmd)) {
        return "STATE_SIP";
    }
    return "UNKNOWN";
}

template struct CmdParse<GenGfxFamily>;

namespace NEO {
template void HardwareParse::findHardwareCommands<SKLFamily>();
template void HardwareParse::findHardwareCommands<SKLFamily>(IndirectHeap *);
template const void *HardwareParse::getStatelessArgumentPointer<SKLFamily>(const Kernel &kernel, uint32_t indexArg, IndirectHeap &ioh);
} // namespace NEO
