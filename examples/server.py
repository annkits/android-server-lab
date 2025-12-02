import socket

HOST = '127.0.0.1'
PORT = 12345

print(f"Сервер запущен на {HOST}:{PORT}, ждём подключения...")

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind((HOST, PORT))
    s.listen()

    while True:
        conn, addr = s.accept()
        print(f"Подключился клиент: {addr}")

        with conn:
            while True:
                data = conn.recv(1024)
                if not data:
                    break
                print(f"Получено от клиента: {data.decode('utf-8')}")

                response = "Hello from Python server!"
                conn.sendall(response.encode('utf-8'))

        print("Соединение закрыто")