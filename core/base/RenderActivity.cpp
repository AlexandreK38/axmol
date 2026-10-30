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

#include "base/RenderActivity.h"
#include "base/RenderDemand.h"

#include <utility>

namespace ax
{

RenderActivity::RenderActivity(std::shared_ptr<RenderDemand> demand) : _demand(std::move(demand))
{
    _demand->beginActivity();
}

RenderActivity::~RenderActivity()
{
    reset();
}

RenderActivity::RenderActivity(RenderActivity&& other) noexcept : _demand(std::move(other._demand)) {}

RenderActivity& RenderActivity::operator=(RenderActivity&& other) noexcept
{
    if (this != &other)
    {
        reset();
        _demand = std::move(other._demand);
    }
    return *this;
}

void RenderActivity::reset()
{
    if (_demand)
    {
        _demand->endActivity();
        _demand.reset();
    }
}

RenderActivity::operator bool() const
{
    return bool(_demand);
}

}  // namespace ax
