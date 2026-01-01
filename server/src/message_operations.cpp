#include "message_operations.h"
#include <sys/socket.h>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <algorithm>


std::unordered_map<std::string, Queue>::iterator find_queue_by_name(const std::string& queue_name) {
    return existing_queues.find(queue_name);
}

bool is_client_subscribed(const Queue& queue, const std::string& client_id) {
    auto sub_it = std::find(queue.subscribers.begin(), queue.subscribers.end(), client_id);
    return sub_it != queue.subscribers.end();
}

std::vector<std::string>::iterator find_subscriber(Queue& queue, const std::string& client_id) {
    return std::find(queue.subscribers.begin(), queue.subscribers.end(), client_id);
}

bool queue_exists(const std::string& queue_name) {
    return find_queue_by_name(queue_name) != existing_queues.end();
}


std::string construct_queue_list(){
     /*
    PREPARING MESSAGE THAT LOOKS LIKE THIS: 
    [TYPE(2b)] [CONTENT_SIZE(4b)] [NUMBER_OF_QUEUES(4b)] [QUEUE1_NAME_SIZE(4b)] [QUEUE1_NAME(n)] [QUEUEX_NAME_SIZE(4b)] [QUEUEX_NAME(n)] 
    */
    std::string internal_data;
   
    {
        std::lock_guard<std::mutex> lock(queues_mutex);
        internal_data.reserve(existing_queues.size() * 32); // Approximate size reservation
        
        uint32_t queues_count = htonl(static_cast<uint32_t>(existing_queues.size()));
        internal_data.append(reinterpret_cast<const char*>(&queues_count), 4);

        for (const auto& [name, q] : existing_queues) {
            // For every queue: name lenght(4b):Name
            uint32_t n_len = htonl(static_cast<uint32_t>(q.name.length()));
            internal_data.append(reinterpret_cast<const char*>(&n_len), 4);
            internal_data.append(q.name);
        }
    }

    std::string packet = prepare_message("QL", internal_data);

    return packet;
}

void broadcast_queues_list(){
    /*
    SENDING MESSAGE THAT LOOKS LIKE THIS: 
    [TYPE(2b)] [CONTENT_SIZE(4b)] [NUMBER_OF_QUEUES(4b)] [QUEUE1_NAME_SIZE(4b)] [QUEUE1_NAME(n)] [QUEUEX_NAME_SIZE(4b)] [QUEUEX_NAME(n)] 
    */

    std::string packet = construct_queue_list();

    std::vector<int> target_sockets;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (auto const& [id, client] : clients) {
            if (client.socket != -1){
                target_sockets.push_back(client.socket);
            }
        }
    }

    for (int sock : target_sockets) {
        if (!send_message(sock, packet)) {
            safe_error("SEND_ERROR: QL to socket:" + std::to_string(sock));
        }
    }
}

void send_single_queue_list(const Client& client){
    std::string packet = construct_queue_list();
     if (!send_message(client.socket, packet)) {
        safe_error("SEND_ERROR: QL to socket:" + std::to_string(client.socket));
     }
}

void send_published_message(const Client& client, const std::string &queue_name, const std::string &content){
    /*
    SENDING MESSAGE THAT LOOKS LIKE THIS: 
    [TYPE(2b)] [CONTENT_SIZE(4b)] [QUEUE_NAME_SIZE(4b)] [QUEUE_NAME(n)] [MESSAGE(n)] 
    */
    std::string internal_data;
    bool exists = false;

    {
    std::lock_guard<std::mutex> lock(queues_mutex);
    if (queue_exists(queue_name)) {
        exists = true;
        internal_data.reserve(4 + queue_name.size() + content.size());

        uint32_t n_len = htonl(static_cast<uint32_t>(queue_name.length()));
        internal_data.append(reinterpret_cast<const char*>(&n_len), sizeof(n_len));

        internal_data.append(queue_name);
        internal_data.append(content);
    }
    }

    if (!exists){ 
        return;
    }

    std::string full_packet = prepare_message("MS", internal_data);

    if (!send_message(client.socket, full_packet)) {
        safe_error("SEND_ERROR: MS to socket:" + std::to_string(client.socket));
    }
}

void send_messages_to_new_subscriber(const Client& client, const std::string& queue_name) { 
    /*
    SENDING MESSAGE THAT LOOKS LIKE THIS: 
    [TYPE(2b)] [CONTENT_SIZE(4b)] [QUEUE_NAME_SIZE(4b)] [QUEUE_NAME(n)] [MESSAGE1_SIZE(4b)] [MESSAGE1(n)] ... [MESSAGEn_SIZE(4b)] [MESSAGEn(n)] 
    */
    std::string internal_data;
    bool exists = false;
    bool has_messages = false;

    {
    std::lock_guard<std::mutex> lock(queues_mutex);
    auto it = find_queue_by_name(queue_name);
    if (it != existing_queues.end()) {
        exists = true;
        auto now = std::chrono::steady_clock::now();

        uint32_t n_len = htonl(static_cast<uint32_t>(queue_name.length()));
        internal_data.append(reinterpret_cast<const char*>(&n_len), 4);
        internal_data.append(queue_name);

        auto msg_it = it->second.messages.begin();
        while (msg_it != it->second.messages.end()) {
            if (msg_it->expire <= now) {
                msg_it = it->second.messages.erase(msg_it);
            } else {
                has_messages = true;
                uint32_t m_len = htonl(static_cast<uint32_t>(msg_it->text.length()));
                internal_data.append(reinterpret_cast<const char*>(&m_len), 4);
                internal_data.append(msg_it->text);
                ++msg_it;
            }
        }
    }
    }

    if (!exists || !has_messages) return;

    safe_print("Queue: " + queue_name + " | Messages to send: " + std::to_string(internal_data.size()));

    std::string full_packet = prepare_message("MA", internal_data);
    if(!send_message(client.socket, full_packet)){
        safe_error("SEND_ERROR: MA to socket:" + std::to_string(client.socket));
    }
    return;
}

void notify_after_delete(const std::vector<std::string>& ids, const std::string &queue_name){
    std::string packet = prepare_message("ND", queue_name + " was deleted");
    for (auto const& id : ids){
        int sock = -1;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            if (clients.count(id)) {
                sock = clients[id].socket;
            }
        }
        if(sock != -1) {
            send_message(sock, packet);
        }
    }
}

void subscribe_to_queue(const Client& client, const std::string& queue_name) {
    bool valid_op = false;
    bool already_subscribed = false;
    
    {
        std::lock_guard<std::mutex> lock(queues_mutex);
        
        auto it = find_queue_by_name(queue_name);
        
        if (it != existing_queues.end()) {
            if(!is_client_subscribed(it->second, client.id)){
                it->second.subscribers.push_back(client.id);
                valid_op = true;
            }
            else{
                already_subscribed = true;
            }
        }
    }
    
    if(valid_op){
        safe_print("Subscribed client " + client.id + " to queue: " + queue_name);
        if(!send_message(client.socket, prepare_message("SS","OK"))){
            safe_error("SEND_ERROR: SS:OK to " + client.id);
        }
        send_messages_to_new_subscriber(client, queue_name);
    }
    else{
        if(already_subscribed){
            safe_print("cant subscribe to queue: " + queue_name);
            if(!send_message(client.socket, prepare_message("SS","ER:ALREADY_SUBSCRIBED"))){
                safe_error("SEND_ERROR: SS:ER to " + client.id);
            }
        }
        else{
            if(!send_message(client.socket, prepare_message("SS","ER:NO_QUEUE"))){
                safe_error("SEND_ERROR: SS:ER to " + client.id);
            }
        }
    }
    return;
}

void unsubscribe_from_queue(const Client& client, const std::string& queue_name) {  
    bool valid_op = false;
    bool subscribing = true;
    
    {
        std::lock_guard<std::mutex> lock(queues_mutex);
        
        auto it = find_queue_by_name(queue_name);
        
        if (it != existing_queues.end()) {
            auto sub_it = find_subscriber(it->second, client.id);
            if(sub_it != it->second.subscribers.end()){
                it->second.subscribers.erase(sub_it);
                valid_op = true;
            }
            else{
                subscribing = false;
            }
        }
    }
    
    if(valid_op){
        safe_print("Unsubscribed client " + client.id + " from queue: " + queue_name);
        if(!send_message(client.socket, prepare_message("SU","OK"))){
            safe_error("SEND_ERROR: SU:OK to " + client.id);
        }
    }
    else{
        safe_print("cant unsubscribe from queue: " + queue_name);
        if(!subscribing){
            if(!send_message(client.socket, prepare_message("SU","ER:NOT_SUBSCRIBING"))){
                safe_error("SEND_ERROR: SU:ER to " + client.id);
            }
        }
        else{
            if(!send_message(client.socket, prepare_message("SU","ER:NO_QUEUE"))){
                safe_error("SEND_ERROR: SU:ER to " + client.id);
            }
        }
    }
    return;
}

void create_queue(const Client& client, const std::string& queue_name) {
    Queue new_queue;
    bool valid_op = false;
    {
        std::lock_guard<std::mutex> lock(queues_mutex);

        auto it = find_queue_by_name(queue_name);

        if (it == existing_queues.end()) {
            new_queue.name = queue_name;
            existing_queues[queue_name] = new_queue;
            valid_op = true;
        }
    }
    if(valid_op){
        safe_print("Created Queue: " + queue_name);
        if(!send_message(client.socket, prepare_message("PC","OK"))){
            safe_error("SEND_ERROR: PC:OK to " + client.id);
        }
        broadcast_queues_list();
    }
    else{
        safe_print("cant create queue: " + queue_name);
        std::string msg = prepare_message("PC","ER:QUEUE_EXISTS");
        if(!send_message(client.socket, msg)){
            safe_error("SEND_ERROR: PC:ER to " + client.id);
        }
    }
    return;
}

void delete_queue(const Client& client, const std::string& queue_name) {
    bool valid_op = false;
    std::vector<std::string> ids;
    
    {
        std::lock_guard<std::mutex> lock(queues_mutex);
        
        auto it = find_queue_by_name(queue_name);
        
        if (it != existing_queues.end()) {
            ids = it->second.subscribers;
            existing_queues.erase(it);
            valid_op = true;
        }
    }

    if (valid_op) {
        safe_print("Deleted Queue: " + queue_name);
        std::string msg = prepare_message("PD","OK");
        if(!send_message(client.socket, msg)){
            safe_error("SEND_ERROR: PD:OK to " + client.id);
        }
        
        notify_after_delete(ids, queue_name);
        broadcast_queues_list();
    } else {
        safe_print("Cannot delete queue: " + queue_name + " (not found)");
        std::string msg = prepare_message("PD","ER:NO_QUEUE");
        if(!send_message(client.socket, msg)){
            safe_error("SEND_ERROR: PD:ER to " + client.id);
        }
    }
    
    return;
}

void publish_message_to_queue(const Client& client, const std::string& content) {
    
    if (content.length() < 8) {
        if(!send_message(client.socket, prepare_message("PB", "ER:DATA_TOO_SHORT"))){
            safe_error("SEND_ERROR: PB:ER to " + client.id);
        }
        return;
    }

    uint32_t n_len, n_ttl;
    std::memcpy(&n_len, content.data(), 4);
    std::memcpy(&n_ttl, content.data() + 4, 4);
    
    uint32_t queue_name_size = ntohl(n_len);
    uint32_t ttl = ntohl(n_ttl);

    if (content.length() < (8 + queue_name_size)) {
        if(!send_message(client.socket, prepare_message("PB", "ER:INVALID_DATA"))){
            safe_error("SEND_ERROR: PB:ER to " + client.id);
        }
        return;
    }
    
    std::string queue_name = content.substr(8, queue_name_size);
    std::string message_body = content.substr(8 + queue_name_size);
    auto msg_expire= std::chrono::steady_clock::now() + std::chrono::seconds(ttl);

    std::vector<int> subscribers_sockets;
    bool valid_op = false;

    {
        std::lock_guard<std::mutex> lock(queues_mutex);
        auto it = find_queue_by_name(queue_name);

        if (it != existing_queues.end()) {
            it->second.messages.push_back({message_body,msg_expire});
            
            std::lock_guard<std::mutex> lock_c(clients_mutex);
            for (const std::string& sub_id : it->second.subscribers) {
                if (clients.count(sub_id)) {
                    if(clients[sub_id].socket != -1){
                        subscribers_sockets.push_back(clients[sub_id].socket);
                    }
                }
            }
            valid_op = true;
        }
    }


    if (valid_op) {
        if(!send_message(client.socket, prepare_message("PB", "OK"))){
            safe_error("SEND_ERROR: PB:OK to " + client.id);
        }

        for (int sub_sock : subscribers_sockets) {
            Client temp_client; 
            temp_client.socket = sub_sock;
            send_published_message(temp_client, queue_name, message_body);
        }
        if (DEBUG == 1){
            safe_print("DEBUG: Published to " + queue_name + " for " + std::to_string(subscribers_sockets.size()) + " subs.");
        }
    } 
    else {
        if(!send_message(client.socket, prepare_message("PB", "ER:NO_QUEUE"))){
            safe_error("SEND_ERROR: PB:ER to " + client.id);
        }
    }
}