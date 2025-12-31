#pragma once

#include <ToolboxUIPlugin.h>

#include <IconsFontAwesome5.h>

class RawDialogs : public ToolboxPlugin {
public:
    RawDialogs()
    {
    }
    ~RawDialogs() override = default;

    const char* Name() const override { return "RawDialog"; }
    const char* Icon() const override { return ICON_FA_DICE; }

    void DrawSettings() override;
    bool HasSettings() const override { return true; }
    void LoadSettings(const wchar_t*) override;
    void SaveSettings(const wchar_t*) override;

    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;

    bool useCtos = false;
};
