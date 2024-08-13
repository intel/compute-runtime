/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/const_stringref.h"

#include <string>
#include <vector>

class OclocArgHelper;
namespace Ocloc {
void printOclocCmdLine(OclocArgHelper &wrapper, const std::vector<std::string> &args);
void printHelp(OclocArgHelper &wrapper);

const std::string &getOclocCurrentLibName();
const std::string &getOclocFormerLibName();

namespace CommandNames {
inline constexpr NEO::ConstStringRef compile = "compile";
inline constexpr NEO::ConstStringRef link = "link";
inline constexpr NEO::ConstStringRef disassemble = "disasm";
inline constexpr NEO::ConstStringRef assemble = "asm";
inline constexpr NEO::ConstStringRef multi = "multi";
inline constexpr NEO::ConstStringRef validate = "validate";
inline constexpr NEO::ConstStringRef query = "query";
inline constexpr NEO::ConstStringRef ids = "ids";
inline constexpr NEO::ConstStringRef concat = "concat";
} // namespace CommandNames
namespace Commands {
int compile(OclocArgHelper *argHelper, const std::vector<std::string> &args);
int link(OclocArgHelper *argHelper, const std::vector<std::string> &args);
int disassemble(OclocArgHelper *argHelper, const std::vector<std::string> &args);
int assemble(OclocArgHelper *argHelper, const std::vector<std::string> &args);
int multi(OclocArgHelper *argHelper, const std::vector<std::string> &args);
int validate(OclocArgHelper *argHelper, const std::vector<std::string> &args);
int query(OclocArgHelper *argHelper, const std::vector<std::string> &args);
int ids(OclocArgHelper *argHelper, const std::vector<std::string> &args);
int concat(OclocArgHelper *argHelper, const std::vector<std::string> &args);
} // namespace Commands
} // namespace Ocloc
