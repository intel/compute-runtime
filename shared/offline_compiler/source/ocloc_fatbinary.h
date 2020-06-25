/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/const_stringref.h"

#include "igfxfmid.h"

#include <string>
#include <vector>

class OclocArgHelper;
namespace NEO {

bool requestedFatBinary(const std::vector<std::string> &args);
inline bool requestedFatBinary(int argc, const char *argv[]) {
    std::vector<std::string> args;
    args.assign(argv, argv + argc);
    return requestedFatBinary(args);
}

int buildFatBinary(const std::vector<std::string> &args, OclocArgHelper *argHelper);
inline int buildFatBinary(int argc, const char *argv[], OclocArgHelper *argHelper) {
    std::vector<std::string> args;
    args.assign(argv, argv + argc);
    return buildFatBinary(args, argHelper);
}

std::vector<PRODUCT_FAMILY> getAllSupportedTargetPlatforms();
std::vector<ConstStringRef> toProductNames(const std::vector<PRODUCT_FAMILY> &productIds);
PRODUCT_FAMILY asProductId(ConstStringRef product, const std::vector<PRODUCT_FAMILY> &allSupportedPlatforms);
std::vector<GFXCORE_FAMILY> asGfxCoreIdList(ConstStringRef core);
void appendPlatformsForGfxCore(GFXCORE_FAMILY core, const std::vector<PRODUCT_FAMILY> &allSupportedPlatforms, std::vector<PRODUCT_FAMILY> &out);
std::vector<ConstStringRef> getTargetPlatformsForFatbinary(ConstStringRef deviceArg, OclocArgHelper *argHelper);

} // namespace NEO
