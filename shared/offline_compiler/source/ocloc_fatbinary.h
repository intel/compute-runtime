/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/utilities/const_stringref.h"

#include "igfxfmid.h"

#include <cstdint>
#include <string>
#include <vector>

namespace NEO {

bool requestedFatBinary(const std::vector<std::string> &args, OclocArgHelper *helper);
inline bool requestedFatBinary(int argc, const char *argv[], OclocArgHelper *helper) {
    std::vector<std::string> args;
    args.assign(argv, argv + argc);
    return requestedFatBinary(args, helper);
}

int buildFatBinary(const std::vector<std::string> &args, OclocArgHelper *argHelper);
inline int buildFatBinary(int argc, const char *argv[], OclocArgHelper *argHelper) {
    std::vector<std::string> args;
    args.assign(argv, argv + argc);
    return buildFatBinary(args, argHelper);
}

template <typename Target>
void getProductsAcronymsForTarget(std::vector<NEO::ConstStringRef> &out, Target target, OclocArgHelper *argHelper);
std::vector<ConstStringRef> getTargetProductsForFatbinary(ConstStringRef deviceArg, OclocArgHelper *argHelper);
int buildFatBinaryForTarget(int retVal, const std::vector<std::string> &argsCopy, std::string pointerSize, Ar::ArEncoder &fatbinary,
                            OfflineCompiler *pCompiler, OclocArgHelper *argHelper, const std::string &deviceConfig);
int appendGenericIr(Ar::ArEncoder &fatbinary, const std::string &inputFile, OclocArgHelper *argHelper);
std::vector<uint8_t> createEncodedElfWithSpirv(const ArrayRef<const uint8_t> &spirv);

} // namespace NEO
