/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/page_table_mngr.h"
#include "runtime/gmm_helper/resource_info.h"

#include "gmm_client_context.h"

namespace NEO {
GmmPageTableMngr::~GmmPageTableMngr() {
    if (clientContext) {
        clientContext->DestroyPageTblMgrObject(pageTableManager);
    }
}

bool GmmPageTableMngr::updateAuxTable(uint64_t gpuVa, Gmm *gmm, bool map) {
    GMM_DDI_UPDATEAUXTABLE ddiUpdateAuxTable = {};
    ddiUpdateAuxTable.BaseGpuVA = gpuVa;
    ddiUpdateAuxTable.BaseResInfo = gmm->gmmResourceInfo->peekHandle();
    ddiUpdateAuxTable.DoNotWait = true;
    ddiUpdateAuxTable.Map = map ? 1u : 0u;

    return updateAuxTable(&ddiUpdateAuxTable) == GMM_STATUS::GMM_SUCCESS;
}

void GmmPageTableMngr::initPageTableManagerRegisters() {
    if (!initialized) {
        initContextAuxTableRegister(csrHandle, GMM_ENGINE_TYPE::ENGINE_TYPE_RCS);

        initialized = true;
    }
}

void GmmPageTableMngr::setCsrHandle(void *csrHandleIn) {
    csrHandle = csrHandleIn;
    gmmSetCsrHandleFunc(pageTableManager, csrHandle);
}

} // namespace NEO
