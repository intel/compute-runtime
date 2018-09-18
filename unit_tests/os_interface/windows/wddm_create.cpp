/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/mocks/mock_wddm_interface23.h"

namespace OCLRT {
Wddm *Wddm::createWddm() {
    return new WddmMock();
}
} // namespace OCLRT
