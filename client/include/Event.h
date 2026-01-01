#include <string>
#include <vector>

class Event {
 public:
    enum class Type
    {
        QueueList,
        Message,
        BatchMessages,
        Disconnected,
        Error,
        StatusUpdate
    };

    Type type() const { return _type; }
    const std::string &source() const { return _source; }
    const std::string &text() const
    {
        static const std::string empty;
        if (_result.empty())
            return empty;
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