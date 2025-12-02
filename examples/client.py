import socket
import time

HOST = '127.0.0.1'
PORT = 12345

print("Запуск Python клиента...")

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))
    message = "Hello World! from Python client"
    s.sendall(message.encode('utf-8'))
    print(f"Отправлено: {message}")

    data = s.recv(1024)
    print(f"Ответ от сервера: {data.decode('utf-8')}")

print("Клиент завершил работу")