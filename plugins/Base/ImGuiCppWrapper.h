#pragma once 

#include <string>
#include <imgui.h>

namespace ImGui {
    bool InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);
    bool ColorPalette(const char* label, size_t* palette_index, const ImVec4* palette, const size_t count, const size_t max_per_line, const ImGuiColorEditFlags flags);
    void ShowHelp(const char* help);
}
