/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds_base.h"
using GenStruct = NEO::GEN11;
using GenGfxFamily = NEO::ICLFamily;

#include "shared/test/common/cmd_parse/cmd_parse_base.inl"
#include "shared/test/common/cmd_parse/cmd_parse_base_mi_arb.inl"
#include "shared/test/common/cmd_parse/cmd_parse_gpgpu_walker.inl"
#include "shared/test/common/cmd_parse/cmd_parse_sip.inl"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.inl"

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
template void HardwareParse::findHardwareCommands<ICLFamily>();
template void HardwareParse::findHardwareCommands<ICLFamily>(IndirectHeap *);
template const void *HardwareParse::getStatelessArgumentPointer<ICLFamily>(const KernelInfo &kernelInfo, uint32_t indexArg, IndirectHeap &ioh, uint32_t rootDeviceIndex);
template const typename ICLFamily::RENDER_SURFACE_STATE *HardwareParse::getSurfaceState<ICLFamily>(IndirectHeap *ssh, uint32_t index);
} // namespace NEO
