/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/compiler_interface/tokenized_string.h"
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/const_stringref.h"

#include <cstdint>
#include <string>
#include <vector>

class OclocArgHelper;

namespace NEO {
namespace Ar {
struct ArEncoder;
}
class OfflineCompiler;

bool isSpvOnly(const std::vector<std::string> &args);
bool requestedFatBinary(ConstStringRef deviceArg, OclocArgHelper *helper);
bool requestedFatBinary(const std::vector<std::string> &args, OclocArgHelper *helper);
inline bool requestedFatBinary(int argc, const char *argv[], OclocArgHelper *helper) {
    std::vector<std::string> args;
    args.assign(argv, argv + argc);
    return requestedFatBinary(args, helper);
}

int getDeviceArgValueIdx(const std::vector<std::string> &args);
int buildFatBinary(const std::vector<std::string> &args, OclocArgHelper *argHelper);
inline int buildFatBinary(int argc, const char *argv[], OclocArgHelper *argHelper) {
    std::vector<std::string> args;
    args.assign(argv, argv + argc);
    return buildFatBinary(args, argHelper);
}

template <typename Target>
void getProductsAcronymsForTarget(std::vector<NEO::ConstStringRef> &out, Target target, OclocArgHelper *argHelper);
std::vector<NEO::ConstStringRef> getProductsForRange(unsigned int productFrom, unsigned int productTo, OclocArgHelper *argHelper);
std::vector<ConstStringRef> getTargetProductsForFatbinary(ConstStringRef deviceArg, OclocArgHelper *argHelper);
int buildFatBinaryForTarget(int retVal, const std::vector<std::string> &argsCopy, std::string pointerSize, Ar::ArEncoder &fatbinary,
                            OfflineCompiler *pCompiler, OclocArgHelper *argHelper, const std::string &deviceConfig);
int appendGenericIr(Ar::ArEncoder &fatbinary, const std::string &inputFile, OclocArgHelper *argHelper, std::string options);
std::vector<uint8_t> createEncodedElfWithSpirv(const ArrayRef<const uint8_t> &spirv, const ArrayRef<const uint8_t> &options);
std::vector<ConstStringRef> getProductForSpecificTarget(const NEO::CompilerOptions::TokenizedString &targets, OclocArgHelper *argHelper);

} // namespace NEO
