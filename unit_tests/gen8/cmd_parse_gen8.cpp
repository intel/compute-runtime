/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/gen_common/gen_cmd_parse.h"
#include "gtest/gtest.h"
using GenStruct = OCLRT::GEN8;
using GenGfxFamily = OCLRT::BDW;
#include "unit_tests/gen_common/cmd_parse_base.inl"
#include "unit_tests/gen_common/cmd_parse_base_mi_arb.inl"
#include "unit_tests/gen_common/cmd_parse_gpgpu_walker.inl"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/helpers/hw_parse.inl"

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

template struct CmdParse<GenGfxFamily>;

namespace OCLRT {
template void HardwareParse::findHardwareCommands<BDWFamily>();
template const void *HardwareParse::getStatelessArgumentPointer<BDWFamily>(const Kernel &kernel, uint32_t indexArg, IndirectHeap &ioh);
} // namespace OCLRT
