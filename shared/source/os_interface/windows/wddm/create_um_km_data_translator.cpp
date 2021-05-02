/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"

namespace NEO {

std::unique_ptr<UmKmDataTranslator> createUmKmDataTranslator(const Gdi &gdi, D3DKMT_HANDLE adapter) {
    return std::make_unique<UmKmDataTranslator>();
}

} // namespace NEO
