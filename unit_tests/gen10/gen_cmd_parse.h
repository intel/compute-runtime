/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "unit_tests/gen_common/gen_cmd_parse_base.h"

struct CnlParse : public OCLRT::CNL {
    static size_t getCommandLength(void *cmd);

    static bool parseCommandBuffer(GenCmdList &_cmds, void *_buffer, size_t _length);

    template <typename CmdType>
    static void validateCommand(GenCmdList::iterator itorBegin, GenCmdList::iterator itorEnd);
};
