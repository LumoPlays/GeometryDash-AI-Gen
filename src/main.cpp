#include <Geode/Geode.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>

using namespace geode::prelude;

// Global tracking structure to save strings and positions across popup openings
struct LayoutTracker {
    std::map<int, std::string> segmentStrings; // Stores strings for slots 1-5
    float lastMaxX = 0.0f;                     // Tracks furthest object X coordinate spawned
    int activeSlot = 1;                        // Tracks currently selected UI slot
};
static LayoutTracker g_layoutTracker;

std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        if (!token.empty()) tokens.push_back(token);
    }
    return tokens;
}

class AIBuilderPopup : public FLAlertLayer {
protected:
    LevelEditorLayer* m_editorLayer;
    TextInput* m_textInput;
    std::vector<CCMenuItemToggler*> m_slotButtons;

    bool init(LevelEditorLayer* editorLayer) {
        if (!FLAlertLayer::init(75)) return false;

        m_editorLayer = editorLayer;
        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto mainLayer = CCLayer::create();
        this->addChild(mainLayer);

        // Main Dialog Box BG
        auto bg = CCScale9Sprite::create("GJ_square01.png");
        bg->setContentSize({ 380.f, 250.f });
        bg->setPosition(winSize / 2);
        mainLayer->addChild(bg);

        // Title
        auto title = CCLabelBMFont::create("AI Level Builder", "goldFont.fnt");
        title->setPosition({ winSize.width / 2, winSize.height / 2 + 100.f });
        title->setScale(0.85f);
        mainLayer->addChild(title);

        auto menu = CCMenu::create();
        menu->setPosition(winSize / 2);
        mainLayer->addChild(menu);

        // --- NEW: SLOT SELECTION BUTTONS (1, 2, 3, 4, 5) ---
        auto slotLabel = CCLabelBMFont::create("Select Segment Slot:", "chatFont.fnt");
        slotLabel->setPosition({ winSize.width / 2, winSize.height / 2 + 65.f });
        slotLabel->setScale(0.65f);
        mainLayer->addChild(slotLabel);

        float startX = -100.f;
        float spacing = 50.f;
        for (int i = 1; i <= 5; ++i) {
            std::string slotNumStr = std::to_string(i);

            auto offSpr = ButtonSprite::create(slotNumStr.c_str(), "goldFont.fnt", "GJ_button_02.png"); // Gray/blue inactive
            offSpr->setScale(0.65f);
            auto onSpr = ButtonSprite::create(slotNumStr.c_str(), "goldFont.fnt", "GJ_button_01.png");  // Green active
            onSpr->setScale(0.65f);

            auto toggler = CCMenuItemToggler::create(
                offSpr, onSpr, this, menu_selector(AIBuilderPopup::onSlotSelected)
            );
            toggler->setTag(i);
            toggler->setPosition({ startX + ((i - 1) * spacing), 35.f });

            // Highlight the globally saved active slot
            if (i == g_layoutTracker.activeSlot) {
                toggler->toggle(true);
            }

            menu->addChild(toggler);
            m_slotButtons.push_back(toggler);
        }

        // Text Input Area
        m_textInput = TextInput::create(320.f, "Paste layout string here...", "chatFont.fnt");
        m_textInput->setPosition({ winSize.width / 2, winSize.height / 2 - 15.f });
        m_textInput->setFilter("0123456789.,;");

        // Load whatever string was previously pasted in this active slot
        m_textInput->setString(g_layoutTracker.segmentStrings[g_layoutTracker.activeSlot]);
        mainLayer->addChild(m_textInput);

        // "Open Site" Button
        auto openSiteSpr = ButtonSprite::create("Open Site", "goldFont.fnt", "GJ_button_04.png");
        openSiteSpr->setScale(0.80f);
        auto openSiteBtn = CCMenuItemSpriteExtra::create(
            openSiteSpr, this, menu_selector(AIBuilderPopup::onOpenWebpage)
        );
        openSiteBtn->setPosition({ -90.f, -85.f });
        menu->addChild(openSiteBtn);

        // "Build Segment" Button
        auto buildSpr = ButtonSprite::create("Build Segment", "goldFont.fnt", "GJ_button_01.png");
        buildSpr->setScale(0.80f);
        auto buildBtn = CCMenuItemSpriteExtra::create(
            buildSpr, this, menu_selector(AIBuilderPopup::onBuildPressed)
        );
        buildBtn->setPosition({ 90.f, -85.f });
        menu->addChild(buildBtn);

        // Close Button
        auto closeSpr = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
        auto closeBtn = CCMenuItemSpriteExtra::create(
            closeSpr, this, menu_selector(AIBuilderPopup::onClose)
        );
        closeBtn->setPosition({ -175.f, 110.f });
        menu->addChild(closeBtn);

        this->setKeypadEnabled(true);
        this->setTouchEnabled(true);

        return true;
    }

    void onSlotSelected(CCObject* sender) {
        auto clickedToggler = static_cast<CCMenuItemToggler*>(sender);
        int targetSlot = clickedToggler->getTag();

        // Save current typing data to the old slot before swapping
        g_layoutTracker.segmentStrings[g_layoutTracker.activeSlot] = m_textInput->getString();

        // Update active slot state
        g_layoutTracker.activeSlot = targetSlot;

        // Force UI radio toggling behavior across numbers 1-5
        for (auto* toggler : m_slotButtons) {
            toggler->toggle(toggler->getTag() == targetSlot);
        }

        // Display previously saved string for newly selected slot
        m_textInput->setString(g_layoutTracker.segmentStrings[targetSlot]);
    }

    void keyBackClicked() override {
        this->onClose(nullptr);
    }

    void onClose(CCObject*) {
        // Final save cache check on exit
        if (m_textInput) {
            g_layoutTracker.segmentStrings[g_layoutTracker.activeSlot] = m_textInput->getString();
        }
        this->setKeyboardEnabled(false);
        this->removeFromParentAndCleanup(true);
    }

    void onOpenWebpage(CCObject*) {
        web::openLinkInBrowser("https://ailevelbuilder.lovable.app");
    }

    void onBuildPressed(CCObject* sender) {
        std::string layoutData = m_textInput->getString();
        g_layoutTracker.segmentStrings[g_layoutTracker.activeSlot] = layoutData;

        if (layoutData.empty()) {
            FLAlertLayer::create("Error", "Please input or paste a layout string for this slot!", "OK")->show();
            return;
        }

        // Reset completely only if building Slot 1 to give a fresh slate
        if (g_layoutTracker.activeSlot == 1) {
            if (m_editorLayer->m_objects && m_editorLayer->m_objects->count() > 0) {
                m_editorLayer->removeAllObjects();
            }
            g_layoutTracker.lastMaxX = 0.0f; // Reset position timeline marker
        }

        auto objects = splitString(layoutData, ';');
        int spawnCount = 0;
        float segmentMaxX = 0.0f;

        // Calculations for positional offsets
        float baseYOffset = 30.0f * 3.0f; // Safe vertical height above ground floor

        // Offset mapping logic
        float baseXOffset = 0.0f;
        if (g_layoutTracker.activeSlot == 1) {
            baseXOffset = 30.0f * 4.0f; // Give Slot 1 a 4-block buffer from player spawn
        }
        else {
            baseXOffset = g_layoutTracker.lastMaxX + (30.0f * 5.0f); // Chain 5 blocks directly after last segment
        }

        for (const auto& objData : objects) {
            auto params = splitString(objData, ',');
            if (params.size() < 3) continue;

            try {
                int id = std::stoi(params[0]);
                float gridX = std::stof(params[1]);
                float gridY = std::stof(params[2]);

                // Calculate total spatial grid position offsets
                float finalX = (gridX * 30.0f) + 15.0f + baseXOffset;
                float finalY = (gridY * 30.0f) + 15.0f + baseYOffset;

                CCPoint position = CCPointMake(finalX, finalY);

                auto gdObject = m_editorLayer->createObject(id, position, true);
                if (gdObject) {
                    m_editorLayer->m_objects->addObject(gdObject);
                    spawnCount++;

                    // Dynamically map the rightmost object coordinate boundary
                    if (finalX > segmentMaxX) {
                        segmentMaxX = finalX;
                    }
                }
            }
            catch (...) {}
        }

        // Update the global tracking boundary if objects successfully rendered
        if (spawnCount > 0) {
            g_layoutTracker.lastMaxX = segmentMaxX;
        }

        m_editorLayer->updateOptions();
        this->onClose(sender);

        std::string statusMsg = "Segment " + std::to_string(g_layoutTracker.activeSlot) +
            " built! Added " + std::to_string(spawnCount) + " objects.";
        FLAlertLayer::create("Success!", statusMsg.c_str(), "Awesome")->show();
    }

public:
    static AIBuilderPopup* create(LevelEditorLayer* editorLayer) {
        auto ret = new AIBuilderPopup();
        if (ret && ret->init(editorLayer)) {
            ret->autorelease();
            return ret;
        }
        CC_SAFE_DELETE(ret);
        return nullptr;
    }
};

class $modify(MyEditorPauseLayer, EditorPauseLayer) {
    bool init(LevelEditorLayer * editorLayer) {
        if (!EditorPauseLayer::init(editorLayer)) return false;

        CCMenu* menu = nullptr;
        if (auto children = this->getChildren()) {
            for (auto* obj : CCArrayExt<CCObject*>(children)) {
                if (auto castedMenu = dynamic_cast<CCMenu*>(obj)) {
                    if (castedMenu->getChildrenCount() > 0 && castedMenu->getPositionY() < 120.0f) {
                        menu = castedMenu;
                        break;
                    }
                }
            }
        }

        if (!menu) {
            menu = this->getChildByType<CCMenu>(0);
        }

        if (menu) {
            auto btnSprite = ButtonSprite::create("AI", "goldFont.fnt", "GJ_button_01.png");
            btnSprite->setScale(0.75f);

            auto btn = CCMenuItemSpriteExtra::create(
                btnSprite, this, menu_selector(MyEditorPauseLayer::onAILayerButton)
            );
            btn->setID("ai-level-builder-btn"_spr);

            menu->addChild(btn);
            menu->updateLayout();
        }

        return true;
    }

    void onAILayerButton(CCObject * sender) {
        AIBuilderPopup::create(this->m_editorLayer)->show();
    }
};