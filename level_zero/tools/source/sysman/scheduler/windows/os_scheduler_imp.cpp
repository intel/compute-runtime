/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/scheduler/os_scheduler.h"

namespace L0 {

class WddmSchedulerImp : public OsScheduler {};

OsScheduler *OsScheduler::create(OsSysman *pOsSysman) {
    WddmSchedulerImp *pWddmSchedulerImp = new WddmSchedulerImp();
    return static_cast<OsScheduler *>(pWddmSchedulerImp);
}

} // namespace L0
