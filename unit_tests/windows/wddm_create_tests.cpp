/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_info.h"
#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "test.h"

#include <typeinfo>

using namespace NEO;

TEST(wddmCreateTests, givenInputVersionWhenCreatingThenCreateRequestedObject) {
    std::unique_ptr<Wddm> wddm(Wddm::createWddm());
    EXPECT_EQ(typeid(*wddm.get()), typeid(Wddm));
}
