/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/cmd_parse_l3_control.inl"

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
