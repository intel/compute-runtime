/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"

#include "sysman/standby/os_standby.h"
#include "sysman/standby/standby_imp.h"
#include "sysman/sysman.h"
#include "sysman/sysman_imp.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace L0 {
namespace ult {

template <>
struct Mock<OsStandby> : public OsStandby {

    Mock<OsStandby>() = default;
    zet_standby_promo_mode_t mode;

    ze_result_t getMockMode(zet_standby_promo_mode_t &m) {
        m = mode;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t setMockMode(zet_standby_promo_mode_t m) {
        mode = m;
        return ZE_RESULT_SUCCESS;
    }

    MOCK_METHOD1(getMode, ze_result_t(zet_standby_promo_mode_t &mode));
    MOCK_METHOD1(setMode, ze_result_t(zet_standby_promo_mode_t mode));
};

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
