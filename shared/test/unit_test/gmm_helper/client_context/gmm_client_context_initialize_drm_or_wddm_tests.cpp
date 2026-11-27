/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/client_context/map_gpu_va_gmm.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace {
uint32_t freeGpuVirtualAddressCalled = 0u;
D3DKMT_FREEGPUVIRTUALADDRESS passedFreeGpuVirtualAddress = {};
NTSTATUS __stdcall freeGpuVirtualAddressMock(const D3DKMT_FREEGPUVIRTUALADDRESS *freeGpuVirtualAddress) {
    freeGpuVirtualAddressCalled++;
    passedFreeGpuVirtualAddress = *freeGpuVirtualAddress;
    return STATUS_SUCCESS;
}
} // namespace

TEST(GmmDrmOrWddmClientContextTest, givenExplicitFreeRequiredWhenFreeingGpuVirtualAddressThenGdiFreesIt) {
    constexpr D3DKMT_HANDLE adapter = 0x1234;
    OsEnvironmentWin osEnvironment;
    osEnvironment.gdi->freeGpuVirtualAddress = freeGpuVirtualAddressMock;
    VariableBackup<uint32_t> freeGpuVirtualAddressCalledBackup(&freeGpuVirtualAddressCalled, 0u);
    VariableBackup<D3DKMT_FREEGPUVIRTUALADDRESS> passedFreeGpuVirtualAddressBackup(&passedFreeGpuVirtualAddress);
    GmmClientContext clientContext{};
    FreeGpuVirtualAddressGmm freeGpuVa{adapter, MemoryConstants::megaByte, MemoryConstants::pageSize64k, nullptr, osEnvironment.gdi.get(), true};

    EXPECT_EQ(static_cast<uint64_t>(STATUS_SUCCESS), clientContext.freeGpuVirtualAddress(&freeGpuVa));
    EXPECT_EQ(1u, freeGpuVirtualAddressCalled);
    EXPECT_EQ(adapter, passedFreeGpuVirtualAddress.hAdapter);
    EXPECT_EQ(MemoryConstants::megaByte, passedFreeGpuVirtualAddress.BaseAddress);
    EXPECT_EQ(MemoryConstants::pageSize64k, passedFreeGpuVirtualAddress.Size);
}

TEST(GmmDrmOrWddmClientContextTest, givenExplicitFreeNotRequiredWhenFreeingGpuVirtualAddressThenGdiIsNotCalledAndSuccessIsReturned) {
    OsEnvironmentWin osEnvironment;
    osEnvironment.gdi->freeGpuVirtualAddress = freeGpuVirtualAddressMock;
    VariableBackup<uint32_t> freeGpuVirtualAddressCalledBackup(&freeGpuVirtualAddressCalled, 0u);
    GmmClientContext clientContext{};
    FreeGpuVirtualAddressGmm freeGpuVa{0, MemoryConstants::megaByte, MemoryConstants::pageSize64k, nullptr, osEnvironment.gdi.get(), false};

    EXPECT_EQ(static_cast<uint64_t>(STATUS_SUCCESS), clientContext.freeGpuVirtualAddress(&freeGpuVa));
    EXPECT_EQ(0u, freeGpuVirtualAddressCalled);
}
