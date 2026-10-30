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

#include "base/RenderDemand.h"

namespace ax
{

void RenderDemand::request(RenderReason reason)
{
    _pending.fetch_or(static_cast<uint32_t>(reason), std::memory_order_relaxed);
}

bool RenderDemand::needed(RenderMode mode) const
{
    return mode == RenderMode::Continuous || _activities.load(std::memory_order_relaxed) != 0 ||
           _pending.load(std::memory_order_relaxed) != 0;
}

uint32_t RenderDemand::consume(RenderMode mode)
{
    auto reasons = _pending.exchange(0, std::memory_order_relaxed);
    if (mode == RenderMode::Continuous)
        reasons |= static_cast<uint32_t>(RenderReason::Continuous);
    if (_activities.load(std::memory_order_relaxed))
        reasons |= static_cast<uint32_t>(RenderReason::Activity);
    return reasons;
}

void RenderDemand::retry(uint32_t reasons)
{
    _pending.fetch_or(reasons | static_cast<uint32_t>(RenderReason::Retry), std::memory_order_relaxed);
}

uint32_t RenderDemand::pending() const
{
    return _pending.load(std::memory_order_relaxed);
}

void RenderDemand::beginActivity()
{
    _activities.fetch_add(1, std::memory_order_relaxed);
    request(RenderReason::Activity);
}

void RenderDemand::endActivity()
{
    _activities.fetch_sub(1, std::memory_order_relaxed);
    // Render the final state even when the activity is cancelled.
    request(RenderReason::Activity);
}

}  // namespace ax
