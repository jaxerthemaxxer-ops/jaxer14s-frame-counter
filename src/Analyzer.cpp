#include "Run.hpp"
#include "Ui.hpp"

#include <algorithm>
#include <chrono>
#include <climits>

using namespace geode::prelude;
using Clock = std::chrono::steady_clock;

namespace fw {

    namespace {
        // Slightly more than one physics step, so every call to the game's update runs exactly one.
        constexpr float StepDt = (1.f / 240.f) * 1.0005f;
        constexpr double BudgetMs = 18.0;   // time spent testing per game frame, keeps the screen alive

        class CancelHandler : public CCNode {
        public:
            static bool requested;
            static CancelHandler* create() {
                auto* ret = new CancelHandler();
                if (ret->init()) { ret->autorelease(); return ret; }
                delete ret;
                return nullptr;
            }
            void onCancel(CCObject*) { requested = true; }
        };
        bool CancelHandler::requested = false;

        struct Engine {
            std::vector<Click> clicks;
            size_t index = 0;
            int tests = 0;
            int unreliable = 0;
            float savedSfx = 1.f;
            bool active = false;
            Clock::time_point start;

            CCLayerColor* overlay = nullptr;
            CCLayerColor* fill = nullptr;
            CCLabelBMFont* status = nullptr;
            float barWidth = 300.f;
        };
        Engine E;

        double msSince(Clock::time_point t) {
            return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
        }

        // ---------- Overlay ----------

        void makeOverlay() {
            auto* scene = CCDirector::get()->getRunningScene();
            if (!scene) return;
            auto win = CCDirector::get()->getWinSize();

            E.overlay = CCLayerColor::create({ 16, 12, 28, 245 }, win.width, win.height);

            auto* redStripe = CCLayerColor::create({ 255, 82, 82, 255 }, win.width / 2, 6.f);
            redStripe->setPosition({ 0.f, win.height - 6.f });
            E.overlay->addChild(redStripe);
            auto* blueStripe = CCLayerColor::create({ 82, 148, 255, 255 }, win.width / 2, 6.f);
            blueStripe->setPosition({ win.width / 2, win.height - 6.f });
            E.overlay->addChild(blueStripe);

            auto* title = CCLabelBMFont::create("Measuring frame windows", "bigFont.fnt");
            title->setScale(.8f);
            title->setPosition({ win.width / 2, win.height / 2 + 50.f });
            E.overlay->addChild(title);

            auto* track = CCLayerColor::create({ 120, 40, 50, 255 }, E.barWidth, 12.f);
            track->setPosition({ win.width / 2 - E.barWidth / 2, win.height / 2 });
            E.overlay->addChild(track);

            E.fill = CCLayerColor::create({ 82, 148, 255, 255 }, 1.f, 12.f);
            E.fill->setPosition({ win.width / 2 - E.barWidth / 2, win.height / 2 });
            E.overlay->addChild(E.fill);

            E.status = CCLabelBMFont::create("Starting...", "chatFont.fnt");
            E.status->setPosition({ win.width / 2, win.height / 2 - 24.f });
            E.overlay->addChild(E.status);

            auto* handler = CancelHandler::create();
            E.overlay->addChild(handler);

            auto* spr = ButtonSprite::create("Cancel", "goldFont.fnt", "GJ_button_06.png", .8f);
            auto* btn = CCMenuItemSpriteExtra::create(spr, handler, menu_selector(CancelHandler::onCancel));
            auto* menu = CCMenu::create();
            menu->addChild(btn);
            menu->setPosition({ win.width / 2, win.height / 2 - 70.f });
            E.overlay->addChild(menu);

            scene->addChild(E.overlay, 10000);
        }

        void updateOverlay() {
            if (!E.overlay) return;
            float frac = E.clicks.empty() ? 1.f : static_cast<float>(E.index) / static_cast<float>(E.clicks.size());
            E.fill->setContentSize({ std::max(1.f, E.barWidth * frac), 12.f });
            E.status->setString(fmt::format("{} / {} clicks   |   {:.1f} s   |   {} tests",
                E.index, E.clicks.size(), msSince(E.start) / 1000.0, E.tests).c_str());
        }

        void removeOverlay() {
            if (E.overlay) E.overlay->removeFromParent();
            E.overlay = nullptr;
            E.fill = nullptr;
            E.status = nullptr;
        }

        void setMuted(bool mute) {
            auto* fmod = FMODAudioEngine::get();
            if (!fmod) return;
            if (mute) {
                E.savedSfx = fmod->m_sfxVolume;
                fmod->setEffectsVolume(0.f);
            }
            else {
                fmod->setEffectsVolume(E.savedSfx);
            }
        }

        // ---------- Building the list of clicks ----------

        std::string buildClicks() {
            auto& S = state();
            E.clicks.clear();

            if (S.inputs.empty() || S.kept.empty()) {
                return "Nothing is recorded yet. Play part of a level first (turn on \"Record runs\" in the settings).";
            }

            for (int a = 0; a < 2; a++) {
                bool arg = a == 1;
                bool held = false;
                std::vector<Click> list;

                for (auto const& in : S.inputs) {
                    if (in.arg != arg) continue;
                    if (in.down && !held) {
                        held = true;
                        Click c;
                        c.arg = arg;
                        c.down = in.tick;
                        list.push_back(c);
                    }
                    else if (!in.down && held) {
                        held = false;
                        list.back().up = in.tick;
                    }
                }

                for (size_t k = 0; k < list.size(); k++) {
                    Click& c = list[k];

                    // Prefer the latest saved state that still leaves room to move the click earlier.
                    int room = c.down - S.settings.maxWindow - 1;
                    int pick = -1;
                    for (int i = 0; i < static_cast<int>(S.kept.size()); i++) {
                        if (S.kept[i].tick <= room) pick = i;
                    }
                    if (pick < 0) {
                        for (int i = 0; i < static_cast<int>(S.kept.size()); i++) {
                            if (S.kept[i].tick <= c.down - 1) { pick = i; break; }
                        }
                    }
                    if (pick < 0) {
                        c.reliable = false;
                        E.clicks.push_back(c);
                        continue;
                    }
                    c.anchorIdx = pick;
                    c.anchorTick = S.kept[pick].tick;

                    int prev = k > 0 ? (list[k - 1].up >= 0 ? list[k - 1].up : list[k - 1].down) : INT_MIN / 2;
                    int next = k + 1 < list.size() ? list[k + 1].down : INT_MAX / 2;

                    c.maxEarly = std::max(0, std::min({ S.settings.maxWindow, c.down - c.anchorTick - 1, c.down - prev - 1 }));
                    int latest = c.up >= 0 ? c.up : c.down;
                    c.maxLate = std::max(0, std::min(S.settings.maxWindow, next - latest - 1));

                    E.clicks.push_back(c);
                }
            }

            std::sort(E.clicks.begin(), E.clicks.end(), [](Click const& x, Click const& y) { return x.down < y.down; });

            if (E.clicks.empty()) return "No complete clicks were recorded.";
            return "";
        }

        // ---------- One test simulation ----------

        // Moves one click `shift` frames, then runs the level from a saved state a few frames before it.
        // Returns true when the player is still alive after the look-ahead.
        bool survives(PlayLayer* pl, std::function<void(float)> const& step, Click const& c, int shift) {
            auto& S = state();
            if (c.anchorIdx < 0 || c.anchorIdx >= static_cast<int>(S.kept.size())) return false;

            Anchor const& anchor = S.kept[c.anchorIdx];
            int endTick = std::max(c.down + shift, (c.up >= 0 ? c.up : c.down) + shift) + S.settings.lookahead;

            // Which buttons were already held when the saved state was taken.
            bool held[2] = { false, false };
            for (auto const& in : S.inputs) {
                if (in.tick > anchor.tick) break;
                held[in.arg ? 1 : 0] = in.down;
            }

            S.seq.clear();
            S.seqNext = 0;
            for (int a = 0; a < 2; a++) {
                if (held[a]) S.seq.push_back({ anchor.tick, true, a == 1 });
            }
            for (auto const& in : S.inputs) {
                if (in.tick <= anchor.tick) continue;
                if (in.tick > endTick) break;

                Input x = in;
                bool isThisDown = x.arg == c.arg && x.down && x.tick == c.down;
                bool isThisUp = x.arg == c.arg && !x.down && c.up >= 0 && x.tick == c.up;
                if (isThisDown || isThisUp) x.tick += shift;
                S.seq.push_back(x);
            }
            std::stable_sort(S.seq.begin(), S.seq.end(), [](Input const& a, Input const& b) { return a.tick < b.tick; });

            S.simDead = false;
            S.simFinished = false;

            pl->loadFromCheckpoint(anchor.cp.data());
            pl->m_extraDelta = 0.0;
            if (pl->m_player1) pl->m_player1->releaseAllButtons();
            if (pl->m_player2) pl->m_player2->releaseAllButtons();

            int guard = 0;
            int limit = (endTick - anchor.tick) + 200;
            while (!S.simDead && !S.simFinished && tickOf(pl) < endTick && guard++ < limit) {
                step(StepDt);
            }

            E.tests++;
            return !S.simDead;
        }

        void measure(PlayLayer* pl, std::function<void(float)> const& step, Click& c) {
            if (c.anchorIdx < 0) {
                c.early = c.late = 0;
                E.unreliable++;
                return;
            }

            // The click as it was played must survive, otherwise the saved state or the physics
            // did not reproduce the run and nothing measured from it can be trusted.
            if (!survives(pl, step, c, 0)) {
                c.reliable = false;
                c.early = c.late = 0;
                E.unreliable++;
                return;
            }

            // The window is assumed to be one unbroken stretch around the original frame, so each
            // side can be found by halving instead of trying every shift one by one.
            int lo = 0, hi = c.maxEarly + 1;
            while (hi - lo > 1) {
                int mid = (lo + hi) / 2;
                if (survives(pl, step, c, -mid)) lo = mid; else hi = mid;
            }
            c.early = lo;
            c.earlyCapped = lo == c.maxEarly;

            lo = 0;
            hi = c.maxLate + 1;
            while (hi - lo > 1) {
                int mid = (lo + hi) / 2;
                if (survives(pl, step, c, mid)) lo = mid; else hi = mid;
            }
            c.late = lo;
            c.lateCapped = lo == c.maxLate;
        }

        void finish(PlayLayer* pl, bool ok) {
            auto& S = state();

            S.analyzing = false;
            S.seq.clear();
            S.seqNext = 0;
            E.active = false;
            CancelHandler::requested = false;

            setMuted(false);
            removeOverlay();

            if (ok) {
                S.results = E.clicks;
                S.hasResults = true;
                S.analysisSeconds = msSince(E.start) / 1000.0;
                S.analysisTests = E.tests;
                log::info("Frame windows: {} clicks, {} tests, {:.2f} s, {} not reproducible",
                    E.clicks.size(), E.tests, S.analysisSeconds, E.unreliable);
            }

            // Put the level back at the start without throwing the recorded run away.
            S.suppressReset = true;
            pl->resetLevel();
            S.suppressReset = false;
            pl->pauseGame(false);

            if (ok) ResultsPopup::create(nullptr)->show();
        }
    }

    std::string beginAnalysis(PlayLayer* pl) {
        auto& S = state();
        if (S.analyzing) return "An analysis is already running.";
        if (!pl) return "Open a level first.";

        std::string err = buildClicks();
        if (!err.empty()) return err;

        E.index = 0;
        E.tests = 0;
        E.unreliable = 0;
        E.start = Clock::now();
        E.active = true;
        CancelHandler::requested = false;

        setMuted(true);
        makeOverlay();
        S.analyzing = true;
        return "";
    }

    void analyzerTick(PlayLayer* pl, std::function<void(float)> const& step) {
        if (!E.active) {
            state().analyzing = false;
            return;
        }

        auto t0 = Clock::now();
        while (E.index < E.clicks.size() && !CancelHandler::requested) {
            measure(pl, step, E.clicks[E.index]);
            E.index++;
            if (msSince(t0) > BudgetMs) break;
        }

        updateOverlay();

        if (CancelHandler::requested) finish(pl, false);
        else if (E.index >= E.clicks.size()) finish(pl, true);
    }

    void abortAnalysis() {
        auto& S = state();
        if (!S.analyzing) return;
        S.analyzing = false;
        S.seq.clear();
        E.active = false;
        CancelHandler::requested = false;
        setMuted(false);
        removeOverlay();
    }
}
