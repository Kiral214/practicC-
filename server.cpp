#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <vector>

namespace beast = boost::beast; 
namespace http = beast::http; 
namespace websocket = beast::websocket; 
namespace net = boost::asio; 
using tcp = net::ip::tcp; 

std::unordered_map<std::string, std::string> users; // Простое хранилище пользователей в памяти

// Класс сессии для обработки WebSocket соединений
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) : ws_(std::move(socket)) {}

    // Запуск сессии
    void start() {
        // Асинхронное принятие WebSocket соединения
        ws_.async_accept([self = shared_from_this()](beast::error_code ec) {
            if (ec) {
                std::cerr << "Accept error: " << ec.message() << std::endl;
                return;
            }
            self->do_read();
        });
    }

private:
    websocket::stream<tcp::socket> ws_;
    beast::flat_buffer buffer_;

    // Чтение сообщения из WebSocket
    void do_read() {
        ws_.async_read(buffer_, [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
            if (ec == websocket::error::closed) {
                std::cerr << "WebSocket closed by client" << std::endl;
                return;
            }
            if (ec) {
                std::cerr << "Read error: " << ec.message() << std::endl;
                return;
            }
            std::string message = beast::buffers_to_string(self->buffer_.data()); // Преобразуем буфер в строку
            self->buffer_.consume(self->buffer_.size()); // Очищаем буфер для следующего чтения
            self->process_message(message);
        });
    }

    // Запись сообщения в WebSocket
    void do_write(const std::string& message) {
        ws_.async_write(net::buffer(message), [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "Write error: " << ec.message() << std::endl;
                return;
            }
        });
    }

    // Сохранение аудио данных в файл
    void saveAudioToFile(const std::vector<char>& audioData, const std::string& filename) {
        std::ofstream file(filename, std::ios::out | std::ios::binary);
        if (file) {
            file.write(audioData.data(), audioData.size());
            file.close();
            std::cout << "Audio saved to file: " << filename << std::endl;
        }
        else {
            std::cerr << "Failed to save audio to file: " << filename << std::endl;
        }
    }

    // Обработка сообщения от клиента
    void process_message(const std::string& message) {
        std::istringstream iss(message);
        std::string command;
        iss >> command;
        if (command == "REGISTER") {
            // Обработка команды регистрации
            std::string username, password;
            iss >> username >> password;
            if (users.find(username) == users.end()) {
                users[username] = password; // Сохраняем пользователя
                do_write("REGISTER_SUCCESS");
            }
            else {
                do_write("REGISTER_FAIL Username already exists");
            }
        }
        else if (command == "LOGIN") {
            // Обработка команды входа
            std::string username, password;
            iss >> username >> password;
            auto it = users.find(username);
            if (it != users.end() && it->second == password) {
                do_write("LOGIN_SUCCESS OpenChatWindow");
            }
            else {
                do_write("LOGIN_FAIL Invalid username or password");
            }
        }
        else if (command == "AUDIO") {
            // Обработка команды аудио
            std::vector<char> audioData(message.begin() + 6, message.end()); // Пропускаем "AUDIO "
            saveAudioToFile(audioData, "received_audio.wav");
            do_write("AUDIO_SUCCESS");
        }
        else {
            do_write("UNKNOWN_COMMAND");
        }
        do_read();
    }
};

// Функция принятия новых соединений
void do_accept(net::io_context& io_context, tcp::acceptor& acceptor) {
    acceptor.async_accept([&](beast::error_code ec, tcp::socket socket) {
        if (!ec) {
            std::make_shared<Session>(std::move(socket))->start(); // Создаем новую сессию для каждого нового соединения
        }
        else {
            std::cerr << "Accept error: " << ec.message() << std::endl;
        }
        do_accept(io_context, acceptor); // Продолжаем принимать новые соединения
        });
}

// Главная функция
int main() {
    try {
        net::io_context io_context; // Контекст ввода-вывода
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 8080)); // Акцептор для принятия соединений на порту 8080
        do_accept(io_context, acceptor); // Начинаем принимать соединения
        io_context.run(); // Запускаем обработку событий
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
