/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace L0 {

struct CmdListSignalEventParameters {
    bool relaxedOrderingDispatch = false;
    bool apiRequestForGraphExternal = false;
};

} // namespace L0
