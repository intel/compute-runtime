/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/migration_sync_data.h"

#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/constants.h"

namespace NEO {

MigrationSyncData::MigrationSyncData(size_t size) {
    hostPtr = alignedMalloc(size, MemoryConstants::pageSize);
}
MigrationSyncData::~MigrationSyncData() {
    alignedFree(hostPtr);
}

uint32_t MigrationSyncData::getCurrentLocation() const { return currentLocation; }
bool MigrationSyncData::isUsedByTheSameContext(volatile TagAddressType *tagAddress) const { return this->tagAddress == tagAddress; }

void MigrationSyncData::setCurrentLocation(uint32_t rootDeviceIndex) {
    currentLocation = rootDeviceIndex;
    migrationInProgress = false;
}

void MigrationSyncData::signalUsage(volatile TagAddressType *tagAddress, TaskCountType taskCount) {
    this->tagAddress = tagAddress;
    latestTaskCountUsed = taskCount;
}

void MigrationSyncData::waitOnCpu() {
    while (tagAddress != nullptr) {
        auto taskCount = *tagAddress;
        if (taskCount >= latestTaskCountUsed) {
            tagAddress = nullptr;
        } else {
            yield();
        }
    };
};

void MigrationSyncData::startMigration() {
    migrationInProgress = true;
}
void MigrationSyncData::yield() const {
    std::this_thread::yield();
}
} // namespace NEO
