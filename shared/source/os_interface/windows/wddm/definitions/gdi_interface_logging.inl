/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void getEnterString<D3DKMT_CREATENATIVEFENCE *>(D3DKMT_CREATENATIVEFENCE *param, char *input, size_t size) {
    snprintf_s(input, size, size, "D3DKMT_CREATENATIVEFENCE Info Type %u Info Flags 0x%x", param->Info.Type, param->Info.Flags.Value);
}

template void logEnter<D3DKMT_CREATENATIVEFENCE *>(D3DKMT_CREATENATIVEFENCE *param);
template void logExit<D3DKMT_CREATENATIVEFENCE *>(NTSTATUS status, D3DKMT_CREATENATIVEFENCE *param);
