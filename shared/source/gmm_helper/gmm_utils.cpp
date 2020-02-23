/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/surface_format_info.h"

using namespace NEO;

void Gmm::applyAuxFlagsForImage(ImageInfo &imgInfo) {}
void Gmm::applyAuxFlagsForBuffer(bool preferRenderCompression) {}

void Gmm::applyMemoryFlags(bool systemMemoryPool, StorageInfo &storageInfo) { this->useSystemMemoryPool = systemMemoryPool; }
