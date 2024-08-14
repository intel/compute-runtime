/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"

namespace L0 {
namespace Sysman {
namespace ult {

std::string gpuUpstreamPortPathInPmt = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0";
const std::string realPathTelem1 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem1";
const std::string realPathTelem2 = "/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:02.0/0000:8e:00.1/pmt_telemetry.1.auto/intel_pmt/telem2";
const std::string sysfsPathTelem1 = "/sys/class/intel_pmt/telem1";
const std::string sysfsPathTelem2 = "/sys/class/intel_pmt/telem2";
const std::string telem1OffsetFileName("/sys/class/intel_pmt/telem1/offset");
const std::string telem1GuidFileName("/sys/class/intel_pmt/telem1/guid");
const std::string telem1TelemFileName("/sys/class/intel_pmt/telem1/telem");
const std::string telem2OffsetFileName("/sys/class/intel_pmt/telem2/offset");
const std::string telem2GuidFileName("/sys/class/intel_pmt/telem2/guid");
const std::string telem2TelemFileName("/sys/class/intel_pmt/telem2/telem");

} // namespace ult
} // namespace Sysman
} // namespace L0
