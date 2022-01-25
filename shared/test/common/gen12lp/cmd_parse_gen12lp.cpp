/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

#include "gtest/gtest.h"
using GenStruct = NEO::GEN12LP;
using GenGfxFamily = NEO::TGLLPFamily;
#include "shared/test/common/cmd_parse/cmd_parse_base.inl"
#include "shared/test/common/cmd_parse/cmd_parse_compute_mode.inl"
#include "shared/test/common/cmd_parse/cmd_parse_gpgpu_walker.inl"
#include "shared/test/common/cmd_parse/cmd_parse_l3_control.inl"
#include "shared/test/common/cmd_parse/cmd_parse_mi_arb.inl"
#include "shared/test/common/cmd_parse/cmd_parse_sip.inl"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.inl"

size_t getAdditionalCommandLengthHwSpecific(void *cmd) {
    using L3_CONTROL_WITH_POST_SYNC = typename GenGfxFamily::L3_CONTROL;
    using L3_CONTROL_WITHOUT_POST_SYNC = typename GenGfxFamily::L3_CONTROL;

    auto pCmdWithPostSync = genCmdCast<L3_CONTROL_WITH_POST_SYNC *>(cmd);
    if (pCmdWithPostSync)
        return pCmdWithPostSync->getBase().TheStructure.Common.Length + 2;

    auto pCmdWithoutPostSync = genCmdCast<L3_CONTROL_WITHOUT_POST_SYNC *>(cmd);
    if (pCmdWithoutPostSync)
        return pCmdWithoutPostSync->getBase().TheStructure.Common.Length + 2;

    return 0;
}

const char *getAdditionalCommandNameHwSpecific(void *cmd) {
    using L3_CONTROL_WITH_POST_SYNC = typename GenGfxFamily::L3_CONTROL;
    using L3_CONTROL_WITHOUT_POST_SYNC = typename GenGfxFamily::L3_CONTROL;

    if (nullptr != genCmdCast<L3_CONTROL_WITH_POST_SYNC *>(cmd)) {
        return "L3_CONTROL(POST_SYNC)";
    }

    if (nullptr != genCmdCast<L3_CONTROL_WITHOUT_POST_SYNC *>(cmd)) {
        return "L3_CONTROL(NO_POST_SYNC)";
    }

    return "UNKNOWN";
}

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
        auto pCmd = genCmdCast<STATE_COMPUTE_MODE *>(cmd);
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

    return getAdditionalCommandLengthHwSpecific(cmd);
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

    if (nullptr != genCmdCast<STATE_COMPUTE_MODE *>(cmd)) {
        return "STATE_COMPUTE_MODE";
    }

    if (nullptr != genCmdCast<GPGPU_CSR_BASE_ADDRESS *>(cmd)) {
        return "GPGPU_CSR_BASE_ADDRESS";
    }

    if (nullptr != genCmdCast<STATE_SIP *>(cmd)) {
        return "STATE_SIP";
    }

    return getAdditionalCommandNameHwSpecific(cmd);
}

template struct CmdParse<GenGfxFamily>;

namespace NEO {
template void HardwareParse::findHardwareCommands<TGLLPFamily>();
template void HardwareParse::findHardwareCommands<TGLLPFamily>(IndirectHeap *);
template const void *HardwareParse::getStatelessArgumentPointer<TGLLPFamily>(const KernelInfo &kernelInfo, uint32_t indexArg, IndirectHeap &ioh, uint32_t rootDeviceIndex);
} // namespace NEO
