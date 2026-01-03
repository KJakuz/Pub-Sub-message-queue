#pragma once

#include <string>
#include <vector>

// @brief Logical abstraction of a result returned from the server.
//
// The specific kind of event is identified by Event::Type.
//
// Every Event contains a type and may also include additional information:
// @param source (queue name)
// @param status or result information
// @param message payload(s)
//
// If an event is expected to contain multiple results, the user should call items() to retrieve all entries.
// If only a single result is expected, text() can be used to retrieve the first (or only) value.
class Event {
 public:

    enum class Type {
        QueueList,
        Message,
        BatchMessages,
        Disconnected,
        Error,
        StatusUpdate,
        Unknown
    };

    Type type() const { return _type; }
    const std::string &source() const { return _source; }
    const std::string &text() const {
        static const std::string empty;
        if (_result.empty()) return empty;
        return _result.front();
    }
    const std::vector<std::string> &items() const { return _result; }

 private:
    Type _type = Type::Unknown;
    std::string _source;
    std::vector<std::string> _result;


    // Helper event dispatch methods

    bool is_valid() const { return _type != Type::Disconnected && _type != Type::Unknown; }
    bool is_heartbeat(char &role, char &cmd) { return role == 'H' && cmd == 'B'; }
    bool is_initial_queue_list(char &role, char &cmd) { return role == 'I' && cmd == 'N'; }
    bool is_update_queue_list(char &role, char &cmd) { return role == 'Q' && cmd == 'L'; }
    bool is_new_message(char &role, char &cmd) { return role == 'M' && cmd == 'S'; }
    bool is_new_batch_messages(char &role, char &cmd) { return role == 'M' && cmd == 'A'; }
    bool is_queue_deleted(char &role, char &cmd) { return role == 'N' && cmd == 'D'; }
    bool is_new_status_update(char &role, char &cmd) { return (role == 'S' && (cmd == 'S' || cmd == 'U')) || (role == 'P' && (cmd == 'C' || cmd == 'D' || cmd == 'B')); }
    bool is_new_error(char &role, char &cmd) { return role == 'L' && cmd == 'O'; }

    friend class MessageQueueClient;
};