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

#include "axmol.h"
#include "renderer/backend/Texture.h"
#include "ui/UIRichText.h"

#include <cstdio>
#include <atomic>

#if AX_TARGET_PLATFORM == AX_PLATFORM_ANDROID
#    include <android/log.h>
#    include <jni.h>
#elif AX_TARGET_PLATFORM == AX_PLATFORM_IOS
#    import <UIKit/UIKit.h>
#elif AX_TARGET_PLATFORM == AX_PLATFORM_WASM
#    include <emscripten/emscripten.h>
#endif

#if AX_TARGET_PLATFORM == AX_PLATFORM_ANDROID || AX_TARGET_PLATFORM == AX_PLATFORM_IOS || \
    AX_TARGET_PLATFORM == AX_PLATFORM_WASM
#    define AX_RENDER_DEMAND_EXTERNAL_LOOP 1
#else
#    define AX_RENDER_DEMAND_EXTERNAL_LOOP 0
#endif

using namespace ax;

namespace
{
int failures              = 0;
unsigned scenarioPass     = 0;
bool suiteComplete        = false;
bool resumed              = false;
uint64_t backgroundFrames = 0;
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
unsigned backgroundEvents = 0;
unsigned foregroundEvents = 0;
#endif

void report(const char* message)
{
    std::printf("%s\n", message);
    std::fflush(stdout);
#if AX_TARGET_PLATFORM == AX_PLATFORM_ANDROID
    __android_log_write(ANDROID_LOG_INFO, "RenderDemand", message);
#endif
}

bool containsYellowText(Image* image)
{
    if (!image)
        return false;
    const auto data       = image->getData();
    const auto size       = image->getDataLen();
    unsigned yellowPixels = 0;
    for (ssize_t i = 0; i + 3 < size; i += 4)
    {
        if (data[i] > 20 && data[i + 1] > 20 && data[i + 2] < 20)
            ++yellowPixels;
    }
    return yellowPixels > 10;
}

class PoolProbe : public Object
{
public:
    explicit PoolProbe(bool& released) : _released(released) {}
    ~PoolProbe() override { _released = true; }

private:
    bool& _released;
};

#if !AX_RENDER_DEMAND_EXTERNAL_LOOP
// Uses the real backend with controllable surface and presentation failures.
class ProbeView : public RenderViewImpl
{
public:
    bool surfaceAvailable = true;
    bool failNextPresent  = false;
    bool initialize() { return initWithRect("Axmol render demand", ax::Rect(0, 0, 480, 320), 1.0f, true); }
    bool isGfxContextReady() override { return surfaceAvailable && RenderViewImpl::isGfxContextReady(); }
    bool present() override
    {
        if (failNextPresent)
        {
            failNextPresent = false;
            return false;
        }
        return RenderViewImpl::present();
    }
};

#endif

class ProbeScene : public Scene
{
public:
    CREATE_FUNC(ProbeScene);
#if AX_RENDER_DEMAND_EXTERNAL_LOOP
    RenderView* view = nullptr;
#else
    ProbeView* view = nullptr;
#endif
    bool compareContinuous = false;

    bool init() override
    {
        if (!Scene::init())
            return false;
        auto director = Director::getInstance();
        director->setStatsDisplay(false);
        _shape = DrawNode::create();
        _shape->drawSolidRect(Vec2(20, 20), Vec2(100, 80), Color4F::GREEN);
        addChild(_shape);
        _label = Label::createWithSystemFont("Render on demand", "Arial", 22);
        _label->setPosition(240, 220);
        addChild(_label);
        const uint8_t pixels[] = {255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255};
        _texture               = new Texture2D();
        _texture->initWithData(pixels, sizeof(pixels), backend::PixelFormat::RGBA8, 2, 2);
        _sprite = Sprite::createWithTexture(_texture);
        _sprite->setScale(32);
        _sprite->setPosition(360, 100);
        addChild(_sprite);
        auto stencil = Sprite::createWithTexture(_texture);
        stencil->setScale(20);
        _clip = ClippingNode::create(stencil);
        _clip->setAlphaThreshold(0.5f);
        _clip->setPosition(150, 100);
        auto clippedShape = DrawNode::create();
        clippedShape->drawSolidRect(Vec2(-30, -30), Vec2(30, 30), Color4F::BLUE);
        _clip->addChild(clippedShape);
        addChild(_clip);
        auto updates =
            EventListenerCustom::create(Director::EVENT_AFTER_UPDATE, [this](EventCustom*) { ++_updateEvents; });
        director->getEventDispatcher()->addEventListenerWithSceneGraphPriority(updates, this);
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
        director->getEventDispatcher()->addEventListenerWithSceneGraphPriority(
            EventListenerCustom::create(EVENT_COME_TO_BACKGROUND, [](EventCustom*) { ++backgroundEvents; }), this);
        director->getEventDispatcher()->addEventListenerWithSceneGraphPriority(
            EventListenerCustom::create(EVENT_COME_TO_FOREGROUND, [](EventCustom*) { ++foregroundEvents; }), this);
#endif
        scheduleUpdate();
        return true;
    }

    ~ProbeScene() override { AX_SAFE_RELEASE(_texture); }

    void update(float) override
    {
        auto director    = Director::getInstance();
        const auto stats = director->getRenderDiagnostics();
        if (suiteComplete)
        {
            if (resumed)
            {
                resumed     = false;
                _resumeTick = 0;
            }
            if (_resumeTick >= 0)
            {
                ++_resumeTick;
                if (_resumeTick == 5)
                    check(stats.frames > backgroundFrames, "real foreground transition redraws");
                if (_resumeTick == 10)
                    utils::captureScreen(
                        [this](RefPtr<Image> image) { _resumeTextVisible = containsYellowText(image.get()); });
                if (_resumeTick == 20)
                {
                    check(_resumeTextVisible, "text pixels survive the real foreground transition");
                    _baseline = stats.frames;
                }
                if (_resumeTick == 40)
                {
                    check(stats.frames == _baseline, "real foreground transition settles at rest");
                    report("LIFECYCLE COMPLETE");
                    _resumeTick = -1;
                }
            }
            return;
        }
        ++_tick;
        if (_tick == 1)
            director->setRenderMode(compareContinuous ? RenderMode::Continuous : RenderMode::OnDemand);
        switch (_tick)
        {
        case 15:
            _baseline = stats.frames;
            break;
        case 20:
            director->getScheduler()->runOnAxmolThread([this]() { _queuedCallback = true; });
            director->queueOperation([](void* flag) { *static_cast<std::atomic_bool*>(flag) = true; },
                                     &_queuedOperation);
            (new PoolProbe(_temporaryReleased))->autorelease();
            break;
        case 30:
            check(_queuedCallback && _temporaryReleased && _updateEvents >= 27,
                  "scheduler callbacks, update events and autorelease progress without rendering");
#if AX_TARGET_PLATFORM == AX_PLATFORM_WASM
            report("SKIP: queueOperation is not implemented by the existing WASM view");
#else
            check(_queuedOperation, "queued platform operations progress without rendering");
#endif
            check(compareContinuous ? stats.frames > _baseline : stats.frames == _baseline,
                  "idle scheduling and rendering policy");
            _shape->setPosition(_shape->getPosition());
            _shape->setColor(_shape->getColor());
            _sprite->setPosition(_sprite->getPosition());
            _sprite->setContentSize(_sprite->getContentSize());
            _label->setString(_label->getString());
            _baseline = stats.frames;
            break;
        case 40:
            check(compareContinuous || stats.frames == _baseline, "identical assignments remain idle");
            _shape->setPosition(25, 0);
            break;
        case 45:
            check(stats.frames > _baseline, "transform mutation after idle");
            _baseline = stats.frames;
            _label->setString("Logic keeps ticking");
            break;
        case 50:
            check(stats.frames > _baseline, "lazy text realization wakes rendering");
            _baseline = stats.frames;
            _shape->setVisible(false);
            break;
        case 55:
            check(stats.frames > _baseline, "hiding a node wakes rendering");
            _shape->setVisible(true);
            _shape->runAction(MoveBy::create(0.1f, Vec2(20, 0)));
            break;
        case 75:
            check(_shape->getPositionX() == 45, "animation final state");
            _baseline = stats.frames;
            break;
        case 85:
            check(compareContinuous || stats.frames == _baseline, "animation settles after final frame");
            {
                uint8_t pixels[] = {0, 0, 255, 255};
                static_cast<backend::Texture2DBackend*>(_texture->getBackendTexture())
                    ->updateSubData(0, 0, 1, 1, 0, pixels);
            }
            break;
        case 90:
        {
            check(stats.frames > _baseline, "in-place backend texture upload wakes rendering");
            _baseline          = stats.frames;
            _requestDuringDraw = true;
            auto afterDraw     = EventListenerCustom::create(Director::EVENT_AFTER_DRAW, [this](EventCustom*) {
                if (_requestDuringDraw)
                {
                    _requestDuringDraw = false;
                    Director::getInstance()->requestRender();
                }
            });
            director->getEventDispatcher()->addEventListenerWithSceneGraphPriority(afterDraw, this);
            director->requestRender();
            break;
        }
        case 100:
            check(stats.frames >= _baseline + 2, "request during rendering survives to another frame");
            _offscreen = RenderTexture::create(32, 32);
            _offscreen->beginWithClear(0, 1, 0, 1);
            _shape->visit(director->getRenderer(), Mat4::IDENTITY, 0);
            _offscreen->end();
            director->getRenderer()->addCallbackCommand([this]() {
                _offscreen->newImage([this](RefPtr<Image> image) {
                    _readback = image && image->getWidth() == 32 && image->getHeight() == 32 &&
                                image->getData()[0] == 0 && image->getData()[1] == 255 && image->getData()[2] == 0 &&
                                image->getData()[3] == 255;
                });
            });
            break;
        case 105:
            utils::captureScreen([this](RefPtr<Image> image) { _screenCapture = bool(image); });
            break;
        case 110:
            check(_screenCapture, "screen capture wakes rendering from idle");
            check(_readback, "offscreen commands and GPU readback complete from idle");
            _baseline = stats.frames;
#if !AX_RENDER_DEMAND_EXTERNAL_LOOP
            view->surfaceAvailable = false;
#endif
            _shape->setPositionX(70);
            break;
        case 115:
#if !AX_RENDER_DEMAND_EXTERNAL_LOOP
            check(stats.frames == _baseline, "absent surface skips scene construction");
            check(director->isRenderRequested(), "absent surface preserves invalidation");
            view->surfaceAvailable = true;
#endif
            break;
        case 120:
#if AX_RENDER_DEMAND_EXTERNAL_LOOP
            check(stats.frames > _baseline, "position mutation requests a frame");
#else
            check(stats.frames > _baseline, "restored surface renders pending state");
#endif
            _baseline = stats.frames;
#if !AX_RENDER_DEMAND_EXTERNAL_LOOP
            view->failNextPresent = true;
#endif
            director->requestRender();
            break;
        case 125:
#if !AX_RENDER_DEMAND_EXTERNAL_LOOP
            check(stats.frames >= _baseline + 2 && stats.failedPresentations >= 1,
                  "failed presentation retries the complete scene");
#endif
            _activity = director->acquireRenderActivity();
            _baseline = stats.frames;
            break;
        case 130:
            check(stats.frames >= _baseline + 4, "scoped activity forces frames");
            _activity.reset();
            break;
        case 135:
            _baseline = stats.frames;
            break;
        case 145:
            check(compareContinuous || stats.frames == _baseline, "activity cancellation settles after final frame");
            director->stopAnimation();
            director->startAnimation();
            break;
        case 150:
            check(stats.frames > _baseline, "lifecycle restart requests a frame");
            _baseline = stats.frames;
#if AX_RENDER_DEMAND_EXTERNAL_LOOP
            // A fake context-loss notification would discard live Android texture names
            // without the platform's accompanying texture reload sequence.
            director->requestRender(RenderReason::Surface);
#else
            director->getEventDispatcher()->dispatchCustomEvent(EVENT_RENDERER_RECREATED);
#endif
            break;
        case 155:
#if AX_RENDER_DEMAND_EXTERNAL_LOOP
            check(stats.frames > _baseline, "explicit surface invalidation requests a frame");
#else
            check(stats.frames > _baseline, "context recreation notification requests a frame");
#endif
            _baseline = stats.frames;
            _shape->clear();
            _shape->drawSolidCircle(Vec2(50, 50), 25, 0, 24, Color4F::RED);
            break;
        case 160:
            check(stats.frames > _baseline, "procedural geometry mutation after idle");
            _baseline = stats.frames;
            getDefaultCamera()->setZoom(1.01f);
            break;
        case 165:
            check(stats.frames > _baseline, "camera mutation wakes rendering");
            _baseline = stats.frames;
            view->setDesignResolutionSize(480, 321, ResolutionPolicy::SHOW_ALL);
            break;
        case 170:
            check(stats.frames > _baseline, "resolution mutation wakes rendering");
            _baseline = stats.frames;
            _label->setOpacity(100);
            _label->setColor(Color3B::YELLOW);
            {
                const auto previousQuad   = _sprite->getQuad();
                uint8_t pixels[4 * 4 * 4] = {};
                auto atlas                = new Texture2D();
                atlas->initWithData(pixels, sizeof(pixels), backend::PixelFormat::RGBA8, 4, 4);
                auto frame = SpriteFrame::createWithTexture(atlas, ax::Rect(0, 0, 2, 2));
                _sprite->setSpriteFrame(frame);
                const auto sameRectQuad = _sprite->getQuad();
                check(previousQuad.tr.texCoords.u != sameRectQuad.tr.texCoords.u,
                      "same rectangle on a different texture refreshes UV coordinates");
                auto rotated =
                    SpriteFrame::createWithTexture(atlas, ax::Rect(0, 0, 2, 2), true, Vec2::ZERO, Vec2(2, 2));
                _sprite->setSpriteFrame(rotated);
                const auto rotatedQuad = _sprite->getQuad();
                check(sameRectQuad.tr.texCoords.u != rotatedQuad.tr.texCoords.u ||
                          sameRectQuad.tr.texCoords.v != rotatedQuad.tr.texCoords.v,
                      "same rectangle with different rotation refreshes UV coordinates");
                atlas->release();
            }
            break;
        case 175:
            check(stats.frames > _baseline, "color and opacity mutation wakes rendering");
            _baseline = stats.frames;
            {
                auto child = Node::create();
                addChild(child);
                child->setLocalZOrder(1);
                removeChild(child);
            }
            break;
        case 180:
            check(stats.frames > _baseline, "node insertion, reordering and removal wake rendering");
            _baseline = stats.frames;
            _clip->setAlphaThreshold(0.75f);
            _clip->setInverted(true);
            break;
        case 182:
#if AX_RENDER_DEMAND_EXTERNAL_LOOP
            utils::captureScreen([this](RefPtr<Image> image) { _finalTextVisible = containsYellowText(image.get()); });
#endif
            break;
        case 185:
            check(stats.frames > _baseline, "clipping properties wake rendering");
            _baseline = stats.frames;
            break;
        case 195:
            check(compareContinuous || stats.frames == _baseline, "alpha-tested clipping settles at rest");
#if AX_RENDER_DEMAND_EXTERNAL_LOOP
            check(_finalTextVisible, "updated UI text is visible in GPU readback");
#endif
            break;
        case 200:
        {
            const auto size = _sprite->getContentSize();
            _sprite->setAutoSize(false);
            _sprite->setContentSize(Vec2::ZERO);
            _sprite->setTextureRect(_sprite->getTextureRect(), true, size);
            check(_sprite->getContentSize() == size, "reapplying a sprite rectangle restores zero content size");
            auto state                 = _sprite->getProgramState();
            const auto textureLocation = state->getUniformLocation(backend::Uniform::TEXTURE);
            auto texture               = _sprite->getTexture()->getBackendTexture();
            state->setTexture(textureLocation, 0, 1, texture);
            state->setTextureArray(textureLocation, {0}, {texture});
            bool defaultTextureIndex = true;
            for (const auto& entry : state->getVertexTextureInfos())
                for (auto index : entry.second.indexs)
                    defaultTextureIndex &= index == 0;
            for (const auto& entry : state->getFragmentTextureInfos())
                for (auto index : entry.second.indexs)
                    defaultTextureIndex &= index == 0;
            check(defaultTextureIndex, "texture array assignment resets the previous texture plane index");
            state->setTexture(textureLocation, 0, texture);
            _richText = ui::RichText::createWithXML("After draw");
            _richText->ignoreContentAdaptWithSize(false);
            _richText->setContentSize(Vec2(180, 40));
            _richText->setPosition(Vec2(240, 270));
            addChild(_richText);
            break;
        }
        case 215:
        {
            _baseline     = stats.frames;
            auto listener = EventListenerCustom::create(Director::EVENT_AFTER_DRAW, [this](EventCustom*) {
                if (!_richTextChanged)
                {
                    _richTextChanged = true;
                    _richText->setHorizontalAlignment(ui::RichText::HorizontalAlignment::RIGHT);
                }
            });
            director->getEventDispatcher()->addEventListenerWithSceneGraphPriority(listener, this);
            director->requestRender();
            break;
        }
        case 225:
            check(stats.frames >= _baseline + 2, "rich text mutation after draw requests the next image");
            _baseline = stats.frames;
            break;
        case 235:
            check(compareContinuous || stats.frames == _baseline, "rich text layout settles at rest");
            _baseline = stats.frames;
            {
                auto listener = EventListenerCustom::create(Director::EVENT_AFTER_DRAW, [this](EventCustom*) {
                    if (!_uniformChanged)
                    {
                        _uniformChanged = true;
                        auto state      = _sprite->getProgramState();
                        Mat4 matrix;
                        Mat4::createTranslation(123, 456, 0, &matrix);
                        state->setUniform(state->getUniformLocation(backend::Uniform::MVP_MATRIX), matrix.m,
                                          sizeof(matrix.m));
                    }
                });
                director->getEventDispatcher()->addEventListenerWithSceneGraphPriority(listener, this);
            }
            director->requestRender();
            break;
        case 245:
            check(stats.frames >= _baseline + 2, "uniform mutation after draw requests the next image");
            _baseline = stats.frames;
            break;
        case 255:
            check(compareContinuous || stats.frames == _baseline, "uniform mutation settles at rest");
            {
                char result[256];
                std::snprintf(result, sizeof(result),
                              "RESULT mode=%s failures=%d ticks=%llu frames=%llu presentations=%llu failed=%llu",
                              compareContinuous ? "Continuous" : "OnDemand", failures,
                              static_cast<unsigned long long>(stats.ticks),
                              static_cast<unsigned long long>(stats.frames),
                              static_cast<unsigned long long>(stats.presentations),
                              static_cast<unsigned long long>(stats.failedPresentations));
                report(result);
            }
#if AX_RENDER_DEMAND_EXTERNAL_LOOP
            if (++scenarioPass < 3)
            {
                auto next               = ProbeScene::create();
                next->view              = view;
                next->compareContinuous = scenarioPass == 1;
                director->replaceScene(next);
            }
            else
            {
                suiteComplete = true;
                report("SUITE COMPLETE; READY FOR LIFECYCLE TESTS");
            }
#else
            director->end();
#endif
            break;
        default:
            break;
        }
    }

private:
    void check(bool passed, const char* message)
    {
        char result[256];
        std::snprintf(result, sizeof(result), "%s: %s", passed ? "PASS" : "FAIL", message);
        report(result);
        if (!passed)
            ++failures;
    }
    unsigned _tick          = 0;
    uint64_t _baseline      = 0;
    ClippingNode* _clip     = nullptr;
    DrawNode* _shape        = nullptr;
    Label* _label           = nullptr;
    ui::RichText* _richText = nullptr;
    bool _uniformChanged    = false;
    bool _richTextChanged   = false;
    Sprite* _sprite         = nullptr;
    Texture2D* _texture     = nullptr;
    RefPtr<RenderTexture> _offscreen;
    RenderActivity _activity;
    bool _finalTextVisible  = false;
    bool _resumeTextVisible = false;
    bool _readback          = false;
    bool _screenCapture     = false;
    bool _queuedCallback    = false;
    std::atomic_bool _queuedOperation{false};
    int _resumeTick         = -1;
    bool _temporaryReleased = false;
    unsigned _updateEvents  = 0;
    bool _requestDuringDraw = false;
};

class ProbeApplication : public Application
{
public:
    bool continuous = false;
    void initGfxContextAttrs() override
    {
        GfxContextAttrs attrs = {8, 8, 8, 8, 24, 8, 0};
        RenderView::setGfxContextAttrs(attrs);
    }
    bool applicationDidFinishLaunching() override
    {
#if AX_TARGET_PLATFORM == AX_PLATFORM_ANDROID || AX_TARGET_PLATFORM == AX_PLATFORM_IOS
        auto view = Director::getInstance()->getRenderView();
#elif AX_TARGET_PLATFORM == AX_PLATFORM_WASM
        auto view = RenderViewImpl::createWithRect("Axmol render demand", ax::Rect(0, 0, 480, 320));
        Director::getInstance()->setRenderView(view);
#else
        auto view = new ProbeView();
        if (!view->initialize())
        {
            view->release();
            ++failures;
            return false;
        }
        auto director = Director::getInstance();
        director->setRenderView(view);
#endif
        auto directorForScene = Director::getInstance();
        view->setDesignResolutionSize(480, 320, ResolutionPolicy::SHOW_ALL);
        auto scene               = ProbeScene::create();
        scene->view              = view;
        scene->compareContinuous = continuous;
        directorForScene->runWithScene(scene);
        return true;
    }
    void applicationDidEnterBackground() override
    {
        auto director    = Director::getInstance();
        backgroundFrames = director->getRenderDiagnostics().frames;
        director->requestRender();
        director->stopAnimation();
        report("LIFECYCLE BACKGROUND");
    }
    void applicationWillEnterForeground() override
    {
        Director::getInstance()->startAnimation();
        resumed = suiteComplete;
        report("LIFECYCLE FOREGROUND");
    }
};
}  // namespace

#if AX_TARGET_PLATFORM == AX_PLATFORM_ANDROID
void axmol_android_app_init(JNIEnv*)
{
    static ProbeApplication application;
}
#elif AX_TARGET_PLATFORM == AX_PLATFORM_IOS
int main(int argc, char** argv)
{
    ProbeApplication application;
    @autoreleasepool
    {
        return UIApplicationMain(argc, argv, nil, @"AxmolAppController");
    }
}
#elif AX_TARGET_PLATFORM == AX_PLATFORM_WASM
namespace
{
std::unique_ptr<ProbeApplication> wasmApplication;
int captureResult = -1;
}  // namespace

extern "C" EMSCRIPTEN_KEEPALIVE double render_demand_metric(int metric)
{
    const auto director = Director::getInstance();
    const auto stats    = director->getRenderDiagnostics();
    switch (metric)
    {
    case 0:
        return stats.ticks;
    case 1:
        return stats.frames;
    case 2:
        return stats.presentations;
    case 3:
        return failures;
    case 4:
        return suiteComplete;
    case 5:
        return director->isRenderRequested();
    case 6:
        return captureResult;
    case 7:
        return backgroundEvents;
    case 8:
        return foregroundEvents;
    default:
        return -1;
    }
}

extern "C" EMSCRIPTEN_KEEPALIVE void render_demand_request()
{
    Director::getInstance()->requestRender();
}

extern "C" EMSCRIPTEN_KEEPALIVE void render_demand_capture()
{
    captureResult = -1;
    utils::captureScreen([](RefPtr<Image> image) { captureResult = containsYellowText(image.get()) ? 1 : 0; });
}

void axmol_wasm_app_exit()
{
    wasmApplication.reset();
}

int main(int argc, char** argv)
{
    wasmApplication = std::make_unique<ProbeApplication>();
    return wasmApplication->run();
}
#else
int main(int argc, char** argv)
{
    ProbeApplication application;
    application.continuous = argc > 1 && std::string_view(argv[1]) == "--continuous";
    const int result       = application.run();
    return failures ? 1 : result;
}

#endif
