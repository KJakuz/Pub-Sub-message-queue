#include "MessageQueueClient.h"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <thread>
#include <atomic>

std::atomic<bool> g_running{true};

void print_help()
{
    std::cout << "\n--- Available Commands ---\n"
              << "  list              - Show cached queues\n"
              << "  create <name>     - Create a new queue\n"
              << "  sub <name>        - Subscribe to messages\n"
              << "  unsub <name>      - Stop listening\n"
              << "  pub <q> <msg>     - Send a message\n"
              << "  delete <name>     - Remove queue from server\n"
              << "  help              - Show this menu\n"
              << "  exit              - Disconnect and quit\n"
              << "--------------------------\n";
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        std::cerr << "Usage: " << argv[0] << " <host> <port> <username>\n";
        return 1;
    }

    MessageQueueClient client(argv[3]);

    std::cout << "Connecting to " << argv[1] << ":" << argv[2] << "..." << std::endl;
    if (!client.connect_to_server(argv[1], argv[2]))
    {
        return 1;
    }

    std::thread ui_thread([&client]()
                          {
        while (g_running) {
            Event ev;
            if (client.poll_event(ev)) {
                std::cout << "\r"; 

                switch (ev.type()) {
                    case Event::Type::Message:
                        std::cout << "[MSG] " << ev.source() << ": " << ev.text() << "\n";
                        break;
                    case Event::Type::BatchMessages:
                        std::cout << "\n[HISTORY] Received " << ev.items().size() << " past messages.\n> " << std::flush;
                        for (const auto &m : ev.items())
                            std::cout << "  - " << m << "\n";
                        break;
                    case Event::Type::QueueList:
                        std::cout << "[SERVER] Found " << ev.items().size() << " queues.\n";
                        break;
                    case Event::Type::StatusUpdate:
                        std::cout << "[SUCCESS] " << ev.text() << "\n";
                        break;
                    case Event::Type::Error:
                        std::cout << "[ERROR] " << ev.text() << "\n";
                        break;
                    case Event::Type::Disconnected:
                        std::cout << "[SYSTEM] " << ev.text() << "\n";
                        g_running = false;
                        break;
                    default: break;
                }
                
                if (g_running) std::cout << "> " << std::flush;
            }
        } });

    print_help();

    std::string input;
    while (g_running)
    {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, input) || input == "exit")
            break;
        if (input.empty())
            continue;

        std::stringstream ss(input);
        std::string cmd, arg1, arg2;
        ss >> cmd >> arg1;

        if (cmd == "help")
        {
            print_help();
        }
        else if (cmd == "list")
        {
            auto queues = client.get_available_queues();
            std::cout << "Available: ";
            for (const auto &q : queues)
                std::cout << "[" << q << "] ";
            std::cout << "\n";
        }
        else if (cmd == "create" && !arg1.empty())
        {
            client.create_queue(arg1);
        }
        else if (cmd == "sub" && !arg1.empty())
        {
            client.subscribe(arg1);
        }
        else if (cmd == "unsub" && !arg1.empty())
        {
            client.unsubscribe(arg1);
        }
        else if (cmd == "pub" && !arg1.empty())
        {
            std::getline(ss >> std::ws, arg2);
            if (!arg2.empty())
                client.publish(arg1, arg2, 60);
            else
                std::cout << "Usage: pub <queue> <message>\n";
        }
        else if (cmd == "delete" && !arg1.empty())
        {
            client.delete_queue(arg1);
        }
        else
        {
            std::cout << "Unknown command: '" << cmd << "'. Type 'help' for info.\n";
        }
    }

    g_running = false;
    client.disconnect();

    if (ui_thread.joinable())
    {
        ui_thread.join();
    }

    std::cout << "Goodbye!\n";
    return 0;
}