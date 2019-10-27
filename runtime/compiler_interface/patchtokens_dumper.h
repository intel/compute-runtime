/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>

namespace NEO {

namespace PatchTokenBinary {

struct ProgramFromPatchtokens;
struct KernelFromPatchtokens;
struct KernelArgFromPatchtokens;

std::string asString(const ProgramFromPatchtokens &prog);
std::string asString(const KernelFromPatchtokens &kern);
std::string asString(const KernelArgFromPatchtokens &arg, const std::string &indent);

} // namespace PatchTokenBinary

} // namespace NEO
