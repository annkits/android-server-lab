#include <iostream>
#include <fstream>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <string>
#include <chrono>
#include <format>
#include <csignal>
#include <zmq.hpp>
#include <json.hpp>
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"
#include "implot.h"
#include "shared.h"

using json = nlohmann::json;
using namespace std;

PacketData latest_packet;
vector<PacketData> history;
mutex history_mutex;
mutex latest_packet_mutex;
atomic<bool> global_running{true};

void run_server();
void run_gui();

void handle_sigint(int) {
    global_running = false;
}

int main() {
    signal(SIGINT, handle_sigint);
    
    global_running = true;

    cout << "Запуск потоков...\n";

    thread server_thread(run_server);
    thread gui_thread(run_gui);

    server_thread.join();
    gui_thread.join();

    cout << "Программа завершена\n";
    return 0;
}