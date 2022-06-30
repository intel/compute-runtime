/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/os_interface/windows/device_command_stream.inl"
#include "shared/source/os_interface/windows/wddm_device_command_stream.inl"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

namespace NEO {

template class DeviceCommandStreamReceiver<XE_HPC_COREFamily>;
template class WddmCommandStreamReceiver<XE_HPC_COREFamily>;
template class CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<XE_HPC_COREFamily>>;
} // namespace NEO
