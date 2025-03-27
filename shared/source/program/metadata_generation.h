/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/arrayref.h"

#include <mutex>
#include <vector>

namespace NEO {

struct KernelInfo;

struct MetadataGeneration {
    void callPopulateZebinExtendedArgsMetadataOnce(const ArrayRef<const uint8_t> refBin, size_t kernelMiscInfoOffset, std::vector<NEO::KernelInfo *> &kernelInfos);
    void callGenerateDefaultExtendedArgsMetadataOnce(std::vector<NEO::KernelInfo *> &kernelInfos);

  private:
    std::once_flag extractAndDecodeMetadataOnceFlag;
    std::once_flag generateDefaultMetadataOnceFlag;
};

} // namespace NEO