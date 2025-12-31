#include "TargetEverything.h"

#include <GWCA/Utilities/Hooker.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/GameEntities/Agent.h>

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static TargetEverything instance;
    return &instance;
}

namespace
{
    void* getIsAgentTargettableFunc = nullptr;
}

bool isTargetableOverride(const GW::Agent* agent)
{
    if (!agent) return false;
    if (const auto living = agent->GetAsAgentLiving()) {
        return true;
    }
    else if (const auto gadget = agent->GetAsAgentGadget()) {
        if (GW::Agents::GetAgentEncName(gadget)) return true;
    }
    return false;
}


void TargetEverything::Initialize(ImGuiContext* ctx, const ImGuiAllocFns allocator_fns, const HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, allocator_fns, toolbox_dll);
    
    if (const auto gwca = GetModuleHandleA("gwca.dll")) 
    {
        getIsAgentTargettableFunc = GetProcAddress(gwca, "?GetIsAgentTargettable@Agents@GW@@YA_NPBUAgent@2@@Z");
        if (getIsAgentTargettableFunc) 
        {
            GW::Hook::CreateHook((void**)&getIsAgentTargettableFunc, isTargetableOverride, nullptr);
            GW::Hook::EnableHooks(getIsAgentTargettableFunc);
        }
    }
}

void TargetEverything::SignalTerminate()
{
    ToolboxPlugin::SignalTerminate();

    if (getIsAgentTargettableFunc) 
    {
        GW::Hook::DisableHooks(getIsAgentTargettableFunc);
        GW::Hook::RemoveHook((void**)&getIsAgentTargettableFunc);
    }
}

bool TargetEverything::CanTerminate() {
    return true;
}
