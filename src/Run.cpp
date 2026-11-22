#include "Run.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace fw {

    namespace {
        constexpr int AnchorEvery = 20;   // ticks between saved states while recording
        constexpr size_t RecentMax = 6;

        void addAnchor(PlayLayer* pl, int tick) {
            auto& S = state();
            CheckpointObject* cp = pl->createCheckpoint();
            if (!cp) return;
            S.recent.push_back(Anchor{ Ref<CheckpointObject>(cp), tick });
            while (S.recent.size() > RecentMax) S.recent.pop_front();
            S.lastAnchorTick = tick;
        }

        void truncateTo(int tick) {
            auto& S = state();
            std::erase_if(S.inputs, [&](Input const& i) { return i.tick >= tick; });
            std::erase_if(S.kept, [&](Anchor const& a) { return a.tick >= tick; });
            std::erase_if(S.recent, [&](Anchor const& a) { return a.tick >= tick; });
        }
    }

    State& state() {
        static State s;
        return s;
    }

    void reloadSettings() {
        auto* m = Mod::get();
        auto& s = state().settings;
        s.record = m->getSettingValue<bool>("record");
        s.maxWindow = static_cast<int>(m->getSettingValue<int64_t>("max-window"));
        s.lookahead = static_cast<int>(m->getSettingValue<int64_t>("lookahead"));
    }

    int tickOf(GJBaseGameLayer* layer) {
        return static_cast<int>(std::floor(layer->m_gameState.m_levelTime * TPS + 1e-3));
    }

    void clearAll() {
        auto& S = state();
        S.inputs.clear();
        S.recent.clear();
        S.kept.clear();
        S.seq.clear();
        S.seqNext = 0;
        S.lastAnchorTick = -1000;
        S.results.clear();
        S.hasResults = false;
    }

    // Called after every attempt start and every practice-mode respawn.
    // Anything recorded after this moment never happened, so it is dropped.
    void onReset(PlayLayer* pl) {
        auto& S = state();
        if (S.analyzing || S.suppressReset) return;

        int tick = tickOf(pl);
        truncateTo(tick);
        S.currentTick = tick;
        S.lastAnchorTick = -1000;
        if (S.settings.record) addAnchor(pl, tick);
    }

    void maybeAnchor(PlayLayer* pl, int tick) {
        auto& S = state();
        if (tick - S.lastAnchorTick >= AnchorEvery) addAnchor(pl, tick);
    }

    // A click is about to be recorded. Keep a saved state from far enough before it
    // that the click can later be moved earlier without leaving the saved range.
    void keepAnchorFor(int clickTick) {
        auto& S = state();
        int need = clickTick - S.settings.maxWindow - 1;

        Anchor const* best = nullptr;
        auto consider = [&](Anchor const& a) {
            if (a.tick <= need && (!best || a.tick > best->tick)) best = &a;
        };
        for (auto const& a : S.recent) consider(a);
        for (auto const& a : S.kept) consider(a);

        if (!best) {
            // Not enough history yet (the very start of a run): use the oldest state we have.
            auto consider2 = [&](Anchor const& a) {
                if (a.tick <= clickTick - 1 && (!best || a.tick < best->tick)) best = &a;
            };
            for (auto const& a : S.recent) consider2(a);
            for (auto const& a : S.kept) consider2(a);
        }
        if (!best) return;

        for (auto const& k : S.kept) if (k.tick == best->tick) return;

        Anchor copy = *best;
        auto it = std::upper_bound(S.kept.begin(), S.kept.end(), copy.tick,
            [](int t, Anchor const& a) { return t < a.tick; });
        S.kept.insert(it, copy);
    }
}

$on_mod(Loaded) {
    fw::reloadSettings();

    listenForSettingChanges<bool>("record", [](bool) { fw::reloadSettings(); });
    listenForSettingChanges<int64_t>("max-window", [](int64_t) { fw::reloadSettings(); });
    listenForSettingChanges<int64_t>("lookahead", [](int64_t) { fw::reloadSettings(); });
}
