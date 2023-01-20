/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address_xehp_and_later.inl"
#include "shared/source/xe_hp_core/hw_cmds_base.h"

namespace NEO {

template <>
void StateBaseAddressHelper<XeHpFamily>::appendExtraCacheSettings(StateBaseAddressHelperArgs<XeHpFamily> &args) {
}

template struct StateBaseAddressHelper<XeHpFamily>;
} // namespace NEO
