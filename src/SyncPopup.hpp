#pragma once
#include <Geode/ui/Popup.hpp>

class SyncPopup : public geode::Popup {
protected:
    cocos2d::CCLabelBMFont* m_status = nullptr;
    bool init();
    void onExport(cocos2d::CCObject*);
    void onImport(cocos2d::CCObject*);

public:
    void setStatus(std::string const& text);
    static SyncPopup* create();
};
