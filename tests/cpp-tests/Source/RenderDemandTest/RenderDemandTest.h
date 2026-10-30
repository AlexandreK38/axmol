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

#include "../BaseTest.h"
#include "ui/UIRichText.h"

DEFINE_TEST_SUITE(RenderDemandTests);

class RenderDemandTestCase : public TestCase
{
public:
    void onEnter() override;
    void onExit() override;
    Type getTestType() const override { return Type::UNIT; }
    float getDuration() const override { return 10.0f; }
    std::string getExpectedOutput() const override { return "PASS"; }
    std::string getActualOutput() const override;

protected:
    void check(bool condition, std::string_view message);
    void finish();
    unsigned _tick     = 0;
    uint64_t _baseline = 0;
    ax::RenderActivity _activity;

private:
    ax::RenderMode _previousMode = ax::RenderMode::Continuous;
    float _previousInterval      = 0;
    bool _previousStats          = false;
    bool _complete               = false;
    std::string _failures;
};

class RenderDemandIdleTest : public RenderDemandTestCase
{
public:
    CREATE_FUNC(RenderDemandIdleTest);
    bool init() override;
    void update(float dt) override;
    std::string title() const override { return "Render demand: idle and animation"; }
    std::string subtitle() const override
    {
        return "Logic keeps ticking; visual changes wake rendering. Wait for PASS.";
    }

private:
    ax::DrawNode* _shape    = nullptr;
    ax::Label* _label       = nullptr;
    uint64_t _baselineTicks = 0;
    bool _callbackRan       = false;
};

class RenderDemandActivityTest : public RenderDemandTestCase
{
public:
    CREATE_FUNC(RenderDemandActivityTest);
    bool init() override;
    void update(float dt) override;
    std::string title() const override { return "Render demand: activities and callbacks"; }
    std::string subtitle() const override
    {
        return "Activities, mode changes and AFTER_DRAW mutations. Wait for PASS.";
    }

private:
    ax::ui::RichText* _text = nullptr;
    ax::Sprite* _sprite     = nullptr;
    bool _changeText        = false;
    bool _changeUniform     = false;
};

class RenderDemandOffscreenTest : public RenderDemandTestCase
{
public:
    CREATE_FUNC(RenderDemandOffscreenTest);
    bool init() override;
    void update(float dt) override;
    std::string title() const override { return "Render demand: offscreen and capture"; }
    std::string subtitle() const override
    {
        return "GPU readback and screen capture wake an idle scene. Wait for PASS.";
    }

private:
    struct CaptureResults
    {
        bool offscreen = false;
        bool screen    = false;
    };
    std::shared_ptr<CaptureResults> _captures = std::make_shared<CaptureResults>();
    ax::Sprite* _sprite                       = nullptr;
};
