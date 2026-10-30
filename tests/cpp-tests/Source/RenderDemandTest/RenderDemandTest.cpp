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

#include "RenderDemandTest.h"

#if AX_ENABLE_EXT_IMGUI
#    include "Inspector/Inspector.h"
#endif

using namespace ax;

RenderDemandTests::RenderDemandTests()
{
    ADD_TEST_CASE(RenderDemandIdleTest);
    ADD_TEST_CASE(RenderDemandActivityTest);
    ADD_TEST_CASE(RenderDemandOffscreenTest);
}

void RenderDemandTestCase::onEnter()
{
    _previousMode     = _director->getRenderMode();
    _previousInterval = _director->getAnimationInterval();
    _previousStats    = _director->isStatsDisplay();
    TestCase::onEnter();
#if AX_ENABLE_EXT_IMGUI
    // The inspector owns a render activity, so it would prevent the idle checks.
    extension::Inspector::getInstance()->close();
#endif
    _director->setStatsDisplay(false);
    // Keep the tick-based animation checks independent of the monitor refresh rate.
    _director->setAnimationInterval(1.0f / 60);
    _director->setRenderMode(RenderMode::OnDemand);
    scheduleUpdate();
}

void RenderDemandTestCase::onExit()
{
    unscheduleUpdate();
    _activity.reset();
    TestCase::onExit();
    _director->setRenderMode(_previousMode);
    _director->setAnimationInterval(_previousInterval);
    _director->setStatsDisplay(_previousStats);
}

std::string RenderDemandTestCase::getActualOutput() const
{
    if (!_failures.empty())
        return _failures;
    return _complete ? "PASS" : "INCOMPLETE";
}

void RenderDemandTestCase::check(bool condition, std::string_view message)
{
    AXLOGI("[RenderDemand] {}: {}", condition ? "PASS" : "FAIL", message);
    if (!condition)
    {
        if (!_failures.empty())
            _failures += "; ";
        _failures += message;
    }
}

void RenderDemandTestCase::finish()
{
    _complete = true;
    unscheduleUpdate();
    // Update the visible result only after the idle measurements are complete.
    setSubtitleLabel(getActualOutput());
    AXLOGI("[RenderDemand] {}: {}", title(), getActualOutput());
}

bool RenderDemandIdleTest::init()
{
    if (!TestCase::init())
        return false;
    _shape = DrawNode::create();
    _shape->drawSolidRect(Vec2(-25, -25), Vec2(25, 25), Color4F::GREEN);
    _shape->setPosition(VisibleRect::center());
    addChild(_shape);
    _label = Label::createWithTTF("Waiting for a visual change", "fonts/arial.ttf", 18);
    _label->setPosition(VisibleRect::center() + Vec2(0, 60));
    addChild(_label);
    return true;
}

void RenderDemandIdleTest::update(float)
{
    const auto stats = _director->getRenderDiagnostics();
    switch (++_tick)
    {
    case 20:
        _baseline      = stats.frames;
        _baselineTicks = stats.ticks;
        // This callback runs on the next scheduler tick, even if nothing is rendered.
        scheduleOnce([this](float) { _callbackRan = true; }, 0, "idle callback");
        break;
    case 40:
        check(stats.frames == _baseline && stats.ticks >= _baselineTicks + 20 && _callbackRan,
              "idle scene keeps logical ticks and scheduler callbacks without new frames");
        _shape->setPosition(_shape->getPosition());
        _shape->setColor(_shape->getColor());
        _label->setString(_label->getString());
        break;
    case 60:
        check(stats.frames == _baseline, "identical assignments do not request frames");
        _shape->setPositionX(_shape->getPositionX() + 20);
        _label->setString("Visual changes wake rendering");
        break;
    case 75:
        check(stats.frames > _baseline, "transform and lazy text changes wake rendering");
        _baseline = stats.frames;
        _shape->runAction(MoveBy::create(0.1f, Vec2(40, 0)));
        break;
    case 110:
        check(stats.frames > _baseline + 1, "animation produces successive images");
        check(_shape->getPositionX() == VisibleRect::center().x + 60, "animation reaches its final position");
        _baseline = stats.frames;
        break;
    case 130:
        check(stats.frames == _baseline, "animation returns to idle after its final image");
        for (int i = 0; i < 25; ++i)
            _director->requestRender();
        break;
    case 145:
        check(stats.frames == _baseline + 1, "explicit requests coalesce into one frame");
        finish();
        break;
    }
}

bool RenderDemandActivityTest::init()
{
    if (!TestCase::init())
        return false;
    _text = ui::RichText::createWithXML("Alignment changes after drawing",
                                        {{ui::RichText::KEY_FONT_FACE, Value("fonts/arial.ttf")}});
    _text->ignoreContentAdaptWithSize(false);
    _text->setContentSize(Vec2(300, 80));
    _text->setPosition(VisibleRect::center());
    addChild(_text);
    _sprite = Sprite::create("Images/grossini.png");
    _sprite->setPosition(VisibleRect::center() + Vec2(-150, 0));
    addChild(_sprite);
    auto listener = EventListenerCustom::create(Director::EVENT_AFTER_DRAW, [this](EventCustom*) {
        if (_changeText)
        {
            _changeText = false;
            _text->setHorizontalAlignment(ui::RichText::HorizontalAlignment::RIGHT);
        }
        if (_changeUniform)
        {
            _changeUniform = false;
            auto state     = _sprite->getProgramState();
            Mat4 matrix;
            Mat4::createTranslation(123, 456, 0, &matrix);
            state->setUniform(state->getUniformLocation(backend::Uniform::MVP_MATRIX), matrix.m, sizeof(matrix.m));
        }
    });
    _eventDispatcher->addEventListenerWithSceneGraphPriority(listener, this);
    return true;
}

void RenderDemandActivityTest::update(float)
{
    const auto frames = _director->getRenderDiagnostics().frames;
    switch (++_tick)
    {
    case 20:
        _baseline = frames;
        _activity = _director->acquireRenderActivity();
        break;
    case 30:
        check(frames >= _baseline + 10, "a scoped activity keeps rendering");
        _activity.reset();
        break;
    case 45:
        _baseline = frames;
        break;
    case 60:
        check(frames == _baseline, "releasing the activity returns to idle");
        _director->setRenderMode(RenderMode::Continuous);
        break;
    case 75:
        check(frames >= _baseline + 15, "Continuous mode renders each tick");
        _director->setRenderMode(RenderMode::OnDemand);
        break;
    case 90:
        _baseline = frames;
        break;
    case 110:
        check(frames == _baseline, "switching back to OnDemand returns to idle");
        _changeText = true;
        _director->requestRender();
        break;
    case 125:
        check(frames >= _baseline + 2, "rich text changed in AFTER_DRAW requests another image");
        _baseline = frames;
        break;
    case 140:
        check(frames == _baseline, "rich text settles at rest");
        _changeUniform = true;
        _director->requestRender();
        break;
    case 155:
        check(frames >= _baseline + 2, "uniform changed in AFTER_DRAW requests another image");
        _baseline = frames;
        break;
    case 170:
        check(frames == _baseline, "uniform mutation settles at rest");
        finish();
        break;
    }
}

bool RenderDemandOffscreenTest::init()
{
    if (!TestCase::init())
        return false;
    _sprite = Sprite::create("Images/grossini.png");
    _sprite->setPosition(VisibleRect::center());
    addChild(_sprite);
    return true;
}

void RenderDemandOffscreenTest::update(float)
{
    const auto frames = _director->getRenderDiagnostics().frames;
    switch (++_tick)
    {
    case 20:
    {
        const auto size = _sprite->getContentSize();
        _sprite->setAutoSize(false);
        _sprite->setContentSize(Vec2::ZERO);
        _sprite->setTextureRect(_sprite->getTextureRect(), false, size);
        check(_sprite->getContentSize() == size, "sprite rectangle restores a zero content size");
        auto state          = _sprite->getProgramState();
        const auto location = state->getUniformLocation(backend::Uniform::TEXTURE);
        auto texture        = _sprite->getTexture()->getBackendTexture();
        state->setTexture(location, 0, 1, texture);
        state->setTextureArray(location, {0}, {texture});
        bool defaultIndex = true;
        for (const auto& entry : state->getVertexTextureInfos())
            for (auto index : entry.second.indexs)
                defaultIndex &= index == 0;
        for (const auto& entry : state->getFragmentTextureInfos())
            for (auto index : entry.second.indexs)
                defaultIndex &= index == 0;
        check(defaultIndex, "texture arrays reset the previous texture plane index");
        // Keep the test usable even when this regression fails.
        state->setTexture(location, 0, texture);
        break;
    }
    case 40:
        _baseline = frames;
        break;
    case 60:
    {
        check(frames == _baseline, "scene is idle before offscreen rendering");
        RefPtr<RenderTexture> target = RenderTexture::create(32, 32);
        target->beginWithClear(0, 1, 0, 1);
        target->end();
        auto captures   = _captures;
        const auto size = target->getSprite()->getTexture()->getContentSizeInPixels();
        _director->getRenderer()->addCallbackCommand([target, captures, size]() {
            target->newImage([captures, size](RefPtr<Image> image) {
                captures->offscreen = image && image->getWidth() == size.width && image->getHeight() == size.height &&
                                      image->getData()[0] == 0 && image->getData()[1] == 255 &&
                                      image->getData()[2] == 0 && image->getData()[3] == 255;
            });
        });
        break;
    }
    case 80:
        check(_captures->offscreen, "offscreen rendering and pixel readback complete from idle");
        _baseline = frames;
        break;
    case 100:
    {
        check(frames == _baseline, "offscreen work returns to idle");
        auto captures = _captures;
        utils::captureScreen([captures](RefPtr<Image> image) { captures->screen = bool(image); });
        break;
    }
    case 120:
        check(_captures->screen && frames > _baseline, "screen capture wakes rendering from idle");
        finish();
        break;
    }
}
