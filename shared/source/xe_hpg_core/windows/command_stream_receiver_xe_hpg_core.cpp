/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/os_interface/windows/device_command_stream.inl"
#include "shared/source/os_interface/windows/wddm_device_command_stream.inl"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

namespace NEO {

template class DeviceCommandStreamReceiver<XE_HPG_COREFamily>;
template class WddmCommandStreamReceiver<XE_HPG_COREFamily>;
template class CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<XE_HPG_COREFamily>>;
} // namespace NEO
