/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/diagnostics/sysman_diagnostics_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"

namespace L0 {
namespace Sysman {
namespace ult {

class ZesDiagnosticsFixture : public SysmanDeviceFixture {
  protected:
    void SetUp() override {
        SysmanDeviceFixture::SetUp();
    }
    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(ZesDiagnosticsFixture, GivenValidDiagnosticsHandlesWhenCallingReInitOnDiagnosticsHandleContextThenHandlesRemainValid) {
    auto pDiagnosticsImp = std::make_unique<L0::Sysman::DiagnosticsImp>(pOsSysman, "MEMORY_PPR");
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.push_back(std::move(pDiagnosticsImp));

    pSysmanDeviceImp->pDiagnosticsHandleContext->reInit();

    EXPECT_EQ(1u, static_cast<uint32_t>(pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.size()));
    for (auto &handle : pSysmanDeviceImp->pDiagnosticsHandleContext->handleList) {
        EXPECT_NE(nullptr, handle);
    }
    pSysmanDeviceImp->pDiagnosticsHandleContext->handleList.clear();
}

} // namespace ult
} // namespace Sysman
} // namespace L0
