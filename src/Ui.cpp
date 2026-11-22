#include "Ui.hpp"

#include "Run.hpp"

#include <Geode/modify/PauseLayer.hpp>
#include <algorithm>

using namespace geode::prelude;

namespace {
    constexpr int RowsPerPage = 7;
    constexpr float BarScale = 3.f;     // pixels per frame
    constexpr float DividerX = 30.f;

    std::string withCommas(int value) {
        std::string digits = std::to_string(value);
        std::string out;
        int count = 0;
        for (size_t i = digits.size(); i-- > 0;) {
            out.insert(out.begin(), digits[i]);
            if (++count % 3 == 0 && i > 0 && digits[i - 1] != '-') out.insert(out.begin(), ',');
        }
        return out;
    }
}

bool ResultsPopup::init(PauseLayer* pause) {
    if (!Popup::init(440.f, 290.f)) return false;

    m_pause = pause;
    if (m_bgSprite) m_bgSprite->setColor({ 34, 26, 52 });
    this->setTitle("Frame Windows");

    // Red and blue banner under the title.
    auto* redBar = CCLayerColor::create({ 255, 82, 82, 255 }, 200.f, 3.f);
    m_mainLayer->addChildAtPosition(redBar, Anchor::Top, ccp(-200, -44));
    auto* blueBar = CCLayerColor::create({ 82, 148, 255, 255 }, 200.f, 3.f);
    m_mainLayer->addChildAtPosition(blueBar, Anchor::Top, ccp(0, -44));

    auto& S = fw::state();

    // Summary line.
    std::string summary;
    if (!S.hasResults) {
        summary = "No analysis yet. Play a level, pause, and press Analyze.";
    }
    else {
        double sum = 0;
        int n = 0;
        fw::Click const* tight = nullptr;
        for (auto const& c : S.results) {
            if (!c.reliable) continue;
            sum += c.window();
            n++;
            if (!tight || c.window() < tight->window()) tight = &c;
        }
        int bad = static_cast<int>(S.results.size()) - n;

        if (n == 0) {
            summary = "None of the clicks could be reproduced.";
        }
        else {
            summary = fmt::format("{} clicks   |   average {:.1f} frames   |   tightest {} (frame {})",
                n, sum / n, tight->window(), withCommas(tight->down));
            if (bad > 0) summary += fmt::format("   |   {} not reproduced", bad);
        }
    }
    auto* summaryLabel = CCLabelBMFont::create(summary.c_str(), "chatFont.fnt");
    summaryLabel->limitLabelWidth(400.f, .7f, .3f);
    m_mainLayer->addChildAtPosition(summaryLabel, Anchor::Top, ccp(0, -60));

    m_rows = CCNode::create();
    m_mainLayer->addChildAtPosition(m_rows, Anchor::Top);

    // Buttons
    auto* analyzeSpr = ButtonSprite::create("Analyze", "goldFont.fnt", "GJ_button_06.png", .8f);
    analyzeSpr->setScale(.75f);
    auto* analyzeBtn = CCMenuItemSpriteExtra::create(analyzeSpr, this, menu_selector(ResultsPopup::onAnalyze));
    m_buttonMenu->addChildAtPosition(analyzeBtn, Anchor::Bottom, ccp(-130, 26));

    auto* prevSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    prevSpr->setScale(.6f);
    auto* prevBtn = CCMenuItemSpriteExtra::create(prevSpr, this, menu_selector(ResultsPopup::onPrev));
    m_buttonMenu->addChildAtPosition(prevBtn, Anchor::Bottom, ccp(20, 26));

    auto* nextSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
    nextSpr->setScale(.6f);
    nextSpr->setFlipX(true);
    auto* nextBtn = CCMenuItemSpriteExtra::create(nextSpr, this, menu_selector(ResultsPopup::onNext));
    m_buttonMenu->addChildAtPosition(nextBtn, Anchor::Bottom, ccp(140, 26));

    m_pageLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_pageLabel->setScale(.8f);
    m_mainLayer->addChildAtPosition(m_pageLabel, Anchor::Bottom, ccp(80, 26));

    this->buildPage();
    return true;
}

void ResultsPopup::buildPage() {
    auto& S = fw::state();
    m_rows->removeAllChildren();

    int total = static_cast<int>(S.results.size());
    int pages = std::max(1, (total + RowsPerPage - 1) / RowsPerPage);
    m_page = std::clamp(m_page, 0, pages - 1);
    m_pageLabel->setString(fmt::format("{} / {}", m_page + 1, pages).c_str());

    auto addText = [this](std::string const& text, float x, float y, ccColor3B color, float scale, CCPoint anchor) {
        auto* label = CCLabelBMFont::create(text.c_str(), "chatFont.fnt");
        label->setScale(scale);
        label->setColor(color);
        label->setAnchorPoint(anchor);
        label->setPosition({ x, y });
        m_rows->addChild(label);
    };

    // Column headers.
    addText("#", -205, -80, { 150, 150, 160 }, .6f, ccp(0, .5f));
    addText("Frame", -178, -80, { 150, 150, 160 }, .6f, ccp(0, .5f));
    addText("Earlier", DividerX - 50, -80, fw::Red, .6f, ccp(.5f, .5f));
    addText("Later", DividerX + 50, -80, fw::Blue, .6f, ccp(.5f, .5f));
    addText("Window", 195, -80, { 235, 235, 240 }, .6f, ccp(1.f, .5f));

    for (int r = 0; r < RowsPerPage; r++) {
        int i = m_page * RowsPerPage + r;
        if (i >= total) break;

        auto const& c = S.results[i];
        float y = -102.f - r * 21.f;

        addText(std::to_string(i + 1), -205, y, { 150, 150, 160 }, .7f, ccp(0, .5f));
        addText(withCommas(c.down), -178, y, { 235, 235, 240 }, .7f, ccp(0, .5f));

        if (!c.reliable) {
            addText("could not reproduce", DividerX, y, { 130, 130, 140 }, .65f, ccp(.5f, .5f));
            continue;
        }

        float redW = c.early * BarScale;
        float blueW = c.late * BarScale;
        if (redW > 0.f) {
            auto* bar = CCLayerColor::create({ 255, 82, 82, 255 }, redW, 9.f);
            bar->setPosition({ DividerX - redW, y - 4.5f });
            m_rows->addChild(bar);
        }
        if (blueW > 0.f) {
            auto* bar = CCLayerColor::create({ 82, 148, 255, 255 }, blueW, 9.f);
            bar->setPosition({ DividerX, y - 4.5f });
            m_rows->addChild(bar);
        }
        auto* divider = CCLayerColor::create({ 245, 245, 250, 255 }, 2.f, 15.f);
        divider->setPosition({ DividerX - 1.f, y - 7.5f });
        m_rows->addChild(divider);

        addText(fmt::format("{}{}", c.early, c.earlyCapped ? "+" : ""), 125, y, fw::Red, .7f, ccp(.5f, .5f));
        addText(fmt::format("{}{}", c.late, c.lateCapped ? "+" : ""), 160, y, fw::Blue, .7f, ccp(.5f, .5f));
        addText(fmt::format("{}", c.window()), 195, y, { 245, 245, 250 }, .8f, ccp(1.f, .5f));
    }

    if (total == 0 && S.hasResults) {
        addText("The run had no clicks.", 0, -140, { 150, 150, 160 }, .8f, ccp(.5f, .5f));
    }
}

void ResultsPopup::onPrev(CCObject*) {
    m_page--;
    this->buildPage();
}

void ResultsPopup::onNext(CCObject*) {
    m_page++;
    this->buildPage();
}

void ResultsPopup::onAnalyze(CCObject*) {
    auto* pl = PlayLayer::get();
    if (!pl) {
        FLAlertLayer::create("Frame Windows", "Open this from the pause menu inside a level.", "OK")->show();
        return;
    }

    std::string err = fw::beginAnalysis(pl);
    if (!err.empty()) {
        FLAlertLayer::create("Frame Windows", err, "OK")->show();
        return;
    }

    this->onClose(nullptr);
    if (m_pause) m_pause->onResume(nullptr);   // the analysis runs while the level is unpaused
}

ResultsPopup* ResultsPopup::create(PauseLayer* pause) {
    auto* ret = new ResultsPopup();
    if (ret->init(pause)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

// A button in the pause menu that opens the window.
class $modify(FWPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto* label = CCLabelBMFont::create("FW", "bigFont.fnt");
        auto* sprite = CircleButtonSprite::create(label, CircleBaseColor::Red, CircleBaseSize::Medium);
        auto* button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(FWPauseLayer::onFrameWindows));
        button->setID("frame-windows-button"_spr);

        if (auto* menu = this->getChildByID("right-button-menu")) {
            menu->addChild(button);
            menu->updateLayout();
        }
        else {
            // Fallback if the pause menu layout is different: own menu, above the pause layer's touches.
            auto win = CCDirector::get()->getWinSize();
            auto* own = CCMenu::create();
            own->setPosition({ 0.f, 0.f });
            own->setHandlerPriority(-600);
            button->setPosition({ win.width - 40.f, 40.f });
            own->addChild(button);
            this->addChild(own, 100);
        }
    }

    void onFrameWindows(CCObject*) {
        ResultsPopup::create(this)->show();
    }
};
