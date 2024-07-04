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

std::unordered_map<std::string, std::string> users; // ������� ��������� ������������� � ������

// ����� ������ ��� ��������� WebSocket ����������
class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket) : ws_(std::move(socket)) {}

    // ������ ������
    void start() {
        // ����������� �������� WebSocket ����������
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

    // ������ ��������� �� WebSocket
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
            std::string message = beast::buffers_to_string(self->buffer_.data()); // ����������� ����� � ������
            self->buffer_.consume(self->buffer_.size()); // ������� ����� ��� ���������� ������
            self->process_message(message);
        });
    }

    // ������ ��������� � WebSocket
    void do_write(const std::string& message) {
        ws_.async_write(net::buffer(message), [self = shared_from_this()](beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "Write error: " << ec.message() << std::endl;
                return;
            }
        });
    }

    // ���������� ����� ������ � ����
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

    // ��������� ��������� �� �������
    void process_message(const std::string& message) {
        std::istringstream iss(message);
        std::string command;
        iss >> command;
        if (command == "REGISTER") {
            // ��������� ������� �����������
            std::string username, password;
            iss >> username >> password;
            if (users.find(username) == users.end()) {
                users[username] = password; // ��������� ������������
                do_write("REGISTER_SUCCESS");
            }
            else {
                do_write("REGISTER_FAIL Username already exists");
            }
        }
        else if (command == "LOGIN") {
            // ��������� ������� �����
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
            // ��������� ������� �����
            std::vector<char> audioData(message.begin() + 6, message.end()); // ���������� "AUDIO "
            saveAudioToFile(audioData, "received_audio.wav");
            do_write("AUDIO_SUCCESS");
        }
        else {
            do_write("UNKNOWN_COMMAND");
        }
        do_read();
    }
};

// ������� �������� ����� ����������
void do_accept(net::io_context& io_context, tcp::acceptor& acceptor) {
    acceptor.async_accept([&](beast::error_code ec, tcp::socket socket) {
        if (!ec) {
            std::make_shared<Session>(std::move(socket))->start(); // ������� ����� ������ ��� ������� ������ ����������
        }
        else {
            std::cerr << "Accept error: " << ec.message() << std::endl;
        }
        do_accept(io_context, acceptor); // ���������� ��������� ����� ����������
        });
}

// ������� �������
int main() {
    try {
        net::io_context io_context; // �������� �����-������
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 8080)); // �������� ��� �������� ���������� �� ����� 8080
        do_accept(io_context, acceptor); // �������� ��������� ����������
        io_context.run(); // ��������� ��������� �������
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
