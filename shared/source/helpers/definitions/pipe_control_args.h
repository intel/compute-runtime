/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/definitions/pipe_control_args_base.h"

namespace NEO {
struct HardwareInfo;

struct PipeControlArgs : PipeControlArgsBase {
    PipeControlArgs() = default;
    PipeControlArgs(bool dcFlush) : PipeControlArgsBase(dcFlush) {}
    void adjustArgs(const HardwareInfo &hwInfo);
};
} // namespace NEO
