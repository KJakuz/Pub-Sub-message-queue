#include "MessageQueueClient.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

// Pomocnicza funkcja do dzielenia wpisanego tekstu na słowa
std::vector<std::string> split(const std::string &s)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, ' '))
        tokens.push_back(token);
    return tokens;
}

int main()
{
    MessageQueueClient client;

    std::cout << "--- Łączenie z serwerem... ---" << std::endl;
    if (!client.connect_to_server("127.0.0.1", "5555", 2137))
    {
        std::cerr << "Błąd: Nie można połączyć się z serwerem!" << std::endl;
        return 1;
    }

    std::cout << "Połączono! Dostępne komendy: " << std::endl;
    std::cout << "  create <nazwa>" << std::endl;
    std::cout << "  delete <nazwa>" << std::endl;
    std::cout << "  publish <nazwa> <wiadomość> <ttl>" << std::endl;
    std::cout << "  exit" << std::endl;

    std::string input;
    while (true)
    {
        std::cout << "\n> ";
        if (!std::getline(std::cin, input) || input == "exit")
            break;

        auto args = split(input);
        if (args.empty())
            continue;

        if (args[0] == "create" && args.size() > 1)
        {
            client.create_queue(args[1]);
        }
        else if (args[0] == "delete" && args.size() > 1)
        {
            client.delete_queue(args[1]);
        }
        else if (args[0] == "publish" && args.size() > 3)
        {
            // publish nazwa_kolejki tresc_wiadomosci ttl
            uint32_t ttl = std::stoul(args[3]);
            client.publish(args[1], args[2]);
        }
        else
        {
            std::cout << "Nieznana komenda lub za mało argumentów." << std::endl;
        }
    }

    client.disconnect();
    std::cout << "Rozłączono." << std::endl;
    return 0;
}