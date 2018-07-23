/*
* Copyright (c) 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "gmm_client_context.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/page_table_mngr.h"

namespace OCLRT {
GmmPageTableMngr::~GmmPageTableMngr() {
    if (clientContext) {
        clientContext->DestroyPageTblMgrObject(pageTableManager);
    }
}

GmmPageTableMngr::GmmPageTableMngr(GMM_DEVICE_CALLBACKS_INT *deviceCb, unsigned int translationTableFlags, GMM_TRANSLATIONTABLE_CALLBACKS *translationTableCb) {
    clientContext = GmmHelper::getClientContext()->getHandle();
    pageTableManager = clientContext->CreatePageTblMgrObject(deviceCb, translationTableCb, translationTableFlags);
}

} // namespace OCLRT
