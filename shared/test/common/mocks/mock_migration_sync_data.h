/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/migration_sync_data.h"

namespace NEO {

struct MockMigrationSyncDataWithYield : public MigrationSyncData {
    using MigrationSyncData::MigrationSyncData;
    void yield() const override {
        (*this->tagAddress)++;
        MigrationSyncData::yield();
    }
};
struct MockMigrationSyncData : public MigrationSyncData {
    using MigrationSyncData::latestTaskCountUsed;
    using MigrationSyncData::MigrationSyncData;
    using MigrationSyncData::tagAddress;
    void waitOnCpu() override {
        waitOnCpuCalled++;
        MigrationSyncData::waitOnCpu();
    }

    uint32_t waitOnCpuCalled = 0u;
};

} // namespace NEO
