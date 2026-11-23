/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/experimental/source/mutable_cmdlist/program/mcl_encoder.h"

namespace L0::ult {

struct MockMclEncoder : public MCL::Program::Encoder::MclEncoder {
    using MCL::Program::Encoder::MclEncoder::addSymbol;
    using MCL::Program::Encoder::MclEncoder::program;
};

} // namespace L0::ult
