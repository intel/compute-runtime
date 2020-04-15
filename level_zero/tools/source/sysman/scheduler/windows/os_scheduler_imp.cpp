/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/scheduler/os_scheduler.h"

namespace L0 {

class WddmSchedulerImp : public OsScheduler {
  public:
    ze_result_t getPreemptTimeout(uint64_t &timeout) override;
    ze_result_t getTimesliceDuration(uint64_t &timeslice) override;
    ze_result_t getHeartbeatInterval(uint64_t &heartbeat) override;
    ze_result_t setPreemptTimeout(uint64_t timeout) override;
    ze_result_t setTimesliceDuration(uint64_t timeslice) override;
    ze_result_t setHeartbeatInterval(uint64_t heartbeat) override;
};

ze_result_t WddmSchedulerImp::getPreemptTimeout(uint64_t &timeout) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::getTimesliceDuration(uint64_t &timeslice) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::getHeartbeatInterval(uint64_t &heartbeat) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::setPreemptTimeout(uint64_t timeout) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::setTimesliceDuration(uint64_t timeslice) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmSchedulerImp::setHeartbeatInterval(uint64_t heartbeat) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

OsScheduler *OsScheduler::create(OsSysman *pOsSysman) {
    WddmSchedulerImp *pWddmSchedulerImp = new WddmSchedulerImp();
    return static_cast<OsScheduler *>(pWddmSchedulerImp);
}

} // namespace L0