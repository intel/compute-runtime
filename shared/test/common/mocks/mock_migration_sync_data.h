/*
 * Copyright (C) 2021-2025 Intel Corporation
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
        auto newTag = *this->tagAddress + 1;
        *this->tagAddress = newTag;
        MigrationSyncData::yield();
    }
};
struct MockMigrationSyncData : public MigrationSyncData {
    using MigrationSyncData::latestTaskCountUsed;
    using MigrationSyncData::MigrationSyncData;
    using MigrationSyncData::tagAddress;
    void signalUsage(volatile TagAddressType *tagAddress, TaskCountType taskCount) override {
        signalUsageCalled++;
        MigrationSyncData::signalUsage(tagAddress, taskCount);
    }
    void waitOnCpu() override {
        waitOnCpuCalled++;
        MigrationSyncData::waitOnCpu();
    }

    uint32_t signalUsageCalled = 0u;
    uint32_t waitOnCpuCalled = 0u;
};

} // namespace NEO
