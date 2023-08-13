#include "editor_core.h"

namespace Electron {
    Timeline::Timeline() {
        
    }

    void Timeline::Render(AppInstance* instance) {
        bool pOpen = true;
        ImGui::Begin(ELECTRON_GET_LOCALIZATION(instance, "TIMELINE_TITLE"), &pOpen, 0);
        if (!pOpen) {
            ImGui::End();
            throw ElectronSignal_CloseWindow;
        }

        static float zoom = TIMELINE_DEFAULT_ZOOM;
        static ImVec2 regionAvail = {0, 0};
        bool windowFocused = ImGui::IsWindowFocused();
        regionAvail = ImGui::GetContentRegionAvail();

        /* Zoom control */ {
            if (ImGui::Button("-")) {
                zoom -= 0.5f;
                zoom = std::clamp(zoom, TIMELINE_MIN_ZOOM, TIMELINE_MAX_ZOOM);
            }
            ImGui::SameLine();

            ImGui::PushItemWidth(regionAvail.x * 0.3f);
                ImGui::SliderFloat("##TimelineZoomControl", &zoom, TIMELINE_MIN_ZOOM, TIMELINE_MAX_ZOOM, "%0.1f", 0);
            ImGui::PopItemWidth();
            ImGui::SameLine();

            if (ImGui::Button("+")) {
                zoom += 0.5f;
            }
            ImGui::SameLine();
            ImGui::Text(ELECTRON_GET_LOCALIZATION(instance, "TIMELINE_ZOOM"));
        }
        ImGui::Separator();
        ImGui::End();
    }
}