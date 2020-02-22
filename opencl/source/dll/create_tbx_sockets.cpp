/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/tbx/tbx_sockets_imp.h"

using namespace NEO;

namespace NEO {
TbxSockets *TbxSockets::create() {
    return new TbxSocketsImp;
}
} // namespace NEO
