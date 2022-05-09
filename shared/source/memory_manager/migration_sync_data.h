/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
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
    void signalUsage(volatile uint32_t *tagAddress, uint32_t taskCount);
    bool isUsedByTheSameContext(volatile uint32_t *tagAddress) const;
    MOCKABLE_VIRTUAL void waitOnCpu();
    bool isMigrationInProgress() const { return migrationInProgress; }
    void *getHostPtr() const { return hostPtr; }

  protected:
    MOCKABLE_VIRTUAL void yield() const;
    volatile uint32_t *tagAddress = nullptr;
    void *hostPtr = nullptr;
    uint32_t latestTaskCountUsed = 0u;
    uint32_t currentLocation = locationUndefined;
    bool migrationInProgress = false;
};
} // namespace NEO