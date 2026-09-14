/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "WebRemoteSnapshotRecording.h"

#if ENABLE(GPU_PROCESS) && HAVE(IOSURFACE)

#include "WebPage.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <wtf/CallbackAggregator.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebRemoteSnapshotRecording);

RefPtr<WebRemoteSnapshotRecording> WebRemoteSnapshotRecording::create(WebPage& page, FrameIdentifier rootFrameIdentifier, const FloatSize& size, float scale, const ColorSpace& colorSpace, NOESCAPE const Function<void(GraphicsContext&)>& paint)
{
    if (size.isEmpty())
        return nullptr;

    auto identifier = generateRemoteSnapshotIdentifier();
    // Created first, so that the snapshot is released if the root cannot be recorded.
    Ref recording = adoptRef(*new WebRemoteSnapshotRecording(identifier, page.identifier()));

    // A root that fails to record fails the snapshot in the GPU process, which fails the sink below.
    auto rootRecorded = MainRunLoopSuccessCallbackAggregator::create(WebPage::failRemoteSnapshotIfRootFails(identifier, [](bool) { }));
    if (!page.recordRemoteSnapshot(identifier, rootFrameIdentifier, WebPage::RemoteSnapshotRole::Root, RenderingMode::DisplayList, FloatRect { { }, size }, size, WTF::move(rootRecorded), paint))
        return nullptr;

    // Not waiting for the root: the GPU process waits for it, and for the frames hosted elsewhere,
    // before drawing. Holding the recording until the reply keeps the release behind it, on the same
    // connection.
    RefPtr connection = WebProcess::singleton().parentProcessConnection();
    if (!connection)
        return nullptr;
    connection->sendWithAsyncReply(Messages::WebProcessProxy::CompleteSnapshotForLayerContents(page.identifier(), identifier, rootFrameIdentifier, scale, colorSpace), [recording](bool success) {
        recording->didComplete(success);
    });

    return recording;
}

WebRemoteSnapshotRecording::WebRemoteSnapshotRecording(RemoteSnapshotIdentifier identifier, PageIdentifier pageIdentifier)
    : RemoteSnapshotRecording(identifier)
    , m_pageIdentifier(pageIdentifier)
{
}

// Releases the snapshot along with the recording. The root creates the snapshot before painting,
// so the release cannot overtake that, and a frame hosted elsewhere that records afterwards
// records into nothing.
WebRemoteSnapshotRecording::~WebRemoteSnapshotRecording()
{
    // Not through the WebPage, which may be gone while the UI process still holds the rendering.
    if (RefPtr connection = WebProcess::singleton().parentProcessConnection())
        connection->send(Messages::WebProcessProxy::ReleaseSnapshotForLayerContents(m_pageIdentifier, identifier()), 0);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && HAVE(IOSURFACE)
