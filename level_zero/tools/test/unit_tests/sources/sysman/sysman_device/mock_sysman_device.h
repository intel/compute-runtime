/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"

#include "sysman/sysman.h"
#include "sysman/sysman_device/os_sysman_device.h"
#include "sysman/sysman_device/sysman_device_imp.h"
#include "sysman/sysman_imp.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace L0 {
namespace ult {

template <>
struct Mock<OsSysmanDevice> : public OsSysmanDevice {

    Mock<OsSysmanDevice>() = default;

    MOCK_METHOD1(getSerialNumber, void(int8_t (&serialNumber)[ZET_STRING_PROPERTY_SIZE]));
    MOCK_METHOD1(getBoardNumber, void(int8_t (&boardNumber)[ZET_STRING_PROPERTY_SIZE]));
    MOCK_METHOD1(getBrandName, void(int8_t (&brandName)[ZET_STRING_PROPERTY_SIZE]));
    MOCK_METHOD1(getModelName, void(int8_t (&modelName)[ZET_STRING_PROPERTY_SIZE]));
    MOCK_METHOD1(getVendorName, void(int8_t (&vendorName)[ZET_STRING_PROPERTY_SIZE]));
    MOCK_METHOD1(getDriverVersion, void(int8_t (&driverVersion)[ZET_STRING_PROPERTY_SIZE]));
    MOCK_METHOD0(reset, ze_result_t());
};

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
