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

#include <doctest.h>
#include "base/RenderActivity.h"
#include "base/RenderDemand.h"

#include <thread>
#include <vector>

using namespace ax;

TEST_SUITE("base/RenderDemand")
{
    TEST_CASE("requests coalesce and requests during a frame survive completion")
    {
        RenderDemand demand;
        demand.consume(RenderMode::OnDemand);
        CHECK_FALSE(demand.needed(RenderMode::OnDemand));
        demand.request(RenderReason::Node);
        demand.request(RenderReason::Node);
        auto reasons = demand.consume(RenderMode::OnDemand);
        CHECK(reasons == static_cast<uint32_t>(RenderReason::Node));
        demand.request(RenderReason::Texture);  // Arrives while the consumed frame is being built.
        CHECK(demand.pending() == static_cast<uint32_t>(RenderReason::Texture));
        CHECK(demand.needed(RenderMode::OnDemand));
    }

    TEST_CASE("failed frames and absent surfaces restore all causes")
    {
        RenderDemand demand;
        demand.request(RenderReason::Offscreen);
        auto frame = demand.consume(RenderMode::OnDemand);
        demand.request(RenderReason::Node);
        demand.retry(frame);
        CHECK((demand.pending() & frame) == frame);
        CHECK((demand.pending() & static_cast<uint32_t>(RenderReason::Node)) != 0);
        CHECK((demand.pending() & static_cast<uint32_t>(RenderReason::Retry)) != 0);
    }

    TEST_CASE("continuous mode remains unconditional across policy changes")
    {
        RenderDemand demand;
        demand.consume(RenderMode::OnDemand);
        CHECK(demand.needed(RenderMode::Continuous));
        CHECK(demand.consume(RenderMode::Continuous) == static_cast<uint32_t>(RenderReason::Continuous));
        CHECK_FALSE(demand.needed(RenderMode::OnDemand));
    }

    TEST_CASE("nested activities cancel and render the final state exactly once")
    {
        auto demand = std::make_shared<RenderDemand>();
        demand->consume(RenderMode::OnDemand);
        RenderActivity first(demand);
        {
            RenderActivity second(demand);
            demand->consume(RenderMode::OnDemand);
            first.reset();
            demand->consume(RenderMode::OnDemand);
            CHECK(demand->needed(RenderMode::OnDemand));
            RenderActivity moved(std::move(second));
            CHECK_FALSE(bool(second));
            CHECK(bool(moved));
        }
        CHECK(demand->needed(RenderMode::OnDemand));
        demand->consume(RenderMode::OnDemand);
        CHECK_FALSE(demand->needed(RenderMode::OnDemand));
        first.reset();
        CHECK_FALSE(demand->needed(RenderMode::OnDemand));
    }

    TEST_CASE("activities safely outlive their original owner")
    {
        auto demand = std::make_shared<RenderDemand>();
        RenderActivity activity(demand);
        demand.reset();
        activity.reset();
        CHECK_FALSE(bool(activity));
    }

    TEST_CASE("concurrent requests cannot be cleared by an in-flight frame")
    {
        RenderDemand demand;
        demand.consume(RenderMode::OnDemand);
        std::vector<std::thread> workers;
        for (int i = 0; i < 8; ++i)
            workers.emplace_back([&demand]() {
                for (int j = 0; j < 1000; ++j)
                    demand.request(RenderReason::Texture);
            });
        for (auto& worker : workers)
            worker.join();
        CHECK(demand.consume(RenderMode::OnDemand) == static_cast<uint32_t>(RenderReason::Texture));
        CHECK_FALSE(demand.needed(RenderMode::OnDemand));
    }
}
