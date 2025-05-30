/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PushSubscription.h"

#include "EventLoop.h"
#include "Exception.h"
#include "JSDOMPromiseDeferred.h"
#include "PushSubscriptionOptions.h"
#include "PushSubscriptionOwner.h"
#include "ScriptExecutionContext.h"
#include "ServiceWorkerContainer.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/Base64.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(PushSubscription);

PushSubscription::PushSubscription(PushSubscriptionData&& data, RefPtr<PushSubscriptionOwner>&& owner)
    : m_data(WTFMove(data))
    , m_pushSubscriptionOwner(WTFMove(owner))
{
}

PushSubscription::~PushSubscription() = default;

const PushSubscriptionData& PushSubscription::data() const
{
    return m_data;
}

const String& PushSubscription::endpoint() const
{
    return m_data.endpoint;
}

std::optional<EpochTimeStamp> PushSubscription::expirationTime() const
{
    return m_data.expirationTime;
}

PushSubscriptionOptions& PushSubscription::options() const
{
    if (!m_options) {
        auto key = m_data.serverVAPIDPublicKey;
        m_options = PushSubscriptionOptions::create(WTFMove(key));
    }

    return *m_options;
}

const Vector<uint8_t>& PushSubscription::clientECDHPublicKey() const
{
    return m_data.clientECDHPublicKey;
}

const Vector<uint8_t>& PushSubscription::sharedAuthenticationSecret() const
{
    return m_data.sharedAuthenticationSecret;
}

ExceptionOr<RefPtr<JSC::ArrayBuffer>> PushSubscription::getKey(PushEncryptionKeyName name) const
{
    auto& source = [&]() -> const Vector<uint8_t>& {
    switch (name) {
    case PushEncryptionKeyName::P256dh:
            return clientECDHPublicKey();
    case PushEncryptionKeyName::Auth:
            return sharedAuthenticationSecret();
    }
    }();

    auto buffer = ArrayBuffer::tryCreate(source);
    if (!buffer)
        return Exception { ExceptionCode::OutOfMemoryError };
    return buffer;
}

void PushSubscription::unsubscribe(ScriptExecutionContext& scriptExecutionContext, DOMPromiseDeferred<IDLBoolean>&& promise)
{
    scriptExecutionContext.eventLoop().queueTask(TaskSource::Networking, [this, protectedThis = Ref { *this }, pushSubscriptionIdentifier = m_data.identifier, promise = WTFMove(promise)]() mutable {
        if (!m_pushSubscriptionOwner) {
            promise.resolve(false);
            return;
        }

        m_pushSubscriptionOwner->unsubscribeFromPushService(pushSubscriptionIdentifier, WTFMove(promise));
    });
}

PushSubscriptionJSON PushSubscription::toJSON() const
{
    return PushSubscriptionJSON {
        endpoint(),
        expirationTime(),
        Vector<KeyValuePair<String, String>> {
            { "p256dh"_s, base64URLEncodeToString(clientECDHPublicKey()) },
            { "auth"_s, base64URLEncodeToString(sharedAuthenticationSecret()) }
        }
    };
}

} // namespace WebCore
