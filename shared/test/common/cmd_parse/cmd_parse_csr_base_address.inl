/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// clang-format off
using namespace NEO;
using GPGPU_CSR_BASE_ADDRESS          = GenStruct::GPGPU_CSR_BASE_ADDRESS;
// clang-format on

template <>
GPGPU_CSR_BASE_ADDRESS *genCmdCast<GPGPU_CSR_BASE_ADDRESS *>(void *buffer) {
    auto pCmd = reinterpret_cast<GPGPU_CSR_BASE_ADDRESS *>(buffer);

    return GPGPU_CSR_BASE_ADDRESS::COMMAND_TYPE_GFXPIPE == pCmd->TheStructure.Common.CommandType &&
                   GPGPU_CSR_BASE_ADDRESS::COMMAND_SUBTYPE_GFXPIPE_COMMON == pCmd->TheStructure.Common.CommandSubtype &&
                   GPGPU_CSR_BASE_ADDRESS::_3D_COMMAND_OPCODE_GFXPIPE_NONPIPELINED == pCmd->TheStructure.Common._3DCommandOpcode &&
                   GPGPU_CSR_BASE_ADDRESS::_3D_COMMAND_SUB_OPCODE_GPGPU_CSR_BASE_ADDRESS == pCmd->TheStructure.Common._3DCommandSubOpcode
               ? pCmd
               : nullptr;
}
