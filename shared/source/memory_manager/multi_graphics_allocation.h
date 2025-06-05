/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/utilities/stackvec.h"

#include <functional>

namespace NEO {

class MigrationSyncData;
class GraphicsAllocation;

class MultiGraphicsAllocation {
  public:
    MultiGraphicsAllocation(uint32_t maxRootDeviceIndex);
    MultiGraphicsAllocation(const MultiGraphicsAllocation &multiGraphicsAllocation);
    MultiGraphicsAllocation(MultiGraphicsAllocation &&) noexcept;
    MultiGraphicsAllocation &operator=(const MultiGraphicsAllocation &) = delete;
    MultiGraphicsAllocation &operator=(MultiGraphicsAllocation &&) = delete;
    ~MultiGraphicsAllocation();

    GraphicsAllocation *getDefaultGraphicsAllocation() const;

    void addAllocation(GraphicsAllocation *graphicsAllocation);

    void removeAllocation(uint32_t rootDeviceIndex);

    GraphicsAllocation *getGraphicsAllocation(uint32_t rootDeviceIndex) const;

    AllocationType getAllocationType() const;

    bool isCoherent() const;

    StackVec<GraphicsAllocation *, 1> const &getGraphicsAllocations() const;

    bool requiresMigrations() const;
    MigrationSyncData *getMigrationSyncData() const { return migrationSyncData; }
    void setMultiStorage(bool value);

    static std::function<MigrationSyncData *(size_t size)> createMigrationSyncDataFunc;

  protected:
    bool isMultiStorage = false;
    MigrationSyncData *migrationSyncData = nullptr;
    StackVec<GraphicsAllocation *, 1> graphicsAllocations;
};

} // namespace NEO
