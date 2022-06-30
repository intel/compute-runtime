/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "opencl/source/sharings/sharing_factory.h"

class SharingFactoryMock : public NEO::SharingFactory {
  public:
    using NEO::SharingFactory::sharings;

    SharingFactoryMock() = default;
    ~SharingFactoryMock() = default;
};
