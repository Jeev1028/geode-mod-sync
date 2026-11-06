#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "SyncPopup.hpp"

using namespace geode::prelude;

class $modify(SyncMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        // Standalone menu (never injected into shared layout menus).
        auto menu = CCMenu::create();
        menu->setID("mod-sync-menu");
        auto spr = ButtonSprite::create("Mod Sync");
        spr->setScale(0.6f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(SyncMenuLayer::onSync));
        menu->addChild(btn);
        auto win = CCDirector::get()->getWinSize();
        menu->setPosition(ccp(70.f, win.height - 18.f));
        this->addChild(menu);
        return true;
    }

    void onSync(CCObject*) {
        SyncPopup::create()->show();
    }
};
