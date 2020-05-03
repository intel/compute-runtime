/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"

#include "sysman/engine/engine_imp.h"
#include "sysman/engine/os_engine.h"
#include "sysman/sysman_imp.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace L0 {
namespace ult {

template <>
struct Mock<OsEngine> : public OsEngine {

    Mock<OsEngine>() = default;

    MOCK_METHOD1(getActiveTime, ze_result_t(uint64_t &activeTime));
    MOCK_METHOD1(getEngineGroup, ze_result_t(zet_engine_group_t &engineGroup));
};

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
