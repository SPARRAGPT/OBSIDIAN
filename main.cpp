#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <filesystem>
#include <windows.h>
#include <d3d11.h>
#include <string>

#include <dxgi.h>
#include <cstdio>
#include <shellapi.h>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

// ============================================================
// OBSIDIAN - MINECRAFT EDITION
// Premium black / purple neon interface
// Developed by Sparra & ChatGPT
// ============================================================

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static ID3D11ShaderResourceView* g_quartzTexture = nullptr;
static bool LoadQuartzTexture()
{
    if (!g_pd3dDevice)
        return false;

    int width = 0;
    int height = 0;
    int channels = 0;

    wchar_t exePath[MAX_PATH] = {};

    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    wchar_t* lastSlash = wcsrchr(exePath, L'\\');

    if (lastSlash)
        *(lastSlash + 1) = L'\0';

    wcscat_s(exePath, MAX_PATH, L"quartz.png");

    char imagePathString[MAX_PATH] = {};
    WideCharToMultiByte(
        CP_ACP, 0, exePath, -1,
        imagePathString, MAX_PATH, nullptr, nullptr
    );

    unsigned char* pixels = stbi_load(
        imagePathString,
        &width,
        &height,
        &channels,
        4
    );
    if (!pixels)
        return false;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = pixels;
    data.SysMemPitch = width * 4;

    ID3D11Texture2D* texture = nullptr;

    HRESULT hr = g_pd3dDevice->CreateTexture2D(
        &desc, &data, &texture
    );

    if (SUCCEEDED(hr))
    {
        hr = g_pd3dDevice->CreateShaderResourceView(
            texture, nullptr, &g_quartzTexture
        );
    }

    if (texture)
        texture->Release();

    stbi_image_free(pixels);

    return SUCCEEDED(hr) && g_quartzTexture != nullptr;
}

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam
);

LRESULT WINAPI WndProc(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam
);

// Pages
enum Page {
    PAGE_DASHBOARD,
    PAGE_MINECRAFT,
    PAGE_PERFORMANCE,
    PAGE_MODS,
    PAGE_SETTINGS,
    PAGE_ABOUT
};

constexpr int PAGE_COUNT = 6;

static int currentPage = PAGE_DASHBOARD;
// Performance dashboard history
static float cpuHistory[100] = {};
static int cpuHistoryIndex = 0;
static float cpuHistoryTimer = 0.0f;
static float animatedCPU = 0.0f;
static float animatedRAM = 0.0f;

// Page transition animation
static float pageFade = 1.0f;
static int previousPage = PAGE_DASHBOARD;
static float pageTransitionTime = 0.0f;

static int accentChoice = 0;
static bool compactSidebar = false;

// Purple, pink, blue and green accent themes.
static const char* accentNames[] = {
    "Violet", "Neon Pink", "Electric Blue", "Emerald"
};

static ImVec4 accentColors[] = {
    ImVec4(0.67f, 0.29f, 1.00f, 1.00f),
    ImVec4(1.00f, 0.20f, 0.63f, 1.00f),
    ImVec4(0.25f, 0.52f, 1.00f, 1.00f),
    ImVec4(0.20f, 0.88f, 0.58f, 1.00f)
};

static const char* pageNames[] = {
    "Dashboard",
    "Minecraft",
    "Performance",
    "Mods",
    "Settings",
    "About"
};

static const char* pageDescriptions[] = {
    "Your command centre",
    "Minecraft tools and options",
    "Your system overview",
    "Personalise your experience",
    "Project information"
};

static ImVec4 Accent()
{
    return accentColors[accentChoice];
}

static ImVec4 WithAlpha(ImVec4 c, float alpha)
{
    c.w = alpha;
    return c;
}

// ============================================================
// THEME
// ============================================================

static void ApplyTheme()
{

    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 10.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;

    style.WindowPadding = ImVec2(16.0f, 14.0f);
    style.FramePadding = ImVec2(10.0f, 7.0f);
    style.ItemSpacing = ImVec2(10.0f, 9.0f);
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;

    ImVec4* colors = style.Colors;

    colors[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.025f, 0.055f, 1.0f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.055f, 0.035f, 0.085f, 1.0f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.065f, 0.040f, 0.100f, 0.98f);

    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.12f, 0.55f, 0.65f);

    colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.055f, 0.16f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.09f, 0.38f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.34f, 0.12f, 0.52f, 1.0f);

    colors[ImGuiCol_Button] = ImVec4(0.22f, 0.08f, 0.38f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.43f, 0.12f, 0.70f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.58f, 0.20f, 0.85f, 1.0f);

    colors[ImGuiCol_Header] = ImVec4(0.22f, 0.08f, 0.38f, 0.85f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.40f, 0.12f, 0.65f, 0.95f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.52f, 0.18f, 0.78f, 1.0f);

    colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.45f, 1.0f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.62f, 0.25f, 0.95f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.85f, 0.55f, 1.0f, 1.0f);

    colors[ImGuiCol_TitleBg] = ImVec4(0.045f, 0.025f, 0.075f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.045f, 0.20f, 1.0f);

    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.05f, 0.20f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.40f, 0.12f, 0.65f, 1.0f);

    colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.12f, 0.48f, 0.70f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.65f, 0.25f, 0.95f, 1.0f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.80f, 0.40f, 1.0f, 1.0f);

    colors[ImGuiCol_Text] = ImVec4(0.94f, 0.91f, 1.0f, 1.0f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.48f, 0.65f, 1.0f);

}

// ============================================================
// REUSABLE UI COMPONENTS
// ============================================================

static void AccentText(const char* text)
{
    ImGui::TextColored(Accent(), "%s", text);
}

static void SectionHeading(
    const char* title,
    const char* subtitle = nullptr)
{
    AccentText(title);

    if (subtitle)
    {
        ImGui::SameLine();
        ImGui::TextDisabled(" / %s", subtitle);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

static bool ActionButton(
    const char* label,
    float width = 180.0f,
    float height = 40.0f)
{
    return ImGui::Button(label, ImVec2(width, height));
}

static void DrawCard(
    const char* id,
    const char* eyebrow,
    const char* title,
    const char* description,
    float height = 130.0f)
{
    ImGui::PushStyleColor(
        ImGuiCol_ChildBg,
        ImVec4(0.055f, 0.038f, 0.085f, 1.0f)
    );

    ImGui::PushStyleColor(
        ImGuiCol_Border,
        ImVec4(
            Accent().x * 0.42f,
            Accent().y * 0.30f,
            Accent().z * 0.42f,
            0.75f
        )
    );

    ImGui::BeginChild(
        id, ImVec2(0, height), true
    );

    AccentText(eyebrow);
    ImGui::Spacing();

    ImGui::Text("%s", title);
    ImGui::Spacing();

    ImGui::PushTextWrapPos(
        ImGui::GetCursorPosX() +
        ImGui::GetContentRegionAvail().x
    );
    ImGui::TextDisabled("%s", description);
    ImGui::PopTextWrapPos();

    ImGui::EndChild();

    ImGui::PopStyleColor(2);
}


static bool NavigationButton(int index, const char* label)
{
    bool selected = (currentPage == index);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    float height = 43.0f;

    // Detect whether the mouse is over this button
    bool hovered = ImGui::IsMouseHoveringRect(
        pos,
        ImVec2(pos.x + width, pos.y + height)
    );

    float pulse = 0.5f + 0.5f * sinf(
        (float)ImGui::GetTime() * 4.0f
    );

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Draw the purple glow behind the button
    if (hovered || selected)
    {
        int alpha = hovered
            ? (int)(35 + pulse * 45)
            : 35;

        draw->AddRectFilled(
            ImVec2(pos.x - 3.0f, pos.y - 3.0f),
            ImVec2(pos.x + width + 3.0f, pos.y + height + 3.0f),
            IM_COL32(150, 35, 255, alpha),
            9.0f
        );
    }

    // Button appearance
    ImGui::PushStyleColor(
        ImGuiCol_Button,
        selected
        ? ImVec4(0.30f, 0.10f, 0.48f, 1.0f)
        : ImVec4(0.09f, 0.045f, 0.15f, 1.0f)
    );

    ImGui::PushStyleColor(
        ImGuiCol_ButtonHovered,
        ImVec4(
            0.38f + pulse * 0.12f,
            0.10f,
            0.62f + pulse * 0.12f,
            1.0f
        )
    );

    ImGui::PushStyleColor(
        ImGuiCol_ButtonActive,
        ImVec4(0.50f, 0.18f, 0.78f, 1.0f)
    );

    ImGui::PushStyleColor(
        ImGuiCol_Border,
        selected || hovered
        ? ImVec4(0.65f, 0.30f, 1.0f, 1.0f)
        : ImVec4(0.20f, 0.10f, 0.30f, 0.5f)
    );

    ImGui::PushStyleVar(
        ImGuiStyleVar_FrameBorderSize,
        selected || hovered ? 1.0f : 0.0f
    );

    bool clicked = ImGui::Button(
        label, ImVec2(-1.0f, height)
    );

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    return clicked;
}

// ============================================================
// DASHBOARD
// ============================================================

static void DrawDashboard()
{
    AccentText("OBSIDIAN / HOME");

    ImGui::Spacing();
    ImGui::Text(
        "Welcome back."
    );

    ImGui::TextDisabled(
        "Your Minecraft companion, built your way."
    );

    ImGui::Spacing();
    ImGui::Spacing();

    // Main hero panel
    ImGui::PushStyleColor(
        ImGuiCol_ChildBg,
        ImVec4(0.065f, 0.035f, 0.105f, 1.0f)
    );

    ImGui::PushStyleColor(
        ImGuiCol_Border,
        WithAlpha(Accent(), 0.80f)
    );

    ImGui::BeginChild(
        "HeroPanel", ImVec2(0, 178), true
    );

    ImGui::Spacing();

    ImGui::TextDisabled("YOUR DESKTOP COMPANION");

    ImGui::Spacing();

    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(
        Accent(), "OBSIDIAN"
    );
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Text("M I N E C R A F T   E D I T I O N");

    ImGui::Spacing();

    ImGui::TextDisabled(
        "A lightweight utility interface."
    );

    ImGui::Spacing();

    if (ActionButton("Explore Minecraft", 190.0f, 36.0f))
        currentPage = PAGE_MINECRAFT;

    ImGui::EndChild();

    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    SectionHeading("OVERVIEW", "Quick access");

    // Three dashboard cards
    float avail = ImGui::GetContentRegionAvail().x;
    float gap = ImGui::GetStyle().ItemSpacing.x;
    float cardWidth = (avail - gap * 2.0f) / 3.0f;

    if (cardWidth < 100.0f)
        cardWidth = 100.0f;

    ImGui::BeginGroup();

    DrawCard(
        "DashMinecraft",
        "01 / GAME",
        "Minecraft",
        "Future game tools and options.",
        142.0f
    );

    ImGui::EndGroup();

    ImGui::SameLine(0.0f, gap);

    ImGui::BeginGroup();

    DrawCard(
        "DashPerformance",
        "02 / SYSTEM",
        "Performance",
        "Review current memory usage.",
        142.0f
    );

    ImGui::EndGroup();

    ImGui::SameLine(0.0f, gap);

    ImGui::BeginGroup();

    DrawCard(
        "DashSettings",
        "03 / PERSONAL",
        "Settings",
        "Choose your accent colour.",
        142.0f
    );

    ImGui::EndGroup();

    ImGui::Spacing();

    // Actual navigation controls for the cards
    if (ActionButton("Open Minecraft", cardWidth, 34.0f))
        currentPage = PAGE_MINECRAFT;

    ImGui::SameLine();

    if (ActionButton("View Performance", cardWidth, 34.0f))
        currentPage = PAGE_PERFORMANCE;

    ImGui::SameLine();

    if (ActionButton("Customise", cardWidth, 34.0f))
        currentPage = PAGE_SETTINGS;

    ImGui::Spacing();
    ImGui::TextDisabled(
        "OBSIDIAN 0.2  |  Developed by Sparra & ChatGPT"
    );
}

// ============================================================
// MINECRAFT PAGE
// ============================================================


static void DrawMinecraft()
{
    SectionHeading(
        "MINECRAFT",
        "Game tools"
    );

    const wchar_t* minecraftPath =
        LR"(C:\Users\jacko\OneDrive\Desktop\levilauncher.exe\versions\1.26.52.03\Minecraft.Windows.exe)";

    const DWORD attributes = GetFileAttributesW(minecraftPath);
    const bool minecraftFound =
        attributes != INVALID_FILE_ATTRIBUTES &&
        !(attributes & FILE_ATTRIBUTE_DIRECTORY);

    ImGui::Spacing();

    if (minecraftFound)
    {
        ImGui::TextColored(
            ImVec4(0.3f, 1.0f, 0.4f, 1.0f),
            "Minecraft Bedrock: DETECTED"
        );
    }
    else
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
            "Minecraft Bedrock: NOT FOUND"
        );
    }

    ImGui::Text("Your Minecraft workspace.");
    ImGui::TextDisabled(
        "Launch and manage your Minecraft setup."
    );

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("GAME LAUNCHER");
    ImGui::Spacing();

    ImGui::TextWrapped(
        "Launch Minecraft Bedrock using your launcher."
    );

    ImGui::Spacing();

    if (ActionButton("Launch Minecraft"))
    {
        const wchar_t* minecraftPath =
            LR"(C:\Users\jacko\OneDrive\Desktop\levilauncher.exe\versions\1.26.52.03\Minecraft.Windows.exe)";

        const HINSTANCE result = ShellExecuteW(
            nullptr,
            L"open",
            minecraftPath,
            nullptr,
            nullptr,
            SW_SHOWNORMAL
        );

        if (reinterpret_cast<INT_PTR>(result) <= 32)
        {
            ImGui::OpenPopup("Launch Error");
        }
    }

    if (ImGui::BeginPopup("Launch Error"))
    {
        ImGui::TextWrapped(
            "Windows could not launch Minecraft from this path."
        );
        ImGui::TextWrapped(
            "Check the executable location and launcher requirements."
        );

        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    ImGui::Spacing();

    ImGui::TextDisabled(
        "Launcher integration is not configured yet."
    );

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ActionButton("Back to Dashboard"))
        currentPage = PAGE_DASHBOARD;
}

// ============================================================
// PERFORMANCE PAGE
// ============================================================
static double GetCPUUsage()
{
    static ULARGE_INTEGER lastIdle = {};
    static ULARGE_INTEGER lastKernel = {};
    static ULARGE_INTEGER lastUser = {};
    static bool firstRun = true;

    FILETIME idleTime, kernelTime, userTime;

    if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
        return 0.0;

    ULARGE_INTEGER idle = {};
    ULARGE_INTEGER kernel = {};
    ULARGE_INTEGER user = {};

    idle.LowPart = idleTime.dwLowDateTime;
    idle.HighPart = idleTime.dwHighDateTime;

    kernel.LowPart = kernelTime.dwLowDateTime;
    kernel.HighPart = kernelTime.dwHighDateTime;

    user.LowPart = userTime.dwLowDateTime;
    user.HighPart = userTime.dwHighDateTime;

    if (firstRun)
    {
        lastIdle = idle;
        lastKernel = kernel;
        lastUser = user;
        firstRun = false;
        return 0.0;
    }

    ULONGLONG idleDelta = idle.QuadPart - lastIdle.QuadPart;
    ULONGLONG kernelDelta = kernel.QuadPart - lastKernel.QuadPart;
    ULONGLONG userDelta = user.QuadPart - lastUser.QuadPart;

    lastIdle = idle;
    lastKernel = kernel;
    lastUser = user;

    ULONGLONG total = kernelDelta + userDelta;

    if (total == 0)
        return 0.0;

    double usage =
        100.0 * (total - idleDelta) / total;

    if (usage < 0.0) usage = 0.0;
    if (usage > 100.0) usage = 100.0;

    return usage;
}
static void DrawPerformance()
{
    SectionHeading(
        "PERFORMANCE",
        "System overview"
    );

    ImGui::TextDisabled(
        "Live Windows memory information"
    );

    ImGui::Spacing();

    MEMORYSTATUSEX memory = {};
    memory.dwLength = sizeof(memory);

    if (GlobalMemoryStatusEx(&memory))
    {
        double totalGB =
            static_cast<double>(memory.ullTotalPhys) /
            (1024.0 * 1024.0 * 1024.0);

        double usedGB =
            static_cast<double>(
                memory.ullTotalPhys - memory.ullAvailPhys
                ) / (1024.0 * 1024.0 * 1024.0);

        ImGui::PushStyleColor(
            ImGuiCol_ChildBg,
            ImVec4(0.055f, 0.038f, 0.085f, 1.0f)
        );

        ImGui::BeginChild(
            "MemoryPanel", ImVec2(0, 150), true
        );

        AccentText("MEMORY");
        ImGui::Spacing();

        ImGui::Text(
            "%.1f GB used / %.1f GB total",
            usedGB, totalGB
        );

        ImGui::Spacing();

        ImGui::PushStyleColor(
            ImGuiCol_PlotHistogram,
            Accent()
        );

        ImGui::ProgressBar(
            animatedRAM / 100.0f,
            ImVec2(-1.0f, 20.0f)
        );

        ImGui::PopStyleColor();

        ImGui::TextDisabled(
            "%lu%% of physical memory in use",
            static_cast<unsigned long>(memory.dwMemoryLoad)
        );

        ImGui::EndChild();

        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::TextDisabled(
            "Unable to retrieve memory information."
        );
    }

    ImGui::Spacing();

    SYSTEM_INFO info = {};
    GetSystemInfo(&info);

    DrawCard(
        "ProcessorCard",
        "PROCESSOR",
        "Logical processors",
        "Windows reports the number of logical processors "
        "available to this application.",
        110.0f
    );

    ImGui::Text(
        "Available logical processors: %lu",
        static_cast<unsigned long>(
            info.dwNumberOfProcessors
            )
    );

    ImGui::Spacing();
    double cpuUsage = GetCPUUsage();

    // Smooth animated CPU and RAM values
    float deltaTime = ImGui::GetIO().DeltaTime;

    animatedCPU += (
        static_cast<float>(cpuUsage) - animatedCPU
        ) * deltaTime * 4.0f;
    MEMORYSTATUSEX currentMemory = {};
    currentMemory.dwLength = sizeof(currentMemory);

    if (GlobalMemoryStatusEx(&currentMemory))
    {
        animatedRAM += (
            static_cast<float>(currentMemory.dwMemoryLoad) - animatedRAM
            ) * deltaTime * 4.0f;
    }

    // Record CPU history every 0.25 seconds
    cpuHistoryTimer += deltaTime;

    if (cpuHistoryTimer >= 0.25f)
    {
        cpuHistoryTimer = 0.0f;

        cpuHistory[cpuHistoryIndex] =
            static_cast<float>(cpuUsage);

        cpuHistoryIndex = (cpuHistoryIndex + 1) % 100;
    }

    ImGui::Spacing();
    AccentText("CPU USAGE");

    ImGui::ProgressBar(
        animatedRAM / 100.0f,
        ImVec2(-1.0f, 20.0f),
        ""
    );

    ImGui::Text(
        "System CPU usage: %.1f%%",
        animatedCPU
    );
    ImGui::Spacing();
    AccentText("LIVE CPU HISTORY");

    ImGui::PushStyleColor(
        ImGuiCol_FrameBg,
        ImVec4(0.055f, 0.038f, 0.085f, 1.0f)
    );

    ImGui::PushStyleColor(
        ImGuiCol_PlotLines,
        Accent()
    );

    ImGui::PlotLines(
        "##CPUHistory",
        cpuHistory,
        IM_ARRAYSIZE(cpuHistory),
        cpuHistoryIndex,
        nullptr,
        0.0f,
        100.0f,
        ImVec2(-1.0f, 100.0f)
    );

    ImGui::PopStyleColor(2);

    ImGui::TextDisabled(
        "CPU samples update every 0.25 seconds"
    );
}


static void DrawMods()
{
    static bool freecam = false;
    static bool fullbright = false;
    static bool fpsCounter = true;
    static bool keystrokes = false;
    static bool toggleSprint = false;
    static bool zoom = false;
    static bool customCrosshair = false;
    static bool singlePlayerFly = false;

    SectionHeading(
        "MOD MANAGER",
        "OBSIDIAN modules and features"
    );

    ImGui::Spacing();

    ImGui::TextDisabled(
        "Minecraft Bedrock | WIP BUILD"
    );

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("PLANNED FEATURES");
    ImGui::TextDisabled(
        "Freecam is being developed for single-player."
    );

    ImGui::Spacing();

    if (ImGui::BeginTable(
        "ObsidianMods",
        2,
        ImGuiTableFlags_SizingStretchSame
    ))
    {
        ImGui::TableNextColumn();

        ImGui::BeginChild(
            "VisualMods",
            ImVec2(0, 225),
            true
        );

        ImGui::Text("VISUAL");
        ImGui::Separator();

        ImGui::Checkbox("Freecam (prototype)", &freecam);
        ImGui::Checkbox("Fullbright", &fullbright);
        ImGui::Checkbox("Zoom", &zoom);
        ImGui::Checkbox("Custom crosshair", &customCrosshair);

        ImGui::EndChild();

        ImGui::TableNextColumn();

        ImGui::BeginChild(
            "UtilityMods",
            ImVec2(0, 225),
            true
        );

        ImGui::Text("UTILITY");
        ImGui::Separator();

        ImGui::Checkbox("FPS counter", &fpsCounter);
        ImGui::Checkbox("Keystrokes", &keystrokes);
        ImGui::Checkbox("Toggle sprint", &toggleSprint);
        ImGui::Checkbox("Single-player fly", &singlePlayerFly);

        ImGui::EndChild();

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("MODULE STATUS");

    if (freecam)
    {
        ImGui::TextColored(
            ImVec4(1.0f, 0.65f, 0.2f, 1.0f),
            "Freecam: prototype enabled"
        );
    }
    else
    {
        ImGui::TextDisabled(
            "Freecam: disabled"
        );
    }

    ImGui::TextDisabled(
        "Minecraft camera integration: not connected"
    );

    ImGui::TextDisabled(
        "Prototype switches reset when the app closes."
    );
}

// ============================================================
// SETTINGS PAGE
// ============================================================

static void DrawSettings()
{
    SectionHeading(
        "SETTINGS",
        "Personalisation"
    );

    ImGui::Text(
        "Make OBSIDIAN yours."
    );

    ImGui::TextDisabled(
        "Choose the accent colour used across the interface."
    );

    ImGui::Spacing();

    ImGui::PushStyleColor(
        ImGuiCol_ChildBg,
        ImVec4(0.055f, 0.038f, 0.085f, 1.0f)
    );

    ImGui::BeginChild(
        "AppearancePanel", ImVec2(0, 190), true
    );

    AccentText("APPEARANCE");

    ImGui::Spacing();

    ImGui::Text("Accent colour");

    if (ImGui::BeginCombo(
        "##AccentCombo",
        accentNames[accentChoice]))
    {
        for (int i = 0; i < IM_ARRAYSIZE(accentNames); ++i)
        {
            bool selected = (accentChoice == i);

            if (ImGui::Selectable(
                accentNames[i], selected))
            {
                accentChoice = i;
                ApplyTheme();
            }

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    ImGui::Spacing();

    ImGui::Text("Preview");

    ImGui::SameLine();

    ImGui::TextColored(
        Accent(), "NEON ACCENT ACTIVE"
    );

    ImGui::Spacing();

    ImGui::TextDisabled(
        "Your selection applies immediately."
    );

    ImGui::EndChild();

    ImGui::PopStyleColor();

    ImGui::Spacing();

    if (ActionButton("Reset to Violet"))
    {
        accentChoice = 0;
        ApplyTheme();
    }

    ImGui::SameLine();

    if (ActionButton("Back to Dashboard"))
        currentPage = PAGE_DASHBOARD;
}

// ============================================================
// ABOUT PAGE
// ============================================================

static void DrawAbout()
{
    SectionHeading(
        "ABOUT",
        "Project details"
    );

    ImGui::Spacing();

    ImGui::PushStyleColor(
        ImGuiCol_ChildBg,
        ImVec4(0.065f, 0.035f, 0.105f, 1.0f)
    );

    ImGui::PushStyleColor(
        ImGuiCol_Border,
        WithAlpha(Accent(), 0.75f)
    );

    ImGui::BeginChild(
        "AboutHero", ImVec2(0, 170), true
    );

    ImGui::Spacing();

    ImGui::SetWindowFontScale(1.5f);
    ImGui::TextColored(Accent(), "OBSIDIAN");
    ImGui::SetWindowFontScale(1.0f);

    ImGui::Text("MINECRAFT EDITION");

    ImGui::Spacing();

    ImGui::TextDisabled(
        "Version 0.2 - UI prototype"
    );

    ImGui::EndChild();

    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    DrawCard(
        "AboutCredits",
        "CREDITS",
        "Developed by Sparra & ChatGPT",
        "Built using C++, Dear ImGui, Win32 and DirectX 11.",
        120.0f
    );

    ImGui::Spacing();

    DrawCard(
        "AboutMission",
        "THE PROJECT",
        "Built from scratch",
        "A lightweight Windows desktop interface designed "
        "to grow with new features over time.",
        120.0f
    );
}

// ============================================================
// MAIN INTERFACE
// ============================================================

static void DrawInterface()
{
    ImGuiIO& io = ImGui::GetIO();

    // Smooth page transition animation
    float deltaTime = ImGui::GetIO().DeltaTime;

    if (pageTransitionTime < 1.0f)
    {
        pageTransitionTime += deltaTime * 2.5f;

        if (pageTransitionTime > 1.0f)
            pageTransitionTime = 1.0f;
    }

    pageFade = pageTransitionTime;


    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGui::Begin(
        "##Root",
        nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoBringToFrontOnFocus
    );

    // Top header
   // Top header
    ImGui::BeginChild(
        "##TopHeader",
        ImVec2(0, 58),
        false
    );



    if (g_quartzTexture != nullptr)
    {
        float time = (float)ImGui::GetTime();

        // Fast, irregular flickering
        float flicker1 = 0.5f + 0.5f * sinf(time * 13.0f);
        float flicker2 = 0.5f + 0.5f * sinf(time * 19.0f + 2.0f);
        float flicker3 = 0.5f + 0.5f * sinf(time * 8.0f + 1.0f);

        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 center(pos.x + 20.0f, pos.y + 20.0f);

        ImDrawList* draw = ImGui::GetWindowDrawList();

        // Large outer purple fire glow
        draw->AddCircleFilled(
            center,
            34.0f + flicker1 * 5.0f,
            IM_COL32(100, 0, 255, (int)(45 + flicker1 * 45)),
            48
        );

        // Mid violet flame glow
        draw->AddCircleFilled(
            center,
            27.0f + flicker2 * 4.0f,
            IM_COL32(180, 20, 255, (int)(65 + flicker2 * 65)),
            48
        );

        // Bright inner purple energy
        draw->AddCircleFilled(
            center,
            21.0f + flicker3 * 2.0f,
            IM_COL32(220, 100, 255, (int)(45 + flicker3 * 45)),
            48
        );

        // Jagged tongues of purple fire around the logo
        for (int i = 0; i < 16; i++)
        {
            float angle = (6.28318f * i / 16.0f)
                + sinf(time * 3.0f + i) * 0.12f;

            float flameHeight =
                8.0f + 10.0f * (0.5f + 0.5f *
                    sinf(time * 11.0f + i * 2.1f));

            float innerRadius = 17.0f;
            float outerRadius = innerRadius + flameHeight;

            ImVec2 base(
                center.x + cosf(angle) * innerRadius,
                center.y + sinf(angle) * innerRadius
            );

            ImVec2 tip(
                center.x + cosf(angle) * outerRadius,
                center.y + sinf(angle) * outerRadius
            );

            draw->AddLine(
                base,
                tip,
                IM_COL32(190, 40, 255, 220),
                3.0f + flicker2 * 2.0f
            );

            // Hot lavender tip
            draw->AddCircleFilled(
                tip,
                2.0f + flicker1 * 1.5f,
                IM_COL32(235, 170, 255, 220),
                8
            );
        }

        // Draw Quartz on top of the fire
        ImGui::Image(
            (ImTextureID)g_quartzTexture,
            ImVec2(40.0f, 40.0f)
        );

        ImGui::SameLine();
    }

    ImGui::TextDisabled(
        "|  MINECRAFT EDITION"
    );

    ImGui::SameLine(
        ImGui::GetWindowWidth() - 150.0f
    );

    ImGui::TextDisabled("BUILD 0.2");

    ImGui::EndChild();

    ImGui::Separator();
    ImGui::Spacing();

    // Sidebar
    float sidebarWidth = compactSidebar ? 80.0f : 220.0f;

    ImGui::BeginChild(
        "##Sidebar",
        ImVec2(sidebarWidth, 0),
        true
    );

    if (compactSidebar)
    {
        if (ImGui::Button("<", ImVec2(-1, 35)))
            compactSidebar = false;
    }
    else
    {
        if (ImGui::Button("< Collapse", ImVec2(-1, 35)))
            compactSidebar = true;
    }

    ImGui::Spacing();

    if (!compactSidebar)
    {
        ImGui::TextDisabled("WORKSPACE");
        ImGui::Spacing();
    }

    for (int i = 0; i < PAGE_COUNT; ++i)
    {
        const char* label = pageNames[i];

        if (compactSidebar)
        {
            char shortLabel[3] = {};
            shortLabel[0] = label[0];
            shortLabel[1] = '\0';

            if (NavigationButton(i, shortLabel))
                currentPage = i;

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", label);
        }
        else
        {
            if (NavigationButton(i, label))
                currentPage = i;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!compactSidebar)
    {
        ImGui::TextDisabled("PROJECT STATUS");

        ImGui::Spacing();

        ImGui::TextColored(
            Accent(), "●"
        );

        ImGui::SameLine();

        ImGui::TextDisabled("Interface online");

        ImGui::Spacing();

        ImGui::TextDisabled("Created by");

        ImGui::Text("Sparra");
        ImGui::TextDisabled("& ChatGPT");
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Main content
    ImGui::BeginChild(
        "##MainContent",
        ImVec2(0, 0),
        false
    );

    ImGui::TextDisabled(
        "%s  /  %s",
        "OBSIDIAN",
        pageNames[currentPage]
    );

    ImGui::Spacing();

    // Detect when the user switches pages
    if (currentPage != previousPage)
    {
        previousPage = currentPage;
        pageTransitionTime = 0.0f;
        pageFade = 0.0f;
    }

    // Fade the page content in
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, pageFade);

    switch (currentPage)
    {
    case PAGE_DASHBOARD:
        DrawDashboard();
        break;

    case PAGE_MINECRAFT:
        DrawMinecraft();
        break;

    case PAGE_PERFORMANCE:
        DrawPerformance();
        break;

    case PAGE_SETTINGS:
        DrawSettings();
        break;

    case PAGE_MODS:
        DrawMods();
        break;

    case PAGE_ABOUT:
        DrawAbout();
        break;
    }

    ImGui::PopStyleVar();
    ImGui::EndChild();

    ImGui::End();
}

// ============================================================
// APPLICATION ENTRY
// ============================================================

int main()
{
    WNDCLASSEXW wc = {
        sizeof(WNDCLASSEXW),
        CS_CLASSDC,
        WndProc,
        0L,
        0L,
        GetModuleHandleW(nullptr),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        L"OBSIDIAN",
        nullptr
    };

    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW(
        wc.lpszClassName,
        L"OBSIDIAN - Minecraft Edition",
        WS_OVERLAPPEDWINDOW,
        100, 100, 1150, 760,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!hwnd)
    {
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        MessageBoxW(
            nullptr,
            L"Could not initialise DirectX 11.",
            L"OBSIDIAN",
            MB_OK | MB_ICONERROR
        );
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ApplyTheme();

    if (!ImGui_ImplWin32_Init(hwnd) ||
        !ImGui_ImplDX11_Init(
            g_pd3dDevice,
            g_pd3dDeviceContext))
    {
        ImGui::DestroyContext();
        CleanupDeviceD3D();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    if (!LoadQuartzTexture())
    {
        MessageBoxW(
            hwnd,
            L"Could not load quartz.png. Check the file is beside OBSIDIAN.exe.",
            L"OBSIDIAN",
            MB_OK | MB_ICONWARNING
        );
    }

    bool running = true;

    while (running)
    {
        MSG msg;

        while (PeekMessageW(
            &msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);

            if (msg.message == WM_QUIT)
                running = false;
        }

        if (!running)
            break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        DrawInterface();

        ImGui::Render();

        const float clearColor[4] = {
            0.025f, 0.015f, 0.040f, 1.0f
        };

        g_pd3dDeviceContext->OMSetRenderTargets(
            1,
            &g_mainRenderTargetView,
            nullptr
        );

        g_pd3dDeviceContext->ClearRenderTargetView(
            g_mainRenderTargetView,
            clearColor
        );

        ImGui_ImplDX11_RenderDrawData(
            ImGui::GetDrawData()
        );

        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();

    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// ============================================================
// DIRECTX 11
// ============================================================

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};

    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;

    D3D_FEATURE_LEVEL featureLevel;

    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0
    };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        levels,
        ARRAYSIZE(levels),
        D3D11_SDK_VERSION,
        &sd,
        &g_pSwapChain,
        &g_pd3dDevice,
        &featureLevel,
        &g_pd3dDeviceContext
    );

    if (FAILED(hr))
    {
        // Fallback to WARP software rendering.
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            0,
            levels,
            ARRAYSIZE(levels),
            D3D11_SDK_VERSION,
            &sd,
            &g_pSwapChain,
            &g_pd3dDevice,
            &featureLevel,
            &g_pd3dDeviceContext
        );
    }

    if (FAILED(hr))
        return false;

    CreateRenderTarget();

    return g_mainRenderTargetView != nullptr;
}

void CreateRenderTarget()
{
    ID3D11Texture2D* backBuffer = nullptr;

    if (SUCCEEDED(g_pSwapChain->GetBuffer(
        0,
        IID_PPV_ARGS(&backBuffer))))
    {
        g_pd3dDevice->CreateRenderTargetView(
            backBuffer,
            nullptr,
            &g_mainRenderTargetView
        );

        backBuffer->Release();
    }
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView)
    {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();

    if (g_pSwapChain)
    {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }

    if (g_pd3dDeviceContext)
    {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }

    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

// ============================================================
// WINDOWS MESSAGE HANDLER
// ============================================================

LRESULT WINAPI WndProc(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui::GetCurrentContext() != nullptr &&
        ImGui_ImplWin32_WndProcHandler(
            hWnd, msg, wParam, lParam))
    {
        return true;
    }

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice &&
            g_pSwapChain &&
            wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();

            HRESULT hr = g_pSwapChain->ResizeBuffers(
                0,
                (UINT)LOWORD(lParam),
                (UINT)HIWORD(lParam),
                DXGI_FORMAT_UNKNOWN,
                0
            );

            if (SUCCEEDED(hr))
                CreateRenderTarget();
        }

        return 0;

    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(
        hWnd, msg, wParam, lParam
    );
}
