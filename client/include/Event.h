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
        StatusUpdate
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
    Type _type;
    std::string _source;
    std::vector<std::string> _result;
    bool is_valid() const { return _type != Type::Disconnected; }

    friend class MessageQueueClient;
};