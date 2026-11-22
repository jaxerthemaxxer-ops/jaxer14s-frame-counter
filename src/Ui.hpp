#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

// The results window: a red and blue bar per click, newest analysis first page.
class ResultsPopup : public geode::Popup {
protected:
    geode::Ref<PauseLayer> m_pause;
    cocos2d::CCNode* m_rows = nullptr;
    cocos2d::CCLabelBMFont* m_pageLabel = nullptr;
    int m_page = 0;

    bool init(PauseLayer* pause);
    void buildPage();

    void onPrev(cocos2d::CCObject*);
    void onNext(cocos2d::CCObject*);
    void onAnalyze(cocos2d::CCObject*);

public:
    static ResultsPopup* create(PauseLayer* pause);
};
