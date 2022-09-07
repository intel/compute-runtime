/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address.h"

namespace NEO {

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendStateBaseAddressParameters(
    StateBaseAddressHelperArgs<GfxFamily> &args,
    bool overrideBindlessSurfaceStateBase) {
}

template <typename GfxFamily>
uint32_t StateBaseAddressHelper<GfxFamily>::getMaxBindlessSurfaceStates() {
    return 0;
}

template <>
void StateBaseAddressHelper<Gen8Family>::programStateBaseAddress(StateBaseAddressHelperArgs<Gen8Family> &args);

} // namespace NEO
