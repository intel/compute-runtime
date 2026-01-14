/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isZesInitSupported() {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
void SysmanProductHelperHw<gfxProduct>::getSupportedSensors(std::map<zes_temp_sensors_t, uint32_t> &supportedSensorTypeMap) {
    supportedSensorTypeMap[ZES_TEMP_SENSORS_GLOBAL] = 1;
    supportedSensorTypeMap[ZES_TEMP_SENSORS_GPU] = 1;
    supportedSensorTypeMap[ZES_TEMP_SENSORS_MEMORY] = 1;
}