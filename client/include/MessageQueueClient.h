#pragma once

#include "Event.h"

#include <string>
#include <map>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstring>
#include <arpa/inet.h>

constexpr size_t SOCKET_TIMEOUT_VALUE = 5;

// @class MessageQueueClient
// @brief Message queue client supporting publishing and subscribing.
//
// @param user_login - an unique user login should be provided.
//
// Example usage:
//
// @code
// MessageQueueClient client("user1");
//
// if (!client.connect_to_server("127.0.0.1", "9000")) {
//      handle error
// }
//
// client.create_queue("jobs");
//
// client.publish("jobs", "hello world", 60);
//
// client.subscribe("jobs");
//
// Event ev;
//
// while (client.poll_event(ev)) {
//      process event
// }
//
// client.disconnect();
// @endcode
//
// Internally, a dedicated receiver thread continuously reads data
// from the server and converts messages into Event objects.
//
// Events are stored in an internal queue and retrieved by calling poll_event().
class MessageQueueClient {
 public:
    MessageQueueClient();
    MessageQueueClient(const std::string &client_login);
    ~MessageQueueClient();

    // @brief Connect to the message queue server.
    //
    // Establishes a TCP connection and performs a handshake.
    //
    // @param host Server hostname or IP address.
    // @param port Server port number.
    // @return true on successful connection, false otherwise.
    bool connect_to_server(const std::string &host, const std::string &port);
    
    //@brief Disconnect from the server.
    //
    // Stops the receiver thread, shutdowns connection, closes the socket.
    void disconnect();

    bool create_queue(const std::string &queue_name);
    bool delete_queue(const std::string &queue_name);
    bool publish(const std::string &queue_name, const std::string &content, size_t ttl);

    bool subscribe(const std::string &queue_name);
    bool unsubscribe(const std::string &queue_name);

    // @brief Retrieve the next pending event from the server.
    //
    // @param ev Output parameter receiving the event.
    // @return true if an event was retrieved, false otherwise.
    bool poll_event(Event &ev);

    std::vector<std::string> get_available_queues() {
        std::lock_guard<std::mutex> lock(_queues_cache_mutex);
        return _available_queues;
    }

    bool is_connected() const { return _connected; };

private:
    int _socket = -1;
    std::string _client_login;
    std::thread _receiver_thread;
    std::atomic<bool> _connected{false};
    
    std::queue<Event> _event_queue;
    std::mutex _event_mutex;
    std::condition_variable _event_cv;

    std::vector<std::string> _available_queues;
    std::mutex _queues_cache_mutex;

    void _receiver_loop();
    static bool _send_message(int socket, const std::string &data);

    // @brief Read exactly N bytes from a socket.
    bool _read_exactly(int sock, char *buffer, size_t size);
    
    std::tuple<std::string, std::string> _handle_message_payload(const std::string &payload);
    std::vector<std::string> _handle_queue_list_payload(const std::string &payload);
    std::vector<std::string> _handle_new_sub_messages(const std::string &payload);

    // @brief Verify server connection via handshake.
    //
    // Seends a login message and expects an OK response.
    // @return true if verification succeeded.
    bool _verify_connection();
    void _handle_disconnect_event();

    void _dispatch_event(char &role, char &cmd, std::string &payload, Event &ev);
};