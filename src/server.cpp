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

#include "shared.h"
#include <zmq.hpp>
#include <json.hpp>

using json = nlohmann::json;
using namespace std;

void run_server() {
    try {
        zmq::context_t context(1);
        zmq::socket_t socket(context, zmq::socket_type::rep);
        socket.set(zmq::sockopt::rcvtimeo, 10000);
        socket.bind("tcp://0.0.0.0:6000");

        cout << "Сервер запущен на порту 6000\n";
        cout << "Ожидание сообщений..\n";

        int next_id = 1;

        while (global_running) {
            zmq::message_t request;
            auto res = socket.recv(request, zmq::recv_flags::none);

            if (res) {
                string message = request.to_string();

                cout << "Полный JSON от клиента:\n" << message << "\n\n";

                auto now = chrono::system_clock::now();
                auto timestamp = format("{:%Y-%m-%d %H:%M:%S}", now);

                PacketData packet;
                packet.id = next_id++;
                packet.timestamp = timestamp;

                try {
                    json parsed = json::parse(message);
                        
                    if (parsed.contains("location") && parsed["location"].is_object()) {
                        auto loc = parsed["location"];

                        packet.location.latitude  = loc.value("latitude",  0.0f);
                        packet.location.longitude = loc.value("longitude", 0.0f);
                        packet.location.altitude  = loc.value("altitude",  0.0f);
                        packet.location.accuracy  = loc.value("accuracy",  0.0f);
                        packet.location.time = to_string(loc.value("time", 0L));
                        
                    }

                    if (parsed.contains("telephony") && parsed["telephony"].is_object()) {
                        auto& tel = parsed["telephony"];

                        if (tel.contains("cells") && tel["cells"].is_array()) {
                            for (const auto& cell_json : tel["cells"]) {
                                CellInfo cell;

                                cell.type         = cell_json.value("type", string("unknown"));
                                cell.registered   = cell_json.value("registered", false);

                                if (cell_json.contains("signal") && cell_json["signal"].is_object()) {
                                    auto& sig = cell_json["signal"];
                                    cell.dbm            = sig.value("dbm", 0);
                                    cell.level          = sig.value("level", 0);
                                    cell.rsrp           = sig.value("rsrp", 0);
                                    cell.rsrq           = sig.value("rsrq", 0);
                                    cell.sinr           = sig.value("rssnr", 0);
                                    cell.timing_advance = sig.value("timingAdvance", 0);
                                }

                                if (cell_json.contains("identity") && cell_json["identity"].is_object()) {
                                    auto& id = cell_json["identity"];
                                    cell.ci            = id.value("ci", -1LL);
                                    cell.pci           = id.value("pci", -1);
                                    cell.tac           = id.value("tac", -1);
                                    cell.mcc           = id.value("mcc", "");
                                    cell.mnc           = id.value("mnc", "");
                                    cell.operator_name = id.value("operator", "");
                                }

                                packet.cells.push_back(cell);
                            }
                        }
                    }
                } 
                catch (const json::parse_error& e) {
                    cerr << "Ошибка парсинга JSON: " << e.what()
                        << " (позиция " << e.byte << ")\n";
                }
                catch (const exception& e) {
                    cerr << "Ошибка при обработке пакета: " << e.what() << "\n";
                }

                {
                lock_guard<mutex> lock(latest_packet_mutex);
                latest_packet = packet;
                }

                {
                lock_guard<mutex> lock(history_mutex);
                history.push_back(packet);
                }

                string reply = "Ok!";
                socket.send(zmq::buffer(reply), zmq::send_flags::none);

                cout << "Получен пакет #" << packet.id << "  " << timestamp << "\n";
                if (packet.location.latitude != 0.0f || packet.location.longitude != 0.0f) {
                    cout << "  Location: "
                         << packet.location.latitude << ", "
                         << packet.location.longitude
                         << " (точность " << packet.location.accuracy << " м)\n";
                }
                if (!packet.cells.empty()) {
                    cout << "  Telephony: " << packet.cells.size() << " cells\n";
                }
                cout << "\n";
            }
        }
        cout << "Поток сервера завершает работу\n";
    }
    catch (const zmq::error_t& e) {
        cerr << "ZQM-ошибка: " << e.what()<< " (" << e.num() << ")" << std::endl;
    }
    catch (const exception& e) {
        cerr << "Ошибка: " << e.what() << std::endl;
    }
}