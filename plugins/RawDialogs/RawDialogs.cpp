#include "RawDialogs.h"

#include <GWCA/GWCA.h>
#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Utilities/Hooker.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Utilities/Scanner.h>

#include "PluginUtils.h"
#include "ImGuiCppWrapper.h"

namespace CtoS 
{
    typedef void(__cdecl* SendPacket_pt)(uint32_t context, uint32_t size, void* packet);
    SendPacket_pt SendPacket_Func = 0;
    SendPacket_pt RetSendPacket = 0;

    uintptr_t game_srv_object_addr;

    bool isInitialized()
    {
        return SendPacket_Func && game_srv_object_addr;
    }

    bool SendPacket(uint32_t size, void* buffer)
    {
        if (!isInitialized()) 
            return false;

        if (GW::GameThread::IsInGameThread() || GW::Render::GetIsInRenderLoop()) {
            // Already in game thread, don't need to worry about buffer lifecycle
            SendPacket_Func(*(uint32_t*)game_srv_object_addr, size, buffer);
            return true;
        }
        // Copy the packet and enqueue in the game thread
        void* buffer_cpy = malloc(size);
        GWCA_ASSERT(buffer_cpy != NULL);
        memcpy(buffer_cpy, buffer, size);
        GW::GameThread::Enqueue([buffer_cpy, size]() {
            SendPacket_Func(*(uint32_t*)game_srv_object_addr, size, buffer_cpy);
            free(buffer_cpy);
        });
        return true;
    }

    bool SendPacket(uint32_t size, ...)
    {
        uint32_t* pak = &size + 1;
        return SendPacket(size, pak);
    }

    void Init()
    {
        SendPacket_Func = (SendPacket_pt)GW::Scanner::FindAssertion("P:\\Code\\Net\\Msg\\MsgConn.cpp", "bytes >= sizeof(dword)", 0, -0x67);
        uintptr_t address = GW::Scanner::FindAssertion("P:\\Code\\Gw\\Net\\Cli\\GcGameCmd.cpp", "No valid case for switch variable 'code'", 0, -0x32);
        if (address)
            game_srv_object_addr = *(uintptr_t*)address;

        if (!isInitialized())
            PluginUtils::logMessage("Error: CtoS function not found. /openchest will not work.", "RawDialogs");
    }
}

namespace {
    GW::HookEntry OnSentChat_HookEntry;

    typedef void (*SendDialog_pt)(uint32_t dialog_id);
    SendDialog_pt SendAgentDialog_Func = 0;

    void sendDialog(DWORD dialogId)
    {
        if (SendAgentDialog_Func) 
            SendAgentDialog_Func(dialogId);
        else if (CtoS::isInitialized())
        {
            #define GAME_CMSG_SEND_DIALOG (0x003A)
            CtoS::SendPacket(0x8, GAME_CMSG_SEND_DIALOG, dialogId);
        }
    }
    void openChest()
    {
        #define GAME_CMSG_INTERACT_GADGET (0x0050)
        #define GAME_CMSG_SEND_SIGNPOST_DIALOG (0x0052)

        const auto target = GW::Agents::GetTarget();
        if (!target || !target->GetIsGadgetType()) return;

        CtoS::SendPacket(0xC, GAME_CMSG_INTERACT_GADGET, target->agent_id, 0);
        CtoS::SendPacket(0x8, GAME_CMSG_SEND_SIGNPOST_DIALOG, 0x2);
    }

    std::string WStringToString(const std::wstring_view str)
    {
        // @Cleanup: ASSERT used incorrectly here; value passed could be from anywhere!
        if (str.empty()) {
            return "";
        }
        // NB: GW uses code page 0 (CP_ACP)
        const int try_code_pages[] = {CP_UTF8, CP_ACP};
        for (auto cp : try_code_pages) {
            const auto size_needed = WideCharToMultiByte(cp, WC_ERR_INVALID_CHARS, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr);
            if (!size_needed) continue;
            std::string dest(size_needed, 0);
            WideCharToMultiByte(cp, 0, str.data(), static_cast<int>(str.size()), dest.data(), size_needed, nullptr, nullptr);
            return dest;
        }
        return {};
    }

    void OnSendChat(GW::HookStatus* status, GW::UI::UIMessage message_id, void* wparam, void*)
    {
        constexpr auto rawDialogStart = "/rawdialog ";
        constexpr auto rawDialogStartLength = std::string_view(rawDialogStart).size();
        constexpr auto openChestStart = "/openchest";

        if (message_id != GW::UI::UIMessage::kSendChatMessage) return;
        const auto wmessage = static_cast<GW::UI::UIPacket::kSendChatMessage*>(wparam)->message;
        if (!(wmessage && *wmessage)) return;
        const auto channel = GW::Chat::GetChannel(*wmessage);
        if (channel != GW::Chat::CHANNEL_COMMAND || status->blocked) return;
        
        const auto message = WStringToString(wmessage);
        if (message.starts_with(rawDialogStart)) {
            const auto dialogString = message.substr(rawDialogStartLength);
            if (dialogString.empty()) return;
            const auto dialogId = std::stoi(dialogString, nullptr, 0);
            status->blocked = true;
            sendDialog(dialogId);
        }
        else if (message.starts_with(openChestStart))
        {
            const auto plugin = static_cast<RawDialogs*>(ToolboxPluginInstance());
            if (plugin && plugin->useCtos) {
                status->blocked = true;
                openChest();
            }
        }
    }
} // namespace

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static RawDialogs instance;
    return &instance;
}

void RawDialogs::LoadSettings(const wchar_t* folder)
{
    ToolboxPlugin::LoadSettings(folder);
    ini.LoadFile(GetSettingFile(folder).c_str());
    //useCtos = ini.GetBoolValue(Name(), VAR_NAME(useCtos), false);

    if (useCtos) 
        CtoS::Init();
}

void RawDialogs::SaveSettings(const wchar_t* folder)
{
    ToolboxPlugin::SaveSettings(folder);
    ini.SetBoolValue(Name(), VAR_NAME(useCtos), useCtos);
    PLUGIN_ASSERT(ini.SaveFile(GetSettingFile(folder).c_str()) == SI_OK);
}

void RawDialogs::DrawSettings()
{
    ToolboxPlugin::DrawSettings();

    ImGui::Text("Example usage:");
    ImGui::Bullet();
    ImGui::Text("Send dialog in decimal notation: /rawdialog 8416257");
    ImGui::Bullet();
    ImGui::Text("Send dialog in hexadecimal notation: /rawdialog 0x806501");
    //ImGui::Bullet();
    //ImGui::Text("Open chest at range: /openchest");
    
    //ImGui::Checkbox("Enable /openchest", &useCtos);
    if (useCtos && !CtoS::isInitialized()) 
        CtoS::Init();
    ImGui::SameLine();
    ImGui::ShowHelp("Flags your account, use at your own risk.");
    
    ImGui::Text("Version 1.1.4. For new releases, feature requests and bug reports check out");
    ImGui::SameLine();

    constexpr auto discordInviteLink = "https://discord.gg/ZpKzer4dK9";
    ImGui::TextColored(ImColor{102, 187, 238, 255}, discordInviteLink);
    if (ImGui::IsItemClicked()) {
        ShellExecute(nullptr, "open", discordInviteLink, nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void RawDialogs::Initialize(ImGuiContext* ctx, ImGuiAllocFns allocator_fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, allocator_fns, toolbox_dll);
    GW::UI::RegisterUIMessageCallback(&OnSentChat_HookEntry, GW::UI::UIMessage::kSendChatMessage, OnSendChat);
    
    const auto address = GW::Scanner::Find("\x89\x4b\x24\x8b\x4b\x28\x83\xe9\x00", "xxxxxxxxx");
    if (GW::Scanner::IsValidPtr(address, GW::ScannerSection::Section_TEXT)) 
    {
        SendAgentDialog_Func = (SendDialog_pt)GW::Scanner::FunctionFromNearCall(address + 0x15);
    }
}

void RawDialogs::SignalTerminate()
{
    ToolboxPlugin::SignalTerminate();
    GW::UI::RemoveUIMessageCallback(&OnSentChat_HookEntry, GW::UI::UIMessage::kSendChatMessage);
}
