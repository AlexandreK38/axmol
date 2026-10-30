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

#include <memory>
#include "platform/PlatformDefine.h"

namespace ax
{

class RenderDemand;

/**
 * Keeps rendering active for the lifetime of a time-dependent effect.
 * Instances can be moved but not copied.
 * Reset/destroy it on completion, cancellation or owner destruction.
 */
class AX_DLL RenderActivity
{
public:
    RenderActivity() = default;
    explicit RenderActivity(std::shared_ptr<RenderDemand> demand);
    ~RenderActivity();
    RenderActivity(const RenderActivity&)            = delete;
    RenderActivity& operator=(const RenderActivity&) = delete;
    RenderActivity(RenderActivity&& other) noexcept;
    RenderActivity& operator=(RenderActivity&& other) noexcept;
    void reset();
    explicit operator bool() const;

private:
    std::shared_ptr<RenderDemand> _demand;
};

}  // namespace ax
