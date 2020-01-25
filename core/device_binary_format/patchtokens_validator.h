/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/device_binary_format/device_binary_formats.h"

#include <string>

namespace NEO {

namespace PatchTokenBinary {
extern bool allowUnhandledTokens;

struct ProgramFromPatchtokens;

DecodeError validate(const ProgramFromPatchtokens &decodedProgram,
                     std::string &outErrReason, std::string &outWarnings);

} // namespace PatchTokenBinary

} // namespace NEO
