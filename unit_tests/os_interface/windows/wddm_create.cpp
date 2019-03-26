/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_wddm.h"

namespace NEO {
Wddm *Wddm::createWddm() {
    return new WddmMock();
}
} // namespace NEO
