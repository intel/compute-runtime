/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_info.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "test.h"

#include <typeinfo>

using namespace OCLRT;

TEST(wddmCreateTests, givenInputVersionWhenCreatingThenCreateRequestedObject) {
    std::unique_ptr<Wddm> wddm(Wddm::createWddm());
    EXPECT_EQ(typeid(*wddm.get()), typeid(Wddm));
}
