#include "client_operations.h"
#include <algorithm>

Client get_client_id(Client client, std::string& id) {
    if (id.length() < 2) {
        if(!send_message(client.socket, prepare_message("LO", "ER:ID_TOO_SHORT"))){
            std::cerr << "SEND_ERROR: LO:ER to socket:" << client.socket << "\n";
        }
        client.id = "";
        return client;
    }
    
    bool reconnected = false;
    bool id_active = false;
    
    {
        std::lock_guard<std::mutex> lock_q(queues_mutex);
        std::lock_guard<std::mutex> lock_c(clients_mutex);
        auto it = clients.find(id);
        
        if (it != clients.end()) {
            if (it->second.socket == -1) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.disconnect_time).count();
                
                if (elapsed >= SECONDS_TO_CLEAR_CLIENT) {
                    for (auto& queue : Existing_Queues) {
                        auto sub_it = std::find(queue.subscribers.begin(), 
                                               queue.subscribers.end(), id);
                        if (sub_it != queue.subscribers.end()) {
                            queue.subscribers.erase(sub_it);
                        }
                    }
                    //new connection
                    it->second.socket = client.socket;
                    it->second.disconnect_time = {};
                    client = it->second;

                } else {
                    //reconnection
                    it->second.socket = client.socket;
                    it->second.disconnect_time = {};
                    client = it->second;
                    reconnected = true;
                }
            } else {
                id_active = true;
            }
        } else {
            client.id = id;
            clients[id] = client;
        }
    }
    
    if (id_active) {
        if(!send_message(client.socket, prepare_message("LO", "ER:ID_TAKEN"))){
            std::cerr << "SEND_ERROR: LO:ER to socket:" << client.socket << "\n";
        }
        client.id = "";
        return client;
    }
    
    if (reconnected) {
        if(!send_message(client.socket, prepare_message("LO", "OK:RECONNECTED"))){
            std::cerr << "SEND_ERROR: LO:OK to " << client.id << "\n";
        }
        std::cout << "Client " << client.id << " reconnected\n";
    } else {
        if(!send_message(client.socket, prepare_message("LO", "OK:LOGGED"))){
            std::cerr << "SEND_ERROR: LO:OK to " << client.id << "\n";
        }
        std::cout << "Client " << client.id << " connected\n";
    }
    
    return client;
}