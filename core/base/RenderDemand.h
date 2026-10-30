/****************************************************************************
 Copyright (c) 2019-present Axmol Engine contributors (see AUTHORS.md).

 https://axmol.dev/

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#pragma once

#include <atomic>
#include <cstdint>
#include "platform/PlatformDefine.h"

namespace ax
{
/** Logic scheduling is independent of this policy. */
enum class RenderMode
{
    Continuous,
    OnDemand,
};

/** Coalesced diagnostic causes; callers may combine them with bitwise OR. */
enum class RenderReason : uint32_t
{
    Explicit   = 1u << 0,
    Node       = 1u << 1,
    Geometry   = 1u << 2,
    Texture    = 1u << 3,
    Scene      = 1u << 4,
    Surface    = 1u << 5,
    Resource   = 1u << 6,
    Offscreen  = 1u << 7,
    Activity   = 1u << 8,
    Retry      = 1u << 9,
    Continuous = 1u << 10,
};

constexpr RenderReason operator|(RenderReason lhs, RenderReason rhs)
{
    return static_cast<RenderReason>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

/**
 * Tracks pending render requests and active rendering scopes.
 * The shared state can outlive Director. Only request() and the activity count are accessed from other threads.
 */
class AX_DLL RenderDemand
{
public:
    void request(RenderReason reason = RenderReason::Explicit);
    bool needed(RenderMode mode) const;
    uint32_t consume(RenderMode mode);
    void retry(uint32_t reasons);
    uint32_t pending() const;
    void beginActivity();
    void endActivity();

private:
    std::atomic<uint32_t> _pending{static_cast<uint32_t>(RenderReason::Scene)};
    std::atomic<uint32_t> _activities{0};
};

/**
 * Collects rendering statistics on the engine thread.
 * Presentations count successful API handoffs, not physical display scanouts.
 */
struct RenderDiagnostics
{
    uint64_t ticks               = 0;
    uint64_t frames              = 0;
    uint64_t presentations       = 0;
    uint64_t failedPresentations = 0;
    uint32_t lastReasons         = 0;
};
}  // namespace ax
