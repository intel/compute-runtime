/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/utilities/const_stringref.h"

#include "compiler_options.h"
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

bool isDeviceWithPlatformAbbreviation(ConstStringRef deviceArg, OclocArgHelper *argHelper);
std::vector<PRODUCT_FAMILY> getAllSupportedTargetPlatforms();
std::vector<PRODUCT_CONFIG> getAllMatchedConfigs(const std::string device, OclocArgHelper *argHelper);
std::vector<DeviceMapping> getTargetConfigsForFatbinary(ConstStringRef deviceArg, OclocArgHelper *argHelper);
std::vector<ConstStringRef> getTargetPlatformsForFatbinary(ConstStringRef deviceArg, OclocArgHelper *argHelper);
std::vector<DeviceMapping> getProductConfigsForOpenRange(ConstStringRef openRange, OclocArgHelper *argHelper, bool rangeTo);
std::vector<DeviceMapping> getProductConfigsForClosedRange(ConstStringRef rangeFrom, ConstStringRef rangeTo, OclocArgHelper *argHelper);
std::vector<ConstStringRef> getPlatformsForClosedRange(ConstStringRef rangeFrom, ConstStringRef rangeTo, PRODUCT_FAMILY platformFrom, OclocArgHelper *argHelper);
std::vector<ConstStringRef> getPlatformsForOpenRange(ConstStringRef openRange, PRODUCT_FAMILY prodId, OclocArgHelper *argHelper, bool rangeTo);
std::vector<DeviceMapping> getProductConfigsForSpecificTargets(CompilerOptions::TokenizedString targets, OclocArgHelper *argHelper);
std::vector<ConstStringRef> getPlatformsForSpecificTargets(CompilerOptions::TokenizedString targets, OclocArgHelper *argHelper);
std::vector<ConstStringRef> toProductNames(const std::vector<PRODUCT_FAMILY> &productIds);
PRODUCT_FAMILY asProductId(ConstStringRef product, const std::vector<PRODUCT_FAMILY> &allSupportedPlatforms);
int buildFatBinaryForTarget(int retVal, std::vector<std::string> argsCopy, std::string pointerSize, Ar::ArEncoder &fatbinary,
                            OfflineCompiler *pCompiler, OclocArgHelper *argHelper, const std::string &deviceConfig);
int appendGenericIr(Ar::ArEncoder &fatbinary, const std::string &inputFile, OclocArgHelper *argHelper);
std::vector<uint8_t> createEncodedElfWithSpirv(const ArrayRef<const uint8_t> &spirv);

} // namespace NEO
