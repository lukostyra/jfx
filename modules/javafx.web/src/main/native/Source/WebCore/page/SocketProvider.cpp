/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#if PLATFORM(JAVA)
#include "SocketProvider.h"
#include "WebTransportSession.h"
#include <wtf/CompletionHandler.h>
#include "SocketStreamHandleImpl.h"
#include "ThreadableWebSocketChannel.h"

namespace WebCore {

#if PLATFORM(JAVA)
Ref<SocketStreamHandle> SocketProvider::createSocketStreamHandle(const URL& url, SocketStreamHandleClient& client, WebSocketIdentifier, PAL::SessionID sessionID, Page* page, const String& credentialPartition, const StorageSessionProvider* provider)
{
    return SocketStreamHandleImpl::create(url, client, sessionID, page, credentialPartition, { }, provider);
}
#else
Ref<SocketStreamHandle> SocketProvider::createSocketStreamHandle(const URL& url, SocketStreamHandleClient& client, WebSocketIdentifier, PAL::SessionID sessionID, const String& credentialPartition, const StorageSessionProvider* provider)
{
    static const auto shouldAcceptInsecureCertificates = false;
    return SocketStreamHandleImpl::create(url, client, sessionID, credentialPartition, { }, provider, shouldAcceptInsecureCertificates);
}
#endif

RefPtr<ThreadableWebSocketChannel> SocketProvider::createWebSocketChannel(Document&, WebSocketChannelClient&)
{
    return nullptr;
}
void SocketProvider::initializeWebTransportSession(WebCore::ScriptExecutionContext&, const URL&, CompletionHandler<void(RefPtr<WebCore::WebTransportSession>&&)>&& completionHandler)
{
    ASSERT_NOT_REACHED();
    completionHandler(nullptr);
}

}
#endif
