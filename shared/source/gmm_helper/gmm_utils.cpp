/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_lib.h"

using namespace NEO;

void Gmm::applyExtraMemoryFlags(const StorageInfo &storageInfo) {}
bool Gmm::extraMemoryFlagsRequired() { return false; }
void Gmm::applyAppResource(const StorageInfo &storageInfo) {}
void Gmm::applyExtraAuxInitFlag() {}
void Gmm::initializeResourceParams() {
    this->resourceParamsData.resize(sizeof(GMM_RESCREATE_PARAMS));
}
