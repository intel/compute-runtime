/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/const_stringref.h"

#include <vector>

class OclocArgHelper;
namespace Ocloc {
void printOclocCmdLine(const std::vector<std::string> &args);
void printHelp(OclocArgHelper *helper);

namespace CommandNames {
constexpr NEO::ConstStringRef compile = "compile";
constexpr NEO::ConstStringRef link = "link";
constexpr NEO::ConstStringRef disassemble = "disasm";
constexpr NEO::ConstStringRef assemble = "asm";
constexpr NEO::ConstStringRef multi = "multi";
constexpr NEO::ConstStringRef validate = "validate";
constexpr NEO::ConstStringRef query = "query";
constexpr NEO::ConstStringRef ids = "ids";
constexpr NEO::ConstStringRef concat = "concat";
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
