/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/cmd_parse_3d_state_btd.inl"
#include "shared/test/common/cmd_parse/cmd_parse_base.inl"
#include "shared/test/common/cmd_parse/cmd_parse_compute_mode.inl"
#include "shared/test/common/cmd_parse/cmd_parse_compute_walker_no_heap.inl"
#include "shared/test/common/cmd_parse/cmd_parse_sip.inl"

#include "gtest/gtest.h"

using STATE_SIP = GenStruct::STATE_SIP;

template <>
size_t CmdParse<GenGfxFamily>::getCommandLengthHwSpecific(void *cmd) {
    {
        auto pCmd = genCmdCast<STATE_COMPUTE_MODE *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<MI_SET_PREDICATE *>(cmd);
        if (pCmd)
            return 1;
    }
    {
        auto pCmd = genCmdCast<_3DSTATE_BTD *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<STATE_SIP *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }
    {
        auto pCmd = genCmdCast<XY_BLOCK_COPY_BLT *>(cmd);
        if (pCmd)
            return pCmd->TheStructure.Common.DwordLength + 2;
    }

    return 0;
}

template <>
const char *CmdParse<GenGfxFamily>::getCommandNameHwSpecific(void *cmd) {
    if (nullptr != genCmdCast<STATE_COMPUTE_MODE *>(cmd)) {
        return "STATE_COMPUTE_MODE";
    }

    if (nullptr != genCmdCast<MI_SET_PREDICATE *>(cmd)) {
        return "MI_SET_PREDICATE";
    }

    if (nullptr != genCmdCast<_3DSTATE_BTD *>(cmd)) {
        return "_3DSTATE_BTD";
    }

    if (nullptr != genCmdCast<STATE_SIP *>(cmd)) {
        return "STATE_SIP";
    }
    if (genCmdCast<XY_BLOCK_COPY_BLT *>(cmd)) {
        return "XY_BLOCK_COPY_BLT";
    }

    return "UNKNOWN";
}

template struct CmdParse<GenGfxFamily>;
