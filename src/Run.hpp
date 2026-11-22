#pragma once

#include <Geode/Geode.hpp>

#include <deque>
#include <functional>
#include <string>
#include <vector>

// Everything the mod remembers about the current run and its analysis.
namespace fw {

    // GD 2.2 runs its physics at 240 steps per second.
    inline constexpr double TPS = 240.0;

    // Red = clicks that could have come earlier, blue = clicks that could have come later.
    inline const cocos2d::ccColor3B Red = { 255, 82, 82 };
    inline const cocos2d::ccColor3B Blue = { 82, 148, 255 };

    // One jump-button event. `arg` is the third argument the game passed to handleButton.
    // It is stored as-is, so it replays exactly the way it was recorded.
    struct Input {
        int tick;
        bool down;
        bool arg;
    };

    // A saved copy of the whole game state, taken a few frames before a click.
    struct Anchor {
        geode::Ref<CheckpointObject> cp;
        int tick = 0;
    };

    struct Click {
        bool arg = false;
        int down = 0;
        int up = -1;            // -1 when the button was never released
        int anchorIdx = -1;     // index into State::kept
        int anchorTick = 0;
        int maxEarly = 0;       // how far this click can move earlier before hitting a neighbour
        int maxLate = 0;
        int early = 0;          // measured: frames earlier that still survive
        int late = 0;           // measured: frames later that still survive
        bool reliable = true;   // false when the unchanged click could not be reproduced
        bool earlyCapped = false;
        bool lateCapped = false;
        int window() const { return early + late + 1; }
    };

    struct Settings {
        bool record = true;
        int maxWindow = 12;
        int lookahead = 60;
    };

    struct State {
        Settings settings;

        // Recording
        std::vector<Input> inputs;
        std::deque<Anchor> recent;
        std::vector<Anchor> kept;       // sorted by tick
        int lastAnchorTick = -1000;
        int currentTick = 0;

        // Analysis
        bool analyzing = false;
        bool suppressReset = false;
        bool simDead = false;
        bool simFinished = false;
        std::vector<Input> seq;         // inputs for the test that is running right now
        size_t seqNext = 0;

        // Results
        std::vector<Click> results;
        bool hasResults = false;
        double analysisSeconds = 0.0;
        int analysisTests = 0;
    };

    State& state();
    void reloadSettings();
    int tickOf(GJBaseGameLayer* layer);

    // Recording
    void clearAll();
    void onReset(PlayLayer* pl);
    void maybeAnchor(PlayLayer* pl, int tick);
    void keepAnchorFor(int clickTick);

    // Analysis (Analyzer.cpp)
    std::string beginAnalysis(PlayLayer* pl);
    void analyzerTick(PlayLayer* pl, std::function<void(float)> const& step);
    void abortAnalysis();
}
