/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#
#include "shared/source/gmm_helper/gmm.h"

using namespace NEO;

void Gmm::applyExtraMemoryFlags(const StorageInfo &storageInfo) {}
bool Gmm::extraMemoryFlagsRequired() { return false; }
void Gmm::applyAppResource(const StorageInfo &storageInfo) {}
void Gmm::applyExtraAuxInitFlag() {}
