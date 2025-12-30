#pragma once

#include <string>
#include <map>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>



class MessageQueueClient {
public:
    struct Event
    {
        enum class Type {
            QueueList,
            Message,
            BatchMessages,
            Disconnected,
            Error,
            StatusUpdate
        };
        Type type;
        std::string queue;
        std::string message;
        std::vector<std::string> queues;
        std::vector<std::string> messages;
    };

    MessageQueueClient();
    MessageQueueClient(const std::string &client_login);
    ~MessageQueueClient();
    // TODO: SPlit logic between separate classes
    // Connection
    bool connect_to_server(const std::string &host, const std::string &port);
    void disconnect();

    // Publisher
    bool create_queue(const std::string &queue_name);
    bool delete_queue(const std::string &queue_name);
    bool publish(const std::string &queue_name, std::string &content);

    // Subscriber
    bool subscribe(const std::string &queue_name);
    bool unsubscribe(const std::string &queue_name);

    bool poll_event(Event &ev);

    std::vector<std::string> get_available_queues()
    {
        std::lock_guard<std::mutex> lock(_queues_cache_mutex);
        return _available_queues;
    }

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
    static bool send_message(int socket, const std::string &data);
    bool read_exactly(int sock, char *buffer, size_t size);
    
    std::tuple<std::string, std::string> _handle_message_payload(const std::string &payload);
    std::vector<std::string> _handle_queue_list_payload(const std::string &payload);
    std::vector<std::string> _handle_new_sub_messages(const std::string &payload);

    bool _verify_connection();

};