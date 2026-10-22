/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/hw_device_id.h"
#include "shared/source/os_interface/windows/wddm/um_km_data_translator.h"

namespace NEO {

bool HwDeviceIdWddm::isDriverStoreValid() const {
    return this->umKmDataTranslator->enabled();
}

} // namespace NEO
