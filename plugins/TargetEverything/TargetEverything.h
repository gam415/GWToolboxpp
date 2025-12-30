#pragma once

#include <ToolboxPlugin.h>

class TargetEverything : public ToolboxPlugin {
public:
    TargetEverything() = default;
    ~TargetEverything() override = default;

    const char* Name() const override { return "Target Everything"; }
    [[nodiscard]] bool HasSettings() const override { return false; }
    void Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
};
