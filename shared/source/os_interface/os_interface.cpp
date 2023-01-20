/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"

namespace NEO {

OSInterface::~OSInterface() {
    if (this->driverModel) {
        this->driverModel->cleanup();
    }
}
DriverModel *OSInterface::getDriverModel() const {
    return this->driverModel.get();
};

void OSInterface::setDriverModel(std::unique_ptr<DriverModel> driverModel) {
    this->driverModel = std::move(driverModel);
};

bool OSInterface::are64kbPagesEnabled() {
    return osEnabled64kbPages;
}

static_assert(!std::is_move_constructible_v<OSInterface>);
static_assert(!std::is_copy_constructible_v<OSInterface>);
static_assert(!std::is_move_assignable_v<OSInterface>);
static_assert(!std::is_copy_assignable_v<OSInterface>);
} // namespace NEO
