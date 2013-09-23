/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/crypto/WorkerContextCrypto.h"

#include "core/dom/ScriptExecutionContext.h"
#include "modules/crypto/WorkerCrypto.h"

namespace WebCore {

WorkerContextCrypto::WorkerContextCrypto()
{
}

WorkerContextCrypto::~WorkerContextCrypto()
{
}

const char* WorkerContextCrypto::supplementName()
{
    return "WorkerContextCrypto";
}

// static
WorkerContextCrypto* WorkerContextCrypto::from(ScriptExecutionContext* context)
{
    WorkerContextCrypto* supplement = static_cast<WorkerContextCrypto*>(Supplement<ScriptExecutionContext>::from(context, supplementName()));
    if (!supplement) {
        supplement = new WorkerContextCrypto();
        provideTo(context, supplementName(), adoptPtr(supplement));
    }
    return supplement;
}

// static
WorkerCrypto* WorkerContextCrypto::crypto(ScriptExecutionContext* context)
{
    return WorkerContextCrypto::from(context)->crypto();
}

WorkerCrypto* WorkerContextCrypto::crypto() const
{
    if (!m_crypto)
        m_crypto = WorkerCrypto::create();
    return m_crypto.get();
}

} // namespace WebCore
