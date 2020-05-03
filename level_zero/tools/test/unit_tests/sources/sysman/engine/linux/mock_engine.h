/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/test/unit_tests/mock.h"
#include "level_zero/tools/source/sysman/engine/linux/os_engine_imp.h"

#include "sysman/engine/engine_imp.h"
#include "sysman/engine/os_engine.h"
#include "sysman/linux/fs_access.h"
#include "sysman/sysman_imp.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

using ::testing::_;

namespace L0 {
namespace ult {

template <>
struct Mock<OsEngine> : public OsEngine {

    Mock<OsEngine>() = default;

    MOCK_METHOD1(getActiveTime, ze_result_t(uint64_t &activeTime));
    MOCK_METHOD1(getEngineGroup, ze_result_t(zet_engine_group_t &engineGroup));
};

template <>
struct Mock<SysfsAccess> : public SysfsAccess {
    MOCK_METHOD2(read, ze_result_t(const std::string file, std::string &val));

    ze_result_t doRead(const std::string file, std::string &val) {
        val = "rcs0";
        return ZE_RESULT_SUCCESS;
    }

    Mock<SysfsAccess>() = default;
};

template <>
struct WhiteBox<::L0::LinuxEngineImp> : public ::L0::LinuxEngineImp {
    using BaseClass = ::L0::LinuxEngineImp;
    using BaseClass::pSysfsAccess;
};

} // namespace ult
} // namespace L0

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
