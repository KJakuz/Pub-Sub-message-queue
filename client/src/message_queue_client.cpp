#include "MessageQueueClient.h"
#include "Protocol.h"
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <vector>

bool MessageQueueClient::connect_to_server(const std::string &host, const std::string &port, const int &client_id) {
    addrinfo hints{ .ai_family = AF_INET, .ai_socktype = SOCK_STREAM, .ai_protocol = IPPROTO_TCP }, *res{};
    if (int err = getaddrinfo(host.c_str(), port.c_str(), &hints, &res)) {
        std::cerr << gai_strerror(err) << std::endl;
        return false;
    }
    _socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (connect(_socket, res->ai_addr, res->ai_addrlen) == -1) {
        std::cerr << "Error with connect" << std::endl;
        freeaddrinfo(res);
        close(_socket);
        return false;
    }
    freeaddrinfo(res);
    if (_verify_connection()) {
        std::cout << "Connected: " << host.c_str() << ":" << port.c_str() << std::endl;
        _connected = true;
        _receiver_thread = std::thread(&MessageQueueClient::_receiver_loop, this);
        return true;
    }

    close(_socket);
    _socket = -1;
    return false;
}

void MessageQueueClient::disconnect() {
    if (_socket != -1) {
        _connected = false;
        shutdown(_socket, SHUT_RDWR);
        if (_receiver_thread.joinable()) _receiver_thread.join();
        close(_socket);
        _socket = -1;
    }
}

bool MessageQueueClient::create_queue(const std::string &queue_name) {
    char mode = ClientMode["PUBLISHER"];
    char action = ClientActions["CREATE_QUEUE"];
    std::string message = Protocol::prepare_message(mode, action, queue_name);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::delete_queue(const std::string &queue_name) {
    char mode = ClientMode["PUBLISHER"];
    char action = ClientActions["DELETE_QUEUE"];
    std::string message = Protocol::prepare_message(mode, action, queue_name);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::publish(const std::string &queue_name, std::string &content) {
    char mode = ClientMode["PUBLISHER"];
    char action = ClientActions["PUBLISH"];
    std::string internal_payload = Protocol::_pack_publish_data(queue_name, content, 3600);
    std::string message = Protocol::prepare_message(mode, action, internal_payload);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::subscribe(const std::string &queue_name) {
    char mode = ClientMode["SUBSCRIBER"];
    char action = ClientActions["SUBSCRIBE"];
    std::string message = Protocol::prepare_message(mode, action, queue_name);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::unsubscribe(const std::string &queue_name) {
    char mode = ClientMode["SUBSCRIBER"];
    char action = ClientActions["UNSUBSCRIBE"];
    std::string message = Protocol::prepare_message(mode, action, queue_name);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::send_message(int sock, const std::string &data) {
    if (sock == -1) return false;

    size_t total_sent = 0;
    size_t data_len = data.size();
    const char *raw_data = data.data();

    while (total_sent < data_len) {
        ssize_t sent = send(sock, raw_data + total_sent, data_len - total_sent, 0);
        if (sent <= 0) { 
            return false;
        }
        total_sent += sent;
    }
    return true;
}

bool MessageQueueClient::read_exactly(int sock, char *buffer, size_t size) {
    size_t total_read = 0;
    while (total_read < size)
    {
        ssize_t received = recv(sock, buffer + total_read, size - total_read, 0);
        if (received <= 0)
            return false;
        total_read += received;
    }
    return true;
}

void MessageQueueClient::_receiver_loop() {
    char header_buffer[6];
    while (_connected) {
        if (!read_exactly(_socket, header_buffer, 6))
        {
            std::cout << "Połączenie zamknięte przez serwer.\n";
            _connected = false;
            break;
        }
        std::string header_str(header_buffer, 6);
        auto [role, cmd, payload_len] = Protocol::_decode_packet(header_str);

        std::string full_payload = "";
        if (payload_len > 0)
        {
            std::vector<char> payload_buffer(payload_len);
            // todo read_exactly?
            ssize_t received_data = recv(_socket, payload_buffer.data(), payload_len, 0);

            if (received_data > 0)
            {
                full_payload.assign(payload_buffer.begin(), payload_buffer.begin() + received_data);
            }
        }

        // HANDLING GIVEN MESSAGE TYPES

        if (role == 'I' && cmd == 'N')
        {
            _available_queues = _handle_queue_list_payload(full_payload);
            for (auto queue : _available_queues) {
                std::cout << "Kolejka dostepna: " << queue <<std::endl;
            }
        }
        else if (role == 'M' && cmd == 'S')
        {
            auto [q_name, content] = _handle_message_payload(full_payload);
            std::cout << "Wiadomosc: " << q_name << ", " << content << std::endl;
        }
        else if (role == 'M' && cmd == 'A')
        {
            auto messages_for_queues = _handle_new_sub_messages(full_payload);
        }
    }
}

std::tuple<std::string, std::string> MessageQueueClient::_handle_message_payload(const std::string &payload) {
    if (payload.size() < 4) {
        // Nie ma dlugosci nazwy
    }

    uint32_t q_name_size_net;
    std::memcpy(&q_name_size_net, payload.data(), sizeof(uint32_t));
    uint32_t q_name_size = ntohl(q_name_size_net);

    if (4 + q_name_size > payload.size())
    {
        std::cerr << "Błąd: Rozmiar nazwy kolejki przekracza rozmiar danych!" << std::endl;
    }

    std::string queue_name = payload.substr(4, q_name_size);
    std::string message_content = payload.substr(4 + q_name_size);

    return {queue_name, message_content};
}

std::vector<std::string> MessageQueueClient::_handle_queue_list_payload(const std::string &payload) {
    std::vector<std::string> queues;

    if (payload.size() < 4)
        return queues;

    uint32_t count_net;
    std::memcpy(&count_net, payload.data(), 4);
    uint32_t count = ntohl(count_net);

    size_t offset = 4;

    for (uint32_t i = 0; i < count; ++i)
    {
        if (offset + 4 > payload.size())
            break;

        uint32_t name_len_net;
        std::memcpy(&name_len_net, payload.data() + offset, 4);
        uint32_t name_len = ntohl(name_len_net);
        offset += 4;

        if (offset + name_len > payload.size())
            break;

        std::string q_name = payload.substr(offset, name_len);
        queues.push_back(q_name);

        offset += name_len; 
    }

    return queues;
}

MessageQueueClient::MessageQueueClient()
    : MessageQueueClient("")
{
}

MessageQueueClient::MessageQueueClient(const std::string &client_login)
    : _socket(-1),
      _client_login(client_login),
      _connected(false)
{
}

MessageQueueClient::~MessageQueueClient() {
    disconnect();
}

bool MessageQueueClient::_verify_connection() {
    std::string login_msg = Protocol::prepare_message('L', 'I', _client_login); 
    if(!send_message(_socket, login_msg)) return false;

    char header[6];
    if (!read_exactly(_socket, header, 6)) return false;

    auto [role, cmd, len] = Protocol::_decode_packet(std::string(header, 6));

    std::string payload;
    if (len > 0) {
        payload.resize(len);
        if (!read_exactly(_socket, &payload[0], len)) return false;
    } // maybe function for that?

    if (role == 'L' && cmd == 'O') {
        std::cout << "Server accepted login: " << payload << std::endl;
        return true;
    }
    else {
        std::cerr << "Login failed: " << payload << std::endl;
        return false;
    }
}

std::vector<std::string> MessageQueueClient::_handle_new_sub_messages(const std::string &payload) {
    if (payload.size() < 4) return;
    size_t offset = 0;
    uint32_t  q_name_len_net;
    std::memcpy(&q_name_len_net, payload.data() + offset, 4);
    uint32_t q_name_len = ntohl(q_name_len_net);
    offset += 4;

    if (offset + q_name_len > payload.size()) return;
    std::string queue_name = payload.substr(offset, q_name_len);
    offset += q_name_len;

    std::vector<std::string> messages;

    while (offset + 4 <= payload.size()){
        uint32_t msg_len_net;
        std::memcpy(&msg_len_net, payload.data() + offset, 4);
        uint32_t msg_len = ntohl(msg_len_net);
        offset += 4;

        if (offset + msg_len > payload.size())
            break;

        std::string message_text = payload.substr(offset, msg_len);
        messages.push_back(message_text);
        offset += msg_len;
    }

    return messages;
}