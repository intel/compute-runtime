/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/common/ddi_table_expectations.h"
#include <level_zero/zer_api.h>

namespace L0 {
namespace ult {

// Single manifest of all L0 runtime DDI tables. Every entry declared by the L0 headers is listed
// here, in header declaration order, together with the API version that first exposes it.
//
// This is the only place that has to be updated when a zer*GetProcAddrTable() implementation
// gains, drops or re-versions an entry, or when the L0 headers add a new DDI slot. Any such change
// that is not reflected here makes the tests below fail.

namespace ZerDdiTableManifest {

namespace zerGlobal {
using Table = zer_global_dditable_t;
constexpr DdiEntryExpectation entries[] = {
    DDI_ENTRY(pfnGetLastErrorDescription, ZE_API_VERSION_1_14),
    DDI_ENTRY(pfnTranslateDeviceHandleToIdentifier, ZE_API_VERSION_1_14),
    DDI_ENTRY(pfnTranslateIdentifierToDeviceHandle, ZE_API_VERSION_1_14),
    DDI_ENTRY(pfnGetDefaultContext, ZE_API_VERSION_1_14),
};
} // namespace zerGlobal

constexpr DdiTableExpectation ddiTables[] = {
    DDI_TABLE(zerGlobal, zerGetGlobalProcAddrTable),
};

} // namespace ZerDdiTableManifest

constexpr size_t ddiTablesCount = std::size(ZerDdiTableManifest::ddiTables);
constexpr const DdiTableExpectation *ddiTables = ZerDdiTableManifest::ddiTables;

TEST(ZerDdiTableManifestTest, givenDdiTableManifestWhenComparingAgainstDdiTableLayoutThenEachSlotIsDescribedAtMostOnce) {
    givenDdiTableManifestWhenComparingAgainstDdiTableLayoutThenEachSlotIsDescribedAtMostOnceFunction(ddiTables, ddiTablesCount);
}

TEST(ZerGetProcAddrTableTest, givenApiVersionWhenGettingProcAddrTableThenOnlyEntriesExposedSinceThatVersionArePopulated) {
    givenApiVersionWhenGettingProcAddrTableThenOnlyEntriesExposedSinceThatVersionArePopulatedFunction(ddiTables, ddiTablesCount);
}

TEST(ZerGetProcAddrTableTest, givenUnsupportedMajorVersionWhenGettingProcAddrTableThenUnsupportedVersionIsReturnedAndNoEntryIsPopulated) {
    givenUnsupportedMajorVersionWhenGettingProcAddrTableThenUnsupportedVersionIsReturnedAndNoEntryIsPopulatedFunction(ddiTables, ddiTablesCount);
}

TEST(ZerGetProcAddrTableTest, givenNullDdiTableWhenGettingProcAddrTableThenInvalidArgumentIsReturned) {
    givenNullDdiTableWhenGettingProcAddrTableThenInvalidArgumentIsReturnedFunction(ddiTables, ddiTablesCount);
}

} // namespace ult
} // namespace L0
