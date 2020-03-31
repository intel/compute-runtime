/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"

#include "gtest/gtest.h"
using GenStruct = NEO::GEN8;
using GenGfxFamily = NEO::BDWFamily;
#include "shared/test/unit_test/cmd_parse/cmd_parse_base.inl"
#include "shared/test/unit_test/cmd_parse/cmd_parse_base_mi_arb.inl"
#include "shared/test/unit_test/cmd_parse/cmd_parse_gpgpu_walker.inl"
#include "shared/test/unit_test/cmd_parse/cmd_parse_sip.inl"
#include "shared/test/unit_test/cmd_parse/hw_parse.h"
#include "shared/test/unit_test/cmd_parse/hw_parse.inl"

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
    return "UNKNOWN";
}

template struct CmdParse<GenGfxFamily>;

namespace NEO {
template void HardwareParse::findHardwareCommands<BDWFamily>();
template void HardwareParse::findHardwareCommands<BDWFamily>(IndirectHeap *);
template const void *HardwareParse::getStatelessArgumentPointer<BDWFamily>(const Kernel &kernel, uint32_t indexArg, IndirectHeap &ioh);
} // namespace NEO
