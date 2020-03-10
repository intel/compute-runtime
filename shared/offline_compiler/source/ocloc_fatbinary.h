/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/const_stringref.h"

#include "igfxfmid.h"

#include <vector>

class OclocArgHelper;
namespace NEO {

bool requestedFatBinary(int argc, const char *argv[]);

int buildFatbinary(int argc, const char *argv[], OclocArgHelper *helper);

std::vector<PRODUCT_FAMILY> getAllSupportedTargetPlatforms();
std::vector<ConstStringRef> toProductNames(const std::vector<PRODUCT_FAMILY> &productIds);
PRODUCT_FAMILY asProductId(ConstStringRef product, const std::vector<PRODUCT_FAMILY> &allSupportedPlatforms);
GFXCORE_FAMILY asGfxCoreId(ConstStringRef core);
void appendPlatformsForGfxCore(GFXCORE_FAMILY core, const std::vector<PRODUCT_FAMILY> &allSupportedPlatforms, std::vector<PRODUCT_FAMILY> &out);
std::vector<ConstStringRef> getTargetPlatformsForFatbinary(ConstStringRef deviceArg);

} // namespace NEO
