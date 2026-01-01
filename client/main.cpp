#include "MessageQueueClient.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>

void print_help()
{
    std::cout << "\n--- Commands ---\n"
              << "list          - Show local cache of available queues\n"
              << "create [name] - Create a new queue\n"
              << "sub [name]    - Subscribe to a queue\n"
              << "unsub [name]  - Unsubscribe from a queue\n"
              << "pub [q] [msg] - Publish a message\n"
              << "delete [name] - Delete a queue\n"
              << "exit          - Quit\n"
              << "----------------\n";
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        std::cout << "Usage: " << argv[0] << " <host> <port> <username>\n";
        return 1;
    }

    std::string host = argv[1];
    std::string port = argv[2];
    std::string user = argv[3];

    MessageQueueClient client(user);

    std::cout << "Connecting to " << host << ":" << port << " as " << user << "...\n";
    if (!client.connect_to_server(host, port))
    {
        std::cerr << "Failed to connect/login!\n";
        return 1;
    }

    std::thread ui_thread([&client]() {
    while (true) {
        Event ev;
        if (client.poll_event(ev)) {
            switch (ev.type()) {
                case Event::Type::Message:
                    std::cout << "\n[NEW MESSAGE] Queue: " << ev.source() << " | Content: " << ev.text() << "\n> " << std::flush;
                    break;
                case Event::Type::BatchMessages:
                    std::cout << "\n[HISTORY] Received " << ev.items().size() << " past messages.\n> " << std::flush;
                    for(const auto& m : ev.items()) std::cout << "  - " << m << "\n";
                    break;
                case Event::Type::QueueList:
                    std::cout << "\n[SERVER] Queue List Updated. Total: " << ev.items().size() << "\n> " << std::flush;
                    break;
                case Event::Type::StatusUpdate:
                    std::cout << "\n[OK] " << ev.text() << "\n> " << std::flush;
                    break;
                case Event::Type::Error:
                    std::cerr << "\n[SERVER ERROR] " << ev.text() << "\n> " << std::flush;
                    break;
                case Event::Type::Disconnected:
                    std::cout << "\n[DISCONNECTED FROM SERVER].\n";
                    exit(0);
            }
        }
    } });
    ui_thread.detach();

    print_help();

    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line))
    {
        if (line == "exit")
            break;
        if (line == "list")
        {
            auto q_list = client.get_available_queues();
            std::cout << "Cached Queues: ";
            for (auto &q : q_list)
                std::cout << "[" << q << "] ";
            std::cout << std::endl;
            continue;
        }

        size_t first_space = line.find(' ');
        std::string cmd = line.substr(0, first_space);

        if (cmd == "create" && first_space != std::string::npos)
        {
            client.create_queue(line.substr(first_space + 1));
        }
        else if (cmd == "sub" && first_space != std::string::npos)
        {
            client.subscribe(line.substr(first_space + 1));
        }
        else if (cmd == "unsub" && first_space != std::string::npos)
        {
            client.unsubscribe(line.substr(first_space + 1));
        }
        else if (cmd == "pub")
        {
            size_t second_space = line.find(' ', first_space + 1);
            if (second_space != std::string::npos)
            {
                std::string q_name = line.substr(first_space + 1, second_space - first_space - 1);
                std::string content = line.substr(second_space + 1);
                client.publish(q_name, content, 3600);
            }
        }
        else if (cmd == "delete" && first_space != std::string::npos)
        {
            client.delete_queue(line.substr(first_space + 1));
        }
    }

    client.disconnect();
    return 0;
}