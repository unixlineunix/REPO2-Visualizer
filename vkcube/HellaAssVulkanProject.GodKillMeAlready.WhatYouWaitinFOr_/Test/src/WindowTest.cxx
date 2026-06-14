#include "../../headers/Core/s_WindowProvider_idk"
#include "../../headers/Core - extra/RSTD_ HF - Ex"
#include "../../headers/Utility/reAnsi"

int main() {
    namespace tui = rstd::ansi::CnsChr;

    tui::RTUI::Ifp("Initializing window...");
    WP::wWindow_s window(800, 600);
    window.init();

    if (!window.getDisplay()) {
        tui::RTUI::Erp("Failed to connect to Wayland display.");
        tui::RTUI::Erp("This application requires a Wayland compositor.");
        tui::RTUI::Erp("Ensure WAYLAND_DISPLAY is set and a compositor is running.");
        tui::RTUI::Erp("Running from TTY or non-Wayland sessions is not supported.");
        return 1;
    }

    tui::RTUI::Ifp("Wayland active. Press ESC to exit, SPACE for fullscreen, M for mouse info.");

    while (window.isRunning()) {
        window.run();
        if (!window.isRunning()) break;

        if (window.getKeyState(IDK::Key::ESCAPE) == IDK::InputState::PRESSED) break;

        static bool fs = false;
        if (window.getKeyState(IDK::Key::SPACE) == IDK::InputState::PRESSED) {
            fs = !fs;
            window.fullscreen(fs);
            tui::RTUI::Ifp(fs ? "Fullscreen enabled" : "Fullscreen disabled");
        }

        if (window.getKeyState(IDK::Key::M) == IDK::InputState::PRESSED) {
            double mx, my;
            window.getMousePosition(mx, my);
            rstd::cout << tui::COLOR::LIGHTYELLOW << "[Mouse::] Pos: "
                       << (long long)mx << ", " << (long long)my
                       << " | Focused: " << (window.WindowFocusState() ? "YES" : "NO")
                       << tui::RESET << rstd::endl;
        }

        if (window.getMouseState(IDK::MouseButton::LEFT) == IDK::InputState::PRESSED) {
            tui::RTUI::Ifp("Left Click!");
        }

        window.updateInputStates();
    }

    window.cleanup();
    tui::RTUI::Ifp("Test finished.");
    return 0;
}