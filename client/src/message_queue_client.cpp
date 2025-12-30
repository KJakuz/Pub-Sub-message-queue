#include "MessageQueueClient.h"
#include "Protocol.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <string>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <vector>

bool MessageQueueClient::connect_to_server(const std::string &host, const std::string &port) {
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
    if (!_verify_connection()) {
        close(_socket);
        _socket = -1;
        return false;
    }
    _connected.store(true);
    _receiver_thread = std::thread(&MessageQueueClient::_receiver_loop, this);
    return true;
}

void MessageQueueClient::disconnect() {
    if (_socket != -1) {
        shutdown(_socket, SHUT_RDWR);
        if (_receiver_thread.joinable()) _receiver_thread.join();
        close(_socket);
        _socket = -1;
    }
}

bool MessageQueueClient::create_queue(const std::string &queue_name) {
    char mode = client_role_map["PUBLISHER"];
    char action = client_action_map["CREATE_QUEUE"];
    std::string message = Protocol::prepare_message(mode, action, queue_name);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::delete_queue(const std::string &queue_name) {
    char mode = client_role_map["PUBLISHER"];
    char action = client_action_map["DELETE_QUEUE"];
    std::string message = Protocol::prepare_message(mode, action, queue_name);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::publish(const std::string &queue_name, std::string &content) {
    char mode = client_role_map["PUBLISHER"];
    char action = client_action_map["PUBLISH"];
    std::string internal_payload = Protocol::_pack_publish_data(queue_name, content, 3600);
    std::string message = Protocol::prepare_message(mode, action, internal_payload);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::subscribe(const std::string &queue_name) {
    char mode = client_role_map["SUBSCRIBER"];
    char action = client_action_map["SUBSCRIBE"];
    std::string message = Protocol::prepare_message(mode, action, queue_name);
    return MessageQueueClient::send_message(_socket, message);
}

bool MessageQueueClient::unsubscribe(const std::string &queue_name) {
    char mode = client_role_map["SUBSCRIBER"];
    char action = client_action_map["UNSUBSCRIBE"];
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
    char header_buffer[HEADER_PACKET_SIZE];
    while (_connected) {
        Event ev{};
        if (!read_exactly(_socket, header_buffer, HEADER_PACKET_SIZE))
        {
            _connected.store(false);
            ev.type = Event::Type::Disconnected;
            {
                std::lock_guard<std::mutex> lock(_event_mutex);
                _event_queue.push(ev);
                _event_cv.notify_one();
            }
            break;
        }
        auto [role, cmd, payload_len] = Protocol::_decode_packet(std::string(header_buffer, HEADER_PACKET_SIZE));

        std::string payload;
        if (payload_len > 0)
        {
            payload.resize(payload_len);
            if (!read_exactly(_socket, payload.data(), payload_len))
            {
                _connected.store(false);
                break;
            }
        }

        // HANDLING GIVEN MESSAGE TYPES
        if (role == 'I' && cmd == 'N')
        {
            ev.type = Event::Type::QueueList;
            ev.queues = _handle_queue_list_payload(payload);
        }
        else if (role == 'M' && cmd == 'S')
        {
            ev.type = Event::Type::Message;
            auto [q, msg] = _handle_message_payload(payload);
            ev.queue = q;
            ev.message = msg;
        }
        else if (role == 'M' && cmd == 'A')
        {
            ev.type = Event::Type::BatchMessages;
            ev.messages = _handle_new_sub_messages(payload);
        }
        else if (role == 'L' && cmd == 'O') {
            if (payload.find("ER:") == 0) {
                ev.type = Event::Type::Error;
                ev.message = payload;
            }
        }
        else if (role == 'Q' && cmd == 'L') {
            auto list = _handle_queue_list_payload(payload);
            {
                std::lock_guard<std::mutex> lock(_queues_cache_mutex);
                _available_queues = list;
            }
            ev.type = Event::Type::QueueList;
            ev.queues = list;
        }
        else if ((role == 'S' && (cmd == 'S' || cmd == 'U')) || (role == 'P' && (cmd == 'C' || cmd == 'D' || cmd == 'B'))) {
            if (payload.find("ER:") == 0)
            {
                ev.type = Event::Type::Error;
                ev.message = "Cmd " + std::string(1, role) + std::string(1, cmd) + " Failed: " + payload;
            }
            else
            {
                ev.type = Event::Type::StatusUpdate;
                ev.message = std::string(1, role) + std::string(1, cmd) + " Success";
            }
        }
        else if (role == 'N' && cmd == 'D')
        {
            ev.type = Event::Type::Error;
            ev.message = "Queue Deleted: " + payload;
        }

        if (ev.type != Event::Type::Disconnected)
        {
            std::lock_guard<std::mutex> lock(_event_mutex);
            _event_queue.push(std::move(ev));
            _event_cv.notify_one();
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
    std::string login_msg = Protocol::prepare_message('L', 'O', _client_login); 
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
    size_t offset = 0;
    uint32_t  q_name_len_net;
    std::memcpy(&q_name_len_net, payload.data() + offset, 4);
    uint32_t q_name_len = ntohl(q_name_len_net);
    offset += 4;

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

bool MessageQueueClient::poll_event(Event &ev) {
    std::lock_guard<std::mutex> lock(_event_mutex);
    if (_event_queue.empty())
        return false;
    ev = std::move(_event_queue.front());
    _event_queue.pop();
    return true;
}