#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio.hpp>
#include <Windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <SFML/Audio.hpp>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

net::io_context io_context;
tcp::resolver resolver(io_context);
websocket::stream<tcp::socket> ws(io_context);

bool connected = false;
bool loggedIn = false;

HWND hwndRegister, hwndMain, hwndStatusLabel, hwndRecordingButton, hwndStopRecordingButton, hwndMessagesList, hwndPlayButton;

sf::SoundBufferRecorder recorder;
sf::SoundBuffer buffer;

// Функция для подключения к серверу
bool ConnectToServer() {
    try {
        auto const results = resolver.resolve("localhost", "8080"); 
        net::connect(ws.next_layer(), results.begin(), results.end()); 
        ws.handshake("localhost", "/"); 
        connected = true; 
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to connect: " << e.what() << std::endl;
        return false;
    }
}

// Функция для преобразования std::wstring в std::string
std::string wstringToString(const std::wstring& wstr) {
    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(bufferSize, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], bufferSize, nullptr, nullptr);
    return str;
}

// Функция для регистрации пользователя
void RegisterUser(const std::wstring& username, const std::wstring& password) {
    if (!connected) {
        SetWindowText(hwndStatusLabel, L"Not connected to server.");
        return;
    }

    std::string usernameStr = wstringToString(username);
    std::string passwordStr = wstringToString(password);
    std::string message = "REGISTER " + usernameStr + " " + passwordStr;
    ws.write(net::buffer(message)); // Отправка сообщения на сервер

    beast::flat_buffer buffer;
    ws.read(buffer); // Чтение ответа от сервера
    std::string response = beast::buffers_to_string(buffer.data());

    if (response.find("REGISTER_SUCCESS") != std::string::npos) {
        loggedIn = true;
        SetWindowText(hwndStatusLabel, L"Registration successful.");
        ShowWindow(hwndRegister, SW_HIDE);
        ShowWindow(hwndMain, SW_SHOW);
    }
    else {
        SetWindowText(hwndStatusLabel, L"Registration failed. Username may already exist.");
    }
}

// Функция для сохранения аудио в файл
void saveAudioToFile(const std::string& filename) {
    if (buffer.saveToFile(filename)) {
        std::cout << "Audio saved to file: " << filename << std::endl;
    }
    else {
        std::cerr << "Failed to save audio to file: " << filename << std::endl;
    }
}

// Функция для проверки существования файла
bool fileExists(const std::string& filename) {
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}

// Функция для воспроизведения аудио из файла
void playAudioFromFile(const std::string& filename) {
    if (!fileExists(filename)) {
        std::cerr << "File does not exist: " << filename << std::endl;
        SetWindowText(hwndStatusLabel, L"Audio file not found.");
        return;
    }

    std::cout << "Playing audio file: " << filename << std::endl;

    sf::SoundBuffer buffer;
    if (!buffer.loadFromFile(filename)) {
        std::cerr << "Failed to load audio file: " << filename << std::endl;
        SetWindowText(hwndStatusLabel, L"Failed to load audio.");
        return;
    }

    sf::Sound sound;
    sound.setBuffer(buffer);
    sound.play();

    while (sound.getStatus() == sf::Sound::Playing) {
        sf::sleep(sf::milliseconds(100));
    }

    SetWindowText(hwndStatusLabel, L"Audio playback finished.");
}

// Функция для начала записи аудио
void RecordAudio() {
    if (!sf::SoundBufferRecorder::isAvailable()) {
        std::cerr << "Microphone is not available." << std::endl;
        SetWindowText(hwndStatusLabel, L"Microphone is not available.");
        return;
    }

    recorder.start();
    SetWindowText(hwndStatusLabel, L"Recording...");
}

// Функция для остановки записи аудио
void StopRecording() {
    recorder.stop();
    buffer = recorder.getBuffer();
    saveAudioToFile("recorded_audio.wav");

    SendMessage(hwndMessagesList, LB_ADDSTRING, 0, (LPARAM)L"Voice message recorded");
    SetWindowText(hwndStatusLabel, L"Recording stopped and saved.");
}

// Функция для отправки аудио на сервер
void sendAudioToServer() {
    if (!connected) {
        SetWindowText(hwndStatusLabel, L"Not connected to server.");
        return;
    }

    try {
        const sf::Int16* samples = buffer.getSamples();
        std::size_t sampleCount = buffer.getSampleCount();

        std::vector<sf::Int16> audioData(samples, samples + sampleCount);

        std::string message = "AUDIO ";
        message.append(reinterpret_cast<const char*>(audioData.data()), audioData.size() * sizeof(sf::Int16));
        ws.write(net::buffer(message)); // Отправка аудио на сервер

        beast::flat_buffer buffer;
        ws.read(buffer); // Чтение ответа от сервера
        std::string response = beast::buffers_to_string(buffer.data());

        if (response.find("AUDIO_SUCCESS") != std::string::npos) {
            SetWindowText(hwndStatusLabel, L"Audio sent to server successfully.");
        }
        else {
            SetWindowText(hwndStatusLabel, L"Failed to send audio to server.");
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Exception while sending audio: " << e.what() << std::endl;
        SetWindowText(hwndStatusLabel, L"Error sending audio to server.");
    }
}

// Функция для воспроизведения выбранного сообщения
void PlaySelectedMessage() {
    int index = SendMessage(hwndMessagesList, LB_GETCURSEL, 0, 0);
    if (index != LB_ERR) {
        playAudioFromFile("recorded_audio.wav");
    }
    else {
        SetWindowText(hwndStatusLabel, L"No message selected.");
    }
}

// Новая функция для отправки произвольных запросов на сервер
std::string SendRequestToServer(const std::string& request) {
    if (!connected) {
        SetWindowText(hwndStatusLabel, L"Not connected to server.");
        return "";
    }

    try {
        ws.write(net::buffer(request)); // Отправка запроса на сервер

        beast::flat_buffer buffer;
        ws.read(buffer); // Чтение ответа от сервера
        std::string response = beast::buffers_to_string(buffer.data());

        return response;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception while sending request: " << e.what() << std::endl;
        return "Error sending request.";
    }
}

// Обработчик сообщений главного окна
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        // Создание элементов интерфейса
        hwndRecordingButton = CreateWindow(L"BUTTON", L"Record", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            50, 50, 100, 30, hwnd, (HMENU)106, NULL, NULL);
        hwndStopRecordingButton = CreateWindow(L"BUTTON", L"Stop Recording", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            180, 50, 120, 30, hwnd, (HMENU)107, NULL, NULL);
        hwndMessagesList = CreateWindow(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY,
            50, 100, 300, 150, hwnd, (HMENU)108, NULL, NULL);
        hwndPlayButton = CreateWindow(L"BUTTON", L"Play", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            50, 270, 100, 30, hwnd, (HMENU)109, NULL, NULL);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 106) {
            SetWindowText(hwndStatusLabel, L"Recording...");
            EnableWindow(hwndRecordingButton, FALSE);
            EnableWindow(hwndStopRecordingButton, TRUE);

            RecordAudio();
        }
        else if (LOWORD(wParam) == 107) {
            SetWindowText(hwndStatusLabel, L"Recording stopped.");
            EnableWindow(hwndRecordingButton, TRUE);
            EnableWindow(hwndStopRecordingButton, FALSE);

            StopRecording();
            sendAudioToServer();
        }
        else if (LOWORD(wParam) == 109) {
            PlaySelectedMessage();
        }
        else if (LOWORD(wParam) == 108 && HIWORD(wParam) == LBN_SELCHANGE) {
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Обработчик сообщений окна регистрации
LRESULT CALLBACK RegisterWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hwndUsername, hwndPassword;

    switch (msg) {
    case WM_CREATE:
        // Создание элементов интерфейса для регистрации
        hwndUsername = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
            50, 50, 200, 20, hwnd, NULL, NULL, NULL);
        hwndPassword = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_PASSWORD,
            50, 100, 200, 20, hwnd, NULL, NULL, NULL);
        CreateWindow(L"BUTTON", L"Register", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            50, 150, 100, 30, hwnd, (HMENU)104, NULL, NULL);
        hwndStatusLabel = CreateWindow(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            50, 200, 200, 20, hwnd, NULL, NULL, NULL);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 104) {
            wchar_t username[100], password[100];
            GetWindowText(hwndUsername, username, 100);
            GetWindowText(hwndPassword, password, 100);
            RegisterUser(username, password);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Главная функция приложения
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Определяем имя основного класса окна
    const wchar_t MainClassName[] = L"MainWindowClass";
    WNDCLASS wcMain = {};
    wcMain.lpfnWndProc = MainWndProc;  // Указываем процедуру обработки сообщений для главного окна
    wcMain.hInstance = hInstance;
    wcMain.lpszClassName = MainClassName;
    RegisterClass(&wcMain);  // Регистрируем класс главного окна

    // Определяем имя класса окна регистрации
    const wchar_t RegisterClassName[] = L"RegisterWindowClass";
    WNDCLASS wcRegister = {};
    wcRegister.lpfnWndProc = RegisterWndProc;  // Указываем процедуру обработки сообщений для окна регистрации
    wcRegister.hInstance = hInstance;
    wcRegister.lpszClassName = RegisterClassName;
    RegisterClass(&wcRegister);  // Регистрируем класс окна регистрации

    // Создаем главное окно приложения
    hwndMain = CreateWindowEx(0, MainClassName, L"Voice Messaging App",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        400, 400, NULL, NULL, hInstance, NULL);
    if (hwndMain == NULL) return 0;  // Проверка на успешное создание окна

    // Создаем окно регистрации
    hwndRegister = CreateWindowEx(0, RegisterClassName, L"Register Window",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        400, 300, NULL, NULL, hInstance, NULL);
    if (hwndRegister == NULL) return 0;  // Проверка на успешное создание окна

    // Показываем окно регистрации
    ShowWindow(hwndRegister, nCmdShow);
    UpdateWindow(hwndRegister);

    // Подключаемся к серверу
    if (!ConnectToServer()) {
        MessageBox(NULL, L"Failed to connect to server", L"Error", MB_OK);  // Выводим сообщение об ошибке при неудачном подключении
        return 0;
    }

    // Основной цикл обработки сообщений
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

