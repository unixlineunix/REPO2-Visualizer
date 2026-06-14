#include "../../headers/Core - extra/RSTD_ HF - Ex"
#include "../../headers/Utility/reAnsi" // Pull in your rewritten TUI header

int main() {
    // Isolate shorthand reference to your new deeply nested TUI namespace
    namespace tui = rstd::ansi::CnsChr;

    // 1. Fetch your user profile and say hello at the very beginning via your new panel
    rstd::aString username = rstd::getenv("USER");
    tui::RTUI::Ifp((rstd::aString("Hello ") + rstd::aString(username) + "!").c_str());

    // 2. Instantiate your dynamic, assembly-backed flat structure container
    rstd::aString telemetry;

    // ==============================================================================
    // THE INFINITE CHAIN REACTION (Zero-Dependency Linux Stack Analysis)
    // ==============================================================================
    telemetry.append(tui::STYLE::BOLD)
             .append(tui::COLOR::LIGHTMAGENTA)
             .append("======================================================================\n")
             .append("               ABSURDLY LONG DYNAMIC ENVIRONMENT TELEMETRY            \n")
             .append("======================================================================\n")
             .append(tui::RESET)

             .append(tui::COLOR::CYAN)
             .append("[CORE SESSION]\n")
             .append(tui::RESET)
             .append("  $USER             = ").append(rstd::getenv("USER").c_str()).append("\n")
             .append("  $SHELL            = ").append(rstd::getenv("SHELL").c_str()).append("\n")
             .append("  $XDG_SESSION_TYPE = ").append(rstd::getenv("XDG_SESSION_TYPE").c_str()).append("\n")

             .append(tui::COLOR::MAGENTA)
             .append("[GRAPHICS NODE]\n")
             .append(tui::RESET)
             .append("  $DISPLAY               = ").append(rstd::getenv("DISPLAY").c_str()).append("\n")
             .append("  $WAYLAND_DISPLAY       = ").append(rstd::getenv("WAYLAND_DISPLAY").c_str()).append("\n")
             .append("  $XDG_CURRENT_DESKTOP   = ").append(rstd::getenv("XDG_CURRENT_DESKTOP").c_str()).append("\n")

             .append(tui::COLOR::GREEN)
             .append("[SYSTEM PATHS]\n")
             .append(tui::RESET)
             .append("  $PATH = ").append(rstd::getenv("PATH").c_str()).append("\n")

             .append(tui::COLOR::YELLOW)
             .append("[LOCALE & KERNEL]\n")
             .append(tui::RESET)
             .append("  $LANG = ").append(rstd::getenv("LANG").c_str()).append("\n")
             .append("  $PWD  = ").append(rstd::getenv("PWD").c_str()).append("\n")
             .append("  $TERM = ").append(rstd::getenv("TERM").c_str()).append("\n")

             .append(tui::COLOR::RED)
             .append("[DRIVER RUNTIMES]\n")
             .append(tui::RESET)
             .append("  $VK_INSTANCE_LAYERS          = ").append(rstd::getenv("VK_INSTANCE_LAYERS").c_str()).append("\n")
             .append("  $MESA_LOADER_DRIVER_OVERRIDE = ").append(rstd::getenv("MESA_LOADER_DRIVER_OVERRIDE").c_str()).append("\n")
             .append("  $__GLX_VENDOR_LIBRARY_NAME   = ").append(rstd::getenv("__GLX_VENDOR_LIBRARY_NAME").c_str()).append("\n")

             .append(tui::COLOR::LIGHTBLUE)
             .append("[SECURITY STORAGE]\n")
             .append(tui::RESET)
             .append("  $HOME           = ").append(rstd::getenv("HOME").c_str()).append("\n")
             .append("  $XDG_RUNTIME_DIR = ").append(rstd::getenv("XDG_RUNTIME_DIR").c_str()).append("\n")

             .append(tui::STYLE::BOLD)
             .append(tui::COLOR::LIGHTGREEN)
             .append("======================================================================\n")
             .append("                   METRICS OVERVIEW GENERATED IN REAL-TIME            \n")
             .append("======================================================================\n")
             .append(tui::RESET)
             .append("Absolute Text Footprint Size: ")
             .appendInt(telemetry.length())
             .append(" Bytes allocated dynamically on the system heap.\n");

    // ==============================================================================
    // OUTPUT STAGE
    // ==============================================================================
    
    // Dump the entire chained block directly to the screen using your custom stream
    rstd::cout << telemetry << rstd::endl;

    // Test your dynamic static-canvas color index macros via custom 256-color panel
    rstd::cout << tui::COLOR::FGCOLORn(208) << "Verifying 256-color buffer tracking..." 
               << tui::RESET << rstd::endl;

    // Test a direct customized header error log statement to verify literal stability
    tui::RTUI::Wrp("Vulkan core setup validation complete.");

    return 0;
}
