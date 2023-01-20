/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/task_count_helper.h"
#include "shared/source/utilities/reference_tracked_object.h"

#include <cstdint>
#include <limits>

namespace NEO {
class MigrationSyncData : public ReferenceTrackedObject<MigrationSyncData> {
  public:
    static constexpr uint32_t locationUndefined = std::numeric_limits<uint32_t>::max();

    MigrationSyncData(size_t size);
    ~MigrationSyncData() override;
    uint32_t getCurrentLocation() const;
    void startMigration();
    void setCurrentLocation(uint32_t rootDeviceIndex);
    MOCKABLE_VIRTUAL void signalUsage(volatile TagAddressType *tagAddress, TaskCountType taskCount);
    bool isUsedByTheSameContext(volatile TagAddressType *tagAddress) const;
    MOCKABLE_VIRTUAL void waitOnCpu();
    bool isMigrationInProgress() const { return migrationInProgress; }
    void *getHostPtr() const { return hostPtr; }

  protected:
    MOCKABLE_VIRTUAL void yield() const;
    volatile TagAddressType *tagAddress = nullptr;
    void *hostPtr = nullptr;
    TaskCountType latestTaskCountUsed = 0u;
    uint32_t currentLocation = locationUndefined;
    bool migrationInProgress = false;
};
} // namespace NEO