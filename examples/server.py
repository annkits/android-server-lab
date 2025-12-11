import zmq
import json
from datetime import datetime

HOST = '0.0.0.0'
PORT = 6000
DATA_FILE = 'received_data.json'


def load_data():
    try:
        with open(DATA_FILE, 'r') as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError):
        return []


def save_data(data_list):
    with open(DATA_FILE, 'w') as f:
        json.dump(data_list, f, indent=2, ensure_ascii=False)


def run_server():
    context = zmq.Context()
    socket = context.socket(zmq.REP)
    socket.bind(f"tcp://{HOST}:{PORT}")

    data_list = load_data()
    packet_count = len(data_list)

    print(f"сервер запущен на {HOST}:{PORT}")
    print("ожидание сообщений от android...")
    print("нажмите ctrl+c для остановки и просмотра данных\n")

    try:
        while True:
            message = socket.recv_string()
            timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

            print(f"[{datetime.now().strftime('%H:%M:%S')}] получено: {message}")

            record = {
                'id': packet_count + 1,
                'timestamp': timestamp,
                'data': message,
                'client_ip': 'Android'
            }

            data_list.append(record)
            save_data(data_list)

            packet_count += 1

            print(f"  -> сохранено как запись #{record['id']}")

            response = "Hello from Server!"
            socket.send_string(response)
            print(f"  -> отправлен ответ: {response}\n")

    except KeyboardInterrupt:
        print("\nсервер остановлен")

        print("\n" + "=" * 50)
        print("все сохранённые данные:")
        print("=" * 50)
        if not data_list:
            print("нет данных")
        else:
            for r in data_list:
                print(f"\nзапись #{r['id']}")
                print(f"  время: {r['timestamp']}")
                print(f"  данные: {r['data']}")
                print(f"  клиент: {r['client_ip']}")
        print("\n" + "=" * 50)
        print(f"всего пакетов: {len(data_list)}")
        print("=" * 50)

    finally:
        socket.close()
        context.term()

run_server()