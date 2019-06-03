/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/sharing_factory.h"

class SharingFactoryMock : public NEO::SharingFactory {
  public:
    using NEO::SharingFactory::sharings;

    SharingFactoryMock() = default;
    ~SharingFactoryMock() = default;
};
