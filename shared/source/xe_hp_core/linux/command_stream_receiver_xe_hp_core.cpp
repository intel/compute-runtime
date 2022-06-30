/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/os_interface/linux/device_command_stream.inl"
#include "shared/source/os_interface/linux/drm_command_stream.inl"
#include "shared/source/os_interface/linux/drm_command_stream_xehp_and_later.inl"
#include "shared/source/xe_hp_core/hw_cmds.h"

namespace NEO {

template class DeviceCommandStreamReceiver<XeHpFamily>;
template class DrmCommandStreamReceiver<XeHpFamily>;
template class CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<XeHpFamily>>;
} // namespace NEO
