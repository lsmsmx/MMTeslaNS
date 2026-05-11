#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include <switch.h>
#include <cstdio>
#include <string>

// =========================================================
// 1. SCENE SWITCHER DATA
// =========================================================
struct SceneEntry {
    int32_t state;
    int32_t sub;
    const char* name;
};

const SceneEntry SCENES[] = {
    {0, 0, "0: DATA_INITIALIZE"}, {0, 1, "1: SYSTEM_STARTUP"},
    {9, 2, "2: LOGO"}, {9, 3, "3: TITLE"}, {9, 4, "4: CONCEAL"},
    {9, 5, "5: GAME"}, {9, 6, "6: PLAYLIST"}, {9, 7, "7: CAPTURE"},
    {9, 8, "8: CS_GALLERY (State 9)"},
    {3, 8, "8: DT_MAIN"},
    {3, 9, "9: DT_MISC"}, {3, 10, "10: DT_OBJ"}, {3, 11, "11: DT_STG"},
    {3, 12, "12: DT_MOT"}, {3, 13, "13: DT_COLLISION"}, {3, 14, "14: DT_SPR"},
    {3, 15, "15: DT_AET"}, {3, 16, "16: DT_AUTH3D"}, {3, 17, "17: DT_CHR"},
    {3, 18, "18: DT_ITEM"}, {3, 19, "19: DT_PERF"}, {3, 20, "20: DT_PVSCRIPT"},
    {3, 21, "21: DT_PRINT"}, {3, 22, "22: DT_CARD"}, {3, 23, "23: DT_OPD"},
    {3, 24, "24: DT_SLIDER"}, {3, 25, "25: DT_GLITTER"}, {3, 26, "26: DT_GRAPHICS"},
    {3, 27, "27: DT_COL_CARD"}, {3, 28, "28: DT_PAD"},
    {4, 29, "29: TEST_MODE"}, {5, 30, "30: APP_ERROR"}, {3, 31, "31: UNK_31"},
    {6, 32, "32: CS_MENU"}, {6, 33, "33: CS_COMMERCE"}, {6, 34, "34: CS_OPTION"},
    {6, 35, "35: CS_TUTORIAL"}, {6, 36, "36: CS_CUST_SEL"}, {6, 37, "37: CS_TUTORIAL_37"},
    {6, 38, "38: CS_GALLERY (State 8)"}, {6, 39, "39: UNK_39"}, {6, 40, "40: UNK_40"},
    {6, 41, "41: UNK_41"}, {6, 42, "42: MENU_SWITCH"}, {6, 43, "43: UNK_43"},
    {6, 44, "44: OPTION_MENU_SW"}, {6, 45, "45: UNK_45"}, {6, 46, "46: UNK_46"}
};

constexpr int SCENE_COUNT = sizeof(SCENES) / sizeof(SCENES[0]);

const char* STATE_NAMES[] = {
    "STARTUP (0)", "ADVERTISE (1)", "GAME (2)", "DATA_TEST (3)", "TEST_MODE (4)",
    "APP_ERROR (5)", "CS_MENU (6)", "CUSTOMIZE (7)", "GALLERY (8)", "MENU_SW (9)", "GAME_SW (10)", "TSHIRT (11)"
};

// =========================================================
// 2. SEND COMMANDS TO GAME (IPC VIA SD CARD)
// =========================================================
// Note: Tesla runs as a separate background process. We use the SD card 
// to communicate with the main game process via simple signal files.

// For changing scenes (creates a file with two int32_t values: state and substate)
void SendStateCommand(int32_t state, int32_t substate) {
    FILE* f = fopen("sdmc:/DMLSwitchPort/diva_state_cmd.bin", "wb");
    if (f) {
        int32_t cmd[2] = { state, substate };
        fwrite(cmd, sizeof(int32_t), 2, f);
        fclose(f);
    }
}

// For ImGui and Overlay toggles (simply creates an empty signal file)
void SendToggleCommand(const char* path) {
    FILE* f = fopen(path, "wb");
    if (f) {
        fclose(f);
    }
}

// =========================================================
// 3. GRAPHICAL SCREENS (GUI)
// =========================================================

// Scene selection screen (State Switcher)
class SmartSelectorGui : public tsl::Gui {
private:
    int32_t m_idx = 9; // Default to DT_MAIN (3, 8)
public:
    virtual tsl::elm::Element* createUI() override {
        auto* frame = new tsl::elm::OverlayFrame("Diva Game Switcher", "Scroll LEFT/RIGHT to change");
        auto* list = new tsl::elm::List();

        auto* subItem = new tsl::elm::ListItem("Target Scene:");
        subItem->setValue(SCENES[m_idx].name);

        auto* stateItem = new tsl::elm::ListItem("Assigned GameState:");
        stateItem->setValue(STATE_NAMES[SCENES[m_idx].state]);

        // Handle left/right D-Pad inputs to scroll through the scene list
        subItem->setClickListener([this, subItem, stateItem](u64 keys) {
            if (keys & HidNpadButton_Right) { m_idx = (m_idx + 1) % SCENE_COUNT; }
            else if (keys & HidNpadButton_Left) { m_idx = (m_idx - 1 + SCENE_COUNT) % SCENE_COUNT; }
            else { return false; }
            
            // Update UI elements dynamically
            subItem->setValue(SCENES[m_idx].name);
            stateItem->setValue(STATE_NAMES[SCENES[m_idx].state]);
            return false;
        });

        list->addItem(subItem);
        list->addItem(stateItem);

        // Execute the jump
        auto* apply = new tsl::elm::ListItem(">>> JUMP TO SCENE <<<");
        apply->setClickListener([this](u64 keys) {
            if (keys & HidNpadButton_A) {
                SendStateCommand(SCENES[m_idx].state, SCENES[m_idx].sub);
                return true;
            }
            return false;
        });
        list->addItem(apply);

        frame->setContent(list);
        return frame;
    }
};

// Main Tesla Menu
class MainGui : public tsl::Gui {
public:
    virtual tsl::elm::Element* createUI() override {
        auto* frame = new tsl::elm::OverlayFrame("Diva Tesla Menu", "Switch Port v2.0");
        auto* list = new tsl::elm::List();

        // Button 1: Scene Switcher Menu
        auto* b1 = new tsl::elm::ListItem("Scene Selector");
        b1->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                tsl::changeTo<SmartSelectorGui>();
                return true;
            }
            return false;
        });
        list->addItem(b1);

        // Button 2: Toggle ImGui Window
        auto* b2 = new tsl::elm::ListItem("Toggle Motion ImGui");
        b2->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                SendToggleCommand("sdmc:/DMLSwitchPort/imgui_toggle.bin");
                return true; // Return true to indicate the click was handled
            }
            return false;
        });
        list->addItem(b2);

        // Button 3: Toggle Gamepad Overlay
        auto* b3 = new tsl::elm::ListItem("Toggle Input Overlay");
        b3->setClickListener([](u64 keys) {
            if (keys & HidNpadButton_A) {
                SendToggleCommand("sdmc:/DMLSwitchPort/overlay_toggle.bin");
                return true;
            }
            return false;
        });
        list->addItem(b3);

        frame->setContent(list);
        return frame;
    }
};

// =========================================================
// OVERLAY CLASS (Initialization & Lifecycle)
// =========================================================
class DivaDebugOverlay : public tsl::Overlay {
public:
    // Mount the SD card so we can write our signal files for the game
    virtual void initServices() override { fsdevMountSdmc(); }
    virtual void exitServices() override { fsdevUnmountDevice("sdmc"); }
    virtual void onShow() override {}
    virtual void onHide() override {}
    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<MainGui>();
    }
};

int main(int argc, char **argv) {
    return tsl::loop<DivaDebugOverlay>(argc, argv);
}
