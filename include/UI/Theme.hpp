#pragma once
#include <imgui.h>

// Funzione centralizzata per applicare i temi
inline void ApplyTheme(ImGuiStyle& style, bool isDarkTheme)
{
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.CellPadding = ImVec2(6.0f, 6.0f);
    style.ItemSpacing = ImVec2(6.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 6.0f);
    style.IndentSpacing = 25.0f;
    style.ScrollbarSize = 15.0f;
    style.GrabMinSize = 12.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;
    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    ImVec4* colors = style.Colors;
    if (isDarkTheme) {
        // Tema scuro
        const ImVec4 bg_main(0.10f, 0.10f, 0.11f, 1.00f);
        const ImVec4 bg_secondary(0.13f, 0.13f, 0.15f, 1.00f);
        const ImVec4 bg_widget(0.18f, 0.18f, 0.20f, 1.00f);
        const ImVec4 accent(0.05f, 0.53f, 0.98f, 1.00f); 
        const ImVec4 accent_hover(0.15f, 0.63f, 1.00f, 1.00f);
        const ImVec4 text_main(0.95f, 0.96f, 0.98f, 1.00f);
        const ImVec4 border_color(0.22f, 0.22f, 0.25f, 1.00f);

        colors[ImGuiCol_Text] = text_main;
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg] = bg_secondary;
        colors[ImGuiCol_ChildBg] = bg_main;
        colors[ImGuiCol_PopupBg] = bg_main;
        colors[ImGuiCol_Border] = border_color;
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = bg_widget;
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.35f, 0.35f, 0.38f, 1.00f);
        colors[ImGuiCol_TitleBg] = bg_main;
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = bg_main;
        colors[ImGuiCol_MenuBarBg] = bg_main;
        colors[ImGuiCol_ScrollbarBg] = bg_main;
        colors[ImGuiCol_ScrollbarGrab] = bg_widget;
        colors[ImGuiCol_ScrollbarGrabHovered] = accent;
        colors[ImGuiCol_ScrollbarGrabActive] = accent_hover;
        colors[ImGuiCol_CheckMark] = accent;
        colors[ImGuiCol_SliderGrab] = accent;
        colors[ImGuiCol_SliderGrabActive] = accent_hover;
        colors[ImGuiCol_Button] = bg_widget;
        colors[ImGuiCol_ButtonHovered] = accent;
        colors[ImGuiCol_ButtonActive] = accent_hover;
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = accent;
        colors[ImGuiCol_HeaderActive] = accent_hover;
        colors[ImGuiCol_Separator] = border_color;
        colors[ImGuiCol_SeparatorHovered] = accent;
        colors[ImGuiCol_SeparatorActive] = accent_hover;
        colors[ImGuiCol_ResizeGrip] = bg_widget;
        colors[ImGuiCol_ResizeGripHovered] = accent;
        colors[ImGuiCol_ResizeGripActive] = accent_hover;
        colors[ImGuiCol_Tab] = bg_widget;
        colors[ImGuiCol_TabHovered] = accent;
        colors[ImGuiCol_TabActive] = accent;
        colors[ImGuiCol_TabUnfocused] = bg_main;
        colors[ImGuiCol_TabUnfocusedActive] = bg_widget;
        colors[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.5f);
        colors[ImGuiCol_DockingEmptyBg] = bg_main;
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.22f, 0.22f, 0.24f, 1.00f);
        colors[ImGuiCol_TableBorderStrong] = border_color;
        colors[ImGuiCol_TableBorderLight] = border_color;
        colors[ImGuiCol_TableRowBg] = bg_main;
        colors[ImGuiCol_TableRowBgAlt] = bg_secondary;
        colors[ImGuiCol_TextSelectedBg] = accent;

    } else {
        // Tema chiaro
        const ImVec4 bg_main(0.94f, 0.94f, 0.94f, 1.00f);
        const ImVec4 bg_secondary(1.00f, 1.00f, 1.00f, 1.00f);
        const ImVec4 bg_widget(0.86f, 0.86f, 0.86f, 1.00f);
        const ImVec4 accent(0.35f, 0.68f, 1.00f, 1.00f); 
        const ImVec4 accent_hover(0.45f, 0.75f, 1.00f, 1.00f);
        const ImVec4 text_main(0.08f, 0.08f, 0.08f, 1.00f);
        const ImVec4 border_color(0.80f, 0.80f, 0.82f, 1.00f);

        colors[ImGuiCol_Text] = text_main;
        colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
        colors[ImGuiCol_WindowBg] = bg_main;
        colors[ImGuiCol_ChildBg] = bg_secondary;
        colors[ImGuiCol_PopupBg] = bg_secondary;
        colors[ImGuiCol_Border] = border_color;
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = bg_widget;
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TitleBg] = bg_main;
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.88f, 0.88f, 0.88f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = bg_main;
        colors[ImGuiCol_MenuBarBg] = bg_main;
        colors[ImGuiCol_ScrollbarBg] = bg_main;
        colors[ImGuiCol_ScrollbarGrab] = bg_widget;
        colors[ImGuiCol_ScrollbarGrabHovered] = accent;
        colors[ImGuiCol_ScrollbarGrabActive] = accent_hover;
        colors[ImGuiCol_CheckMark] = accent;
        colors[ImGuiCol_SliderGrab] = accent;
        colors[ImGuiCol_SliderGrabActive] = accent_hover;
        colors[ImGuiCol_Button] = bg_widget;
        colors[ImGuiCol_ButtonHovered] = accent;
        colors[ImGuiCol_ButtonActive] = accent_hover;
        colors[ImGuiCol_Header] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = accent;
        colors[ImGuiCol_HeaderActive] = accent_hover;
        colors[ImGuiCol_Separator] = border_color;
        colors[ImGuiCol_SeparatorHovered] = accent;
        colors[ImGuiCol_SeparatorActive] = accent_hover;
        colors[ImGuiCol_ResizeGrip] = bg_widget;
        colors[ImGuiCol_ResizeGripHovered] = accent;
        colors[ImGuiCol_ResizeGripActive] = accent_hover;
        colors[ImGuiCol_Tab] = bg_widget;
        colors[ImGuiCol_TabHovered] = accent;
        colors[ImGuiCol_TabActive] = accent;
        colors[ImGuiCol_TabUnfocused] = bg_main;
        colors[ImGuiCol_TabUnfocusedActive] = bg_widget;
        colors[ImGuiCol_DockingPreview] = ImVec4(accent.x, accent.y, accent.z, 0.5f);
        colors[ImGuiCol_DockingEmptyBg] = bg_main;
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
        colors[ImGuiCol_TableBorderStrong] = border_color;
        colors[ImGuiCol_TableBorderLight] = border_color;
        colors[ImGuiCol_TableRowBg] = bg_secondary;
        colors[ImGuiCol_TableRowBgAlt] = bg_main;
        colors[ImGuiCol_TextSelectedBg] = accent;
    }
}