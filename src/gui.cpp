#include <GL/glew.h>
#include <SDL2/SDL.h>

#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>

#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"
#include "implot.h"
#include "shared.h"

void run_gui() {
    // 1) Инициализация SDL
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    SDL_Window* window = SDL_CreateWindow(
        "Backend start", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 768, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    // 2) Инициализация контекста Dear Imgui
    ImGui::CreateContext();
    ImPlot::CreateContext();
    auto& style = ImGui::GetStyle();
    
    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 6.0f;
    style.FrameRounding     = 5.0f;
    style.PopupRounding     = 6.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding      = 5.0f;
    
    style.WindowPadding     = ImVec2(10, 10);
    style.FramePadding      = ImVec2(8, 5);
    style.ItemSpacing       = ImVec2(8, 6);

    // Ввод\вывод
    ImGuiIO& io = ImGui::GetIO();
    ImFont* font_normal = io.Fonts->AddFontFromFileTTF("JetBrainsMono-Medium.ttf", 17.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    ImFont* font_big = io.Fonts->AddFontFromFileTTF("JetBrainsMono-Bold.ttf", 21.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());

    style.FrameRounding = 4.0f;
    style.WindowBorderSize = 0.0f;
    style.PopupBorderSize = 0.0f;
    style.GrabRounding = 4.0f;

    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.73f, 0.75f, 0.74f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.09f, 0.09f, 0.09f, 0.94f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border]                 = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.71f, 0.39f, 0.39f, 0.54f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.84f, 0.66f, 0.66f, 0.40f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.84f, 0.66f, 0.66f, 0.67f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.47f, 0.22f, 0.22f, 0.67f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.47f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.47f, 0.22f, 0.22f, 0.67f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.34f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.71f, 0.39f, 0.39f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.84f, 0.66f, 0.66f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.47f, 0.22f, 0.22f, 0.65f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.71f, 0.39f, 0.39f, 0.65f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
    colors[ImGuiCol_Header]                 = ImVec4(0.71f, 0.39f, 0.39f, 0.54f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.84f, 0.66f, 0.66f, 0.65f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.84f, 0.66f, 0.66f, 0.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.71f, 0.39f, 0.39f, 0.54f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.71f, 0.39f, 0.39f, 0.54f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.71f, 0.39f, 0.39f, 0.54f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.84f, 0.66f, 0.66f, 0.66f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.84f, 0.66f, 0.66f, 0.66f);
    colors[ImGuiCol_Tab]                    = ImVec4(0.71f, 0.39f, 0.39f, 0.54f);
    colors[ImGuiCol_TabHovered]             = ImVec4(0.84f, 0.66f, 0.66f, 0.66f);
    colors[ImGuiCol_TabActive]              = ImVec4(0.84f, 0.66f, 0.66f, 0.66f);
    colors[ImGuiCol_TabUnfocused]           = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Включить Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Включить Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Включить Docking

    // 2.1) Привязка Imgui к SDL2 и OpenGl backend'ам
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    // 3) Игра началась
    // bool running = true;
    while (global_running) {

        // 3.0) Обработка event'ов (inputs, window resize, mouse moving, etc.);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            std::cout << "Processing some event: "<< event.type << std::endl;
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                global_running = false;
            }
        }

        // 3.1) Начинаем создавать новый фрейм;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_None);

        ImGui::SetNextWindowSize(ImVec2(1100, 850), ImGuiCond_FirstUseEver);
        ImGui::Begin("Data", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::PushFont(font_big);
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Текущее местоположение");
        ImGui::PopFont();

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 8));

        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, 140);
        {
            lock_guard<mutex> lock(latest_packet_mutex); 

            ImGui::Text("Широта");   
            ImGui::NextColumn();
            ImGui::Text("%.6f °", latest_packet.location.latitude); 
            ImGui::NextColumn();

            ImGui::Text("Долгота"); 
            ImGui::NextColumn();
            ImGui::Text("%.6f °", latest_packet.location.longitude); 
            
            ImGui::NextColumn();

            ImGui::Text("Высота"); 
            ImGui::NextColumn();
            ImGui::Text("%.1f м", latest_packet.location.altitude); 
            ImGui::NextColumn();

            ImGui::Text("Точность"); 
            ImGui::NextColumn();
            ImGui::Text("%.1f м", latest_packet.location.accuracy); 
            ImGui::NextColumn();

            ImGui::Text("Время"); 
            ImGui::NextColumn();
            ImGui::Text("%s", latest_packet.location.time.c_str());
        }
        ImGui::Columns(1);
        ImGui::Dummy(ImVec2(0, 20));

        ImGui::PushFont(font_big);
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Сотовая связь");
        ImGui::PopFont();

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 8));

        {
            lock_guard<mutex> lock(latest_packet_mutex);

            ImGui::Text("Найдено вышек: %zu", latest_packet.cells.size());

            if (latest_packet.cells.empty()) {
                ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.6f, 1.0f), "Нет данных о вышках");
            } else {
                ImGui::Separator();

                size_t index = 1;
                for (const auto& cell : latest_packet.cells) {
                    string title = "Вышка #" + to_string(index++);
                    ImGui::Text("%s", title.c_str());
                    ImGui::Indent(16.0f);

                    ImGui::Columns(2, nullptr, false);
                    ImGui::SetColumnWidth(0, 140);

                    ImGui::Text("Тип");              
                    ImGui::NextColumn();
                    ImGui::Text("%s", cell.type.c_str()); 
                    ImGui::NextColumn();

                    ImGui::Text("Зарегистрирована"); 
                    ImGui::NextColumn();
                    ImGui::Text("%s", cell.registered ? "да" : "нет"); 
                    ImGui::NextColumn();

                    ImGui::Text("dBm");              
                    ImGui::NextColumn();
                    ImGui::Text("%d", cell.dbm);     
                    ImGui::NextColumn();

                    ImGui::Text("Уровень");          
                    ImGui::NextColumn();
                    ImGui::Text("%d", cell.level);   
                    ImGui::NextColumn();

                    if (cell.rsrp != 0) {
                        ImGui::Text("RSRP");         
                        ImGui::NextColumn();
                        ImGui::Text("%d", cell.rsrp); 
                        ImGui::NextColumn();
                    }

                    if (cell.rsrq != 0) {
                        ImGui::Text("RSRQ");         
                        ImGui::NextColumn();
                        ImGui::Text("%d", cell.rsrq); 
                        ImGui::NextColumn();
                    }

                    if (cell.sinr != 0) {
                        ImGui::Text("SINR");         
                        ImGui::NextColumn();
                        ImGui::Text("%d", cell.sinr); 
                        ImGui::NextColumn();
                    }

                    if (cell.timing_advance != 0) {
                        ImGui::Text("Timing Adv");   
                        ImGui::NextColumn();
                        ImGui::Text("%d", cell.timing_advance); 
                        ImGui::NextColumn();
                    }

                    if (cell.ci != -1) {
                        ImGui::Text("CI");           
                        ImGui::NextColumn();
                        ImGui::Text("%ld", cell.ci); 
                        ImGui::NextColumn();
                    }

                    if (!cell.mcc.empty()) {
                        ImGui::Text("MCC / MNC");    
                        ImGui::NextColumn();
                        ImGui::Text("%s / %s", cell.mcc.c_str(), cell.mnc.c_str()); 
                        ImGui::NextColumn();
                    }

                    if (!cell.operator_name.empty()) {
                        ImGui::Text("Оператор");     
                        ImGui::NextColumn();
                        ImGui::Text("%s", cell.operator_name.c_str()); 
                        ImGui::NextColumn();
                    }

                    ImGui::Columns(1);
                    ImGui::Unindent();
                    ImGui::Dummy(ImVec2(0, 6));
                }
            }
        }

        ImGui::End();

        // 3.3) Отправляем на рендер;
        ImGui::Render();
        glClearColor(0.06f, 0.06f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    // 4) Закрываем приложение безопасно.
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}