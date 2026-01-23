#include "MessageQueueClient.h"
#include "Protocol.h"
#include "Helpers.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <string>
#include <unistd.h>
#include <iostream>
#include <thread>
#include <vector>
#include <cctype>

std::mutex cout_mutex;

void thread_safe_print(const std::string &msg) {
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << msg << std::endl;
}

// ------------------------------
// CONSTRUCTOR
// ------------------------------

MessageQueueClient::MessageQueueClient()
    : MessageQueueClient("")
{}

MessageQueueClient::MessageQueueClient(const std::string &client_login)
    : _socket(-1),
      _client_login(client_login),
      _connected(false)
{}

MessageQueueClient::~MessageQueueClient() {
    disconnect();
}

// ------------------------------
// CONNECTION
// ------------------------------

bool MessageQueueClient::connect_to_server(const std::string &host, const std::string &port) {
    addrinfo hints{}, *res{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (int err = getaddrinfo(host.c_str(), port.c_str(), &hints, &res)) {
        thread_safe_print(gai_strerror(err));
        return false;
    }
    _socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    const int reuse{1};
    setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    struct timeval tv{};
    tv.tv_sec = SOCKET_TIMEOUT_VALUE;
    tv.tv_usec = 0;
    setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (connect(_socket, res->ai_addr, res->ai_addrlen) == -1) {
        thread_safe_print("DEBUG: Error with connect");
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
    bool was_connected = _connected.exchange(false);
    
    if (was_connected) {
        int sock = _socket.exchange(-1);
        if (sock != -1) {
            shutdown(sock, SHUT_RDWR);
            close(sock);
        }
    }
    
    if (_receiver_thread.joinable()) _receiver_thread.join();
}

void MessageQueueClient::_handle_error_event(const std::string &reason, bool is_fatal) {
    if(!_connected.load()) return; // We don't bother with handling event when we are disconnecting

    Event ev;
    if (is_fatal) {
        ev._type = Event::Type::Disconnected;
        _connected.store(false);
    }
    else {
        ev._type = Event::Type::Error;
    }
    ev._result.push_back(reason);

    {
        std::lock_guard<std::mutex> lock(_event_mutex);
        _event_queue.push(std::move(ev));
    }
    _event_cv.notify_one();
}

bool MessageQueueClient::_verify_connection() {
    std::string login_msg = Protocol::_prepare_message('L', 'O', _client_login);
    if (!_send_message(_socket, login_msg))
        return false;

    // Read acknowledgment and accept from a server.
    char header[HEADER_PACKET_SIZE];
    if (!_read_exactly(_socket, header, HEADER_PACKET_SIZE))
        return false;

    auto [role, cmd, len] = Protocol::_decode_packet(std::string(header, HEADER_PACKET_SIZE));

    if (len > MAX_PAYLOAD) {
        thread_safe_print("DEBUG: Server sent too much data.");
        return false;
    }

    std::string payload;
    if (len > 0) {
        payload.resize(len);
        if (!_read_exactly(_socket, payload.data(), len)) return false;
    }
    // Server accept new client by sending LO message.
    if (role == 'L' && cmd == 'O') {
        if (payload.find("OK") == 0) {
            return true;
        }
    }

    thread_safe_print("DEBUG: Login failed: " + payload);
    return false;
}

// ------------------------------
// ACTIONS
// ------------------------------

bool MessageQueueClient::create_queue(const std::string &queue_name) {
    if (!_connected.load() || !_is_valid_queue_name(queue_name)) return false;
    std::string message = Protocol::_prepare_message(Role::Publisher, Action::Create, queue_name);
    return MessageQueueClient::_send_message(_socket, message);
}

bool MessageQueueClient::delete_queue(const std::string &queue_name) {
    if (!_connected.load()) return false;
    std::string message = Protocol::_prepare_message(Role::Publisher, Action::Delete, queue_name);
    return MessageQueueClient::_send_message(_socket, message);
}

bool MessageQueueClient::publish(const std::string &queue_name, const std::string &content, uint32_t ttl) {
    if (!_is_valid_ttl(ttl) || !_connected.load()) return false;
    std::string internal_payload = Protocol::_pack_publish_data(queue_name, content, ttl);
    std::string message = Protocol::_prepare_message(Role::Publisher, Action::Publish, internal_payload);
    return MessageQueueClient::_send_message(_socket, message);
}

bool MessageQueueClient::subscribe(const std::string &queue_name) {
    if (!_connected.load()) return false;
    std::string message = Protocol::_prepare_message(Role::Subscriber, Action::Subscribe, queue_name);
    return MessageQueueClient::_send_message(_socket, message);
}

bool MessageQueueClient::unsubscribe(const std::string &queue_name) {
    if (!_connected.load()) return false;
    std::string message = Protocol::_prepare_message(Role::Subscriber, Action::Unsubscribe, queue_name);
    return MessageQueueClient::_send_message(_socket, message);
}

// ------------------------------
// COMMUNICATION
// ------------------------------

bool MessageQueueClient::_send_message(int sock, const std::string &data) {
    if (sock < 0) return false;

    size_t total_sent = 0;
    while (total_sent < data.size()) {
        ssize_t sent = send(sock, data.data() + total_sent, data.size() - total_sent, MSG_NOSIGNAL);
        
        if (sent <= 0) return false;
        total_sent += sent;
    }
    return true;
}

bool MessageQueueClient::_read_exactly(int sock, char *buffer, size_t size) {
    if (sock < 0) return false;

    size_t total_read = 0;
    while (total_read < size) {
        ssize_t received = recv(sock, buffer + total_read, size - total_read, 0);
        if (received <= 0)
            return false;
        total_read += received;
    }
    return true;
}

void MessageQueueClient::_receiver_loop() {
    char header_buffer[HEADER_PACKET_SIZE];

    while (_connected.load()) {
        if (_socket.load() == -1) break;
        if (!_read_exactly(_socket, header_buffer, HEADER_PACKET_SIZE)) {
            _handle_error_event("Reading packet failed.", true);
            break;
        }
        auto [role, cmd, payload_len] = Protocol::_decode_packet(std::string(header_buffer, HEADER_PACKET_SIZE));

        if (payload_len > MAX_PAYLOAD) {
            _handle_error_event("Message from server is too big.", false);
            std::string discard;
            discard.resize(payload_len);
            if (!_read_exactly(_socket, discard.data(), payload_len)) {
                _handle_error_event("Failed to read oversized payload.", true);
                break;
            }
            continue;
        }

        std::string payload;
        if (payload_len > 0) {
            payload.resize(payload_len);
            if (!_read_exactly(_socket, payload.data(), payload_len)) {
                _handle_error_event("Read exactly failed.", true);
                break;
            }
        }

        Event ev{};
        _dispatch_event(role, cmd, payload, ev);
    }
}

void MessageQueueClient::_dispatch_event(char &role, char &cmd, std::string &payload, Event &ev) {
    if (ev.is_heartbeat(role, cmd)) {
        std::string heartbeat = Protocol::_prepare_message('H', 'B', "");
        _send_message(_socket, heartbeat);
        return;
    }
    if (ev.is_initial_queue_list(role, cmd)) {
        ev._type = Event::Type::QueueList;
        ev._result = _handle_queue_list_payload(payload);
    }
    else if (ev.is_new_message(role, cmd)) {
        ev._type = Event::Type::Message;
        auto [q, msg] = _handle_message_payload(payload);
        ev._source = q;
        ev._result.push_back(msg);
    }
    else if (ev.is_new_batch_messages(role, cmd)) {
        ev._type = Event::Type::BatchMessages;
        ev._result = _handle_new_sub_messages(payload);
    }
    else if (ev.is_new_error(role, cmd)) {
        if (payload.find("ER:") == 0)
        {
            ev._type = Event::Type::Error;
            ev._result.push_back(payload);
        }
    }
    else if (ev.is_update_queue_list(role, cmd))
    {
        auto list = _handle_queue_list_payload(payload);
        {
            std::lock_guard<std::mutex> lock(_queues_cache_mutex);
            _available_queues = list;
        }
        ev._type = Event::Type::QueueList;
        ev._result = list;
    }
    else if (ev.is_new_status_update(role, cmd)) {
        if (payload.find("ER:") == 0) {
            ev._type = Event::Type::Error;
            ev._result.push_back("Cmd " + std::string(1, role) + std::string(1, cmd) + " Failed: " + payload);
        }
        else {
            ev._type = Event::Type::StatusUpdate;
            ev._result.push_back(std::string(1, role) + std::string(1, cmd) + " Success");
        }
    }
    else if (ev.is_queue_deleted(role, cmd)) {
        ev._type = Event::Type::Error;
        ev._result.push_back("Queue Deleted: " + payload);
    }
    else {
        ev._type = Event::Type::Error;
        ev._result.push_back("Unknown message type: [" + std::string(1, role) + std::string(1, cmd) + "]");
    }

    if (ev.is_valid()) {
        std::lock_guard<std::mutex> lock(_event_mutex);
        _event_queue.push(std::move(ev));
        _event_cv.notify_one();
    }
}


// ------------------------------
// HANDLING MESSAGES
// ------------------------------

std::tuple<std::string, std::string> MessageQueueClient::_handle_message_payload(const std::string &payload) {
    uint32_t q_name_size;
    extract_convert_net_to_host(payload, 0, q_name_size);
    std::string queue_name = payload.substr(4, q_name_size);
    std::string message_content = payload.substr(4 + q_name_size);

    return {queue_name, message_content};
}

std::vector<std::string> MessageQueueClient::_handle_queue_list_payload(const std::string &payload) {
    std::vector<std::string> queues;
    uint32_t count_net;
    extract_convert_net_to_host(payload, 0, count_net);

    size_t offset = 4;

    // Get all available queues and save it to list.
    for (uint32_t i = 0; i < count_net; ++i) {
        if (offset + 4 > payload.size()) break;

        uint32_t q_name_size;
        extract_convert_net_to_host(payload, offset, q_name_size);
        offset += 4;

        if (offset + q_name_size > payload.size()) break;

        std::string q_name = payload.substr(offset, q_name_size);
        queues.push_back(q_name);

        offset += q_name_size; 
    }
    return queues;
}

std::vector<std::string> MessageQueueClient::_handle_new_sub_messages(const std::string &payload) {
    size_t offset = 0;
    uint32_t  q_name_len;
    extract_convert_net_to_host(payload, offset, q_name_len);
    offset += 4;

    std::string queue_name = payload.substr(offset, q_name_len);
    offset += q_name_len;

    std::vector<std::string> messages;

    // Get all available messages for subscriber and save it to list.
    while (offset + 4 <= payload.size()){
        uint32_t msg_len;
        extract_convert_net_to_host(payload, offset, msg_len);
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
    std::unique_lock<std::mutex> lock(_event_mutex);
    _event_cv.wait_for(lock, std::chrono::milliseconds(100), 
        [this] { return !_event_queue.empty() || !_connected; });

    if (_event_queue.empty()) return false;

    ev = std::move(_event_queue.front());
    _event_queue.pop();
    return true;
}