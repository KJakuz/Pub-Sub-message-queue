#pragma once

#include <string>
#include <map>
#include <thread>



class MessageQueueClient
{
public:
    MessageQueueClient();
    ~MessageQueueClient();

    // Connection
    bool connect_to_server(const std::string &host, const std::string &port, const int &client_id);
    void disconnect();
    static bool send_message(int socket, const std::string &data);

    // Publisher
    bool create_queue(const std::string &queue_name);
    bool delete_queue(const std::string &queue_name);
    bool publish(const std::string &queue_name, std::string &content);

    // Subscriber
    bool subscribe(const std::string &queue_name);
    bool unsubscribe(const std::string &queue_name);

private:
    int _socket;
    int _client_id;
    bool _connected;
    void _receiver_loop();
    std::thread _receiver_thread;
};