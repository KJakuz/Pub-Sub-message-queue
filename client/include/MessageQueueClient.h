#pragma once

#include "Event.h"
#include "Helpers.h"

#include <string>
#include <map>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstring>
#include <arpa/inet.h>

constexpr size_t SOCKET_TIMEOUT_VALUE = 35;

// @class MessageQueueClient
// @brief Client for interacting with a message queue server.
//
// @param client_login Unique user login.
//
// MessageQueueClient provides an asynchronous API for publishing and
// subscribing to named queues over a TCP connection.
//
// The client maintains an internal receiver thread that continuously
// reads data from the server and converts incoming messages into Event
// objects. These events can be retrieved by calling poll_event().
//
// - Public action methods (create_queue, publish, subscribe, etc.)
// return true if the request was successfully sent to the server.
// A return value of true does NOT guarantee that the server accepted
// or successfully processed the request.
//
// - Server-side failures, protocol errors, and connection issues are
// reported asynchronously as Event::Type::Error or
// Event::Type::Disconnected events.
//
// - Methods return false only for local failures such as invalid
// arguments, disconnected state, or socket send errors.
//
// - The client starts one internal receiver thread upon successful
// connection.
// - All public methods are thread-safe and may be called concurrently.
class MessageQueueClient {
 public:
    MessageQueueClient();
    MessageQueueClient(const std::string &client_login);
    ~MessageQueueClient();

    // @brief Connect to the server.
    //
    // Establishes a TCP connection and performs a protocol handshake using
    // the client login provided at construction time.
    //
    // On success, a dedicated receiver thread is started.
    //
    // @param host Server hostname or IPv4 address.
    // @param port Server port number (string representation).
    //
    // @return true if the connection and handshake succeeded,
    //         false otherwise.
    //
    // @note This function must be called before any publish or subscribe
    //       operations.
    bool connect_to_server(const std::string &host, const std::string &port);

    // @brief Disconnect from the server.
    //
    // Stops the receiver thread, shuts down the socket, and releases
    // all associated resources.
    void disconnect();

    // @brief Request creation of a new queue.
    //
    // Sends a queue creation request to the server. The result of the
    // operation (success or failure) is delivered asynchronously via
    // an Event::Type::StatusUpdate or Event::Type::Error event.
    //
    // @param queue_name Name of the queue to create.
    //
    // @return true if the request was successfully sent to the server,
    // false if the client is disconnected or the queue name is invalid.
    bool create_queue(const std::string &queue_name);
    bool delete_queue(const std::string &queue_name);

    // @brief Publish a message to a queue.
    //
    // Sends a publish request to the server with the specified message
    // content and time-to-live (TTL). Server-side acceptance or rejection
    // is reported asynchronously via events.
    //
    // @param queue_name Name of the target queue.
    // @param content Message payload.
    // @param ttl Time-to-live in seconds.
    //
    // @return true if the request was sent successfully,
    // false if the client is disconnected or arguments are invalid.
    bool publish(const std::string &queue_name, const std::string &content, uint32_t ttl);

    // @brief Subscribe to active queue.
    bool subscribe(const std::string &queue_name);
    // @brief Unubscribe active (and subscribed) queue.
    bool unsubscribe(const std::string &queue_name);

    // @brief Retrieve the next pending event.
    //
    // Blocks until an event is available, the client disconnects or timeout.
    //
    // @param ev Output parameter receiving the next event.
    //
    // @return true if an event was retrieved, false if the client is disconnected, no events remain or timeout.
    bool poll_event(Event &ev);

    // @brief Return list of currently available queues.
    std::vector<std::string> get_available_queues() {
        std::lock_guard<std::mutex> lock(_queues_cache_mutex);
        return _available_queues;
    }

    // @brief Check connection.
    bool is_connected() const { return _connected.load(); };

private:
    std::atomic<int> _socket{-1};
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
    // Sends a login message and expects an OK response.
    // @return true if verification succeeded.
    bool _verify_connection();

    // @brief Handle incorrect server response.
    //
    // @param reason Content of created event with reason of Error.
    // @param is_fatal If is_fatal is true then Disconnect, otherwise
    // treat it like an Error.
    //
    // Server ansers with specified error as ER: are
    // handled in dispatch_event(). This function create
    // Error or Disconnect event when client get harmful response.
    void _handle_error_event(const std::string &reason, bool is_fatal);

    void _dispatch_event(char &role, char &cmd, std::string &payload, Event &ev);
};