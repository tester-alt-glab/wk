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

#pragma once

#include <WebCore/RemoteSnapshotIdentifier.h>
#include <wtf/Function.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

// A page rendering recorded outside this process, because it may include frames whose content this
// process must not see. The pixels never come back here: a layer displays the rendering by naming
// its identifier, for as long as this object is alive.
class RemoteSnapshotRecording : public RefCountedAndCanMakeWeakPtr<RemoteSnapshotRecording> {
public:
    virtual ~RemoteSnapshotRecording() = default;

    RemoteSnapshotIdentifier identifier() const { return m_identifier; }

    // Kept here rather than reported to the caller, because a cross-document view transition
    // captures in the old document and waits in the new one.
    bool isCompleted() const { return m_succeeded.has_value(); }
    // Only then may the identifier be given to a layer.
    bool succeeded() const { return m_succeeded.value_or(false); }

    // Not called if the recording is destroyed first.
    void whenCompleted(Function<void()>&& observer)
    {
        if (isCompleted()) {
            observer();
            return;
        }
        m_completionObservers.append(WTF::move(observer));
    }

protected:
    explicit RemoteSnapshotRecording(RemoteSnapshotIdentifier identifier)
        : m_identifier(identifier)
    {
    }

    void didComplete(bool success)
    {
        ASSERT(!isCompleted());
        m_succeeded = success;
        for (auto& observer : std::exchange(m_completionObservers, { }))
            observer();
    }

private:
    const RemoteSnapshotIdentifier m_identifier;
    std::optional<bool> m_succeeded;
    Vector<Function<void()>> m_completionObservers;
};

} // namespace WebCore
