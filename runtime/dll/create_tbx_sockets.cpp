/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/tbx/tbx_sockets_imp.h"

using namespace OCLRT;

namespace OCLRT {
TbxSockets *TbxSockets::create() {
    return new TbxSocketsImp;
}
} // namespace OCLRT
