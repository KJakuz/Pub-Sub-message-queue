#include "message_operations.h"
#include <sys/socket.h>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <algorithm>


std::vector<Queue>::iterator find_queue_by_name(const std::string& queue_name) {
    return std::find_if(Existing_Queues.begin(), Existing_Queues.end(), 
        [&queue_name](const Queue& q){return q.name == queue_name;
        });
}

bool is_client_subscribed(const Queue& queue, const std::string& client_id) {
    auto sub_it = std::find(queue.subscribers.begin(), queue.subscribers.end(), client_id);
    return sub_it != queue.subscribers.end();
}

std::vector<std::string>::iterator find_subscriber(Queue& queue, const std::string& client_id) {
    return std::find(queue.subscribers.begin(), queue.subscribers.end(), client_id);
}

bool queue_exists(const std::string& queue_name) {
    return find_queue_by_name(queue_name) != Existing_Queues.end();
}


void broadcast_queue_list(){
    /*
    SENDING MESSAGE THAT LOOKS LIKE THIS: 
    [TYPE(2b)] [CONTENT_SIZE(4b)] [NUMBER_OF_QUEUES(4b)] [QUEUE1_NAME_SIZE(4b)] [QUEUE1_NAME(n)] [QUEUEX_NAME_SIZE(4b)] [QUEUEX_NAME(n)] 
    */
    std::string internal_data;
   
    {
        std::lock_guard<std::mutex> lock(queues_mutex);
        
        uint32_t queues_count = htonl(static_cast<uint32_t>(Existing_Queues.size()));
        internal_data.append(reinterpret_cast<const char*>(&queues_count), 4);

        for (const auto& q : Existing_Queues) {
            // For every queue: name lenght(4b):Name
            uint32_t n_len = htonl(static_cast<uint32_t>(q.name.length()));
            internal_data.append(reinterpret_cast<const char*>(&n_len), 4);
            internal_data.append(q.name);
        }
    }

    std::string packet = prepare_message("IN", internal_data);

    std::vector<int> target_sockets;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (auto const& [id, client] : clients) {
            target_sockets.push_back(client.socket);
        }
    }

    for (int sock : target_sockets) {
        if (!send_message(sock, packet)) {
            std::cerr << "SEND_ERROR: IN to socket:" << sock << "\n";
        }
    }
}

void send_published_message(Client client,std::string &queue_name, std::string &content){
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
        std::cerr << "SEND_ERROR: MS to socket:" << client.socket << "\n";
    }
}

void send_messages_to_new_subscriber(Client client, std::string queue_name) { 
    /*
    SENDING MESSAGE THAT LOOKS LIKE THIS: 
    [TYPE(2b)] [CONTENT_SIZE(4b)] [QUEUE_NAME_SIZE(4b)] [QUEUE_NAME(n)] [MESSAGE1_SIZE(4b)] [MESSAGE1(n)] ... [MESSAGEn_SIZE(4b)] [MESSAGEn(n)] 
    */
    std::string internal_data;
    bool exists = false;
    bool has_messages = false;
    int debug_count = 0;

    {
    std::lock_guard<std::mutex> lock(queues_mutex);
    auto it = find_queue_by_name(queue_name);
    if (it != Existing_Queues.end()) {
        exists = true;
        auto now = std::chrono::steady_clock::now();

        uint32_t n_len = htonl(static_cast<uint32_t>(queue_name.length()));
        internal_data.append(reinterpret_cast<const char*>(&n_len), 4);
        internal_data.append(queue_name);

        auto msg_it = it->messages.begin();
        while (msg_it != it->messages.end()) {
            if (msg_it->expire <= now) {
                msg_it = it->messages.erase(msg_it);
            } else {
                debug_count++; //CZEMU SIE BUGUJE BEZ TEGO COS ????
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

    std::cout << "Queue: " << queue_name << " | Messages to send: " << debug_count << "\n";

    std::string full_packet = prepare_message("MA", internal_data);
    if(!send_message(client.socket, full_packet)){
        std::cerr << "SEND_ERROR: MA to socket:" << client.socket << "\n";
    }
    return;
}

void notify_after_delete(std::vector<std::string> ids, std::string &queue_name){
    for (auto id: ids){
        if(!send_message(clients[id].socket, prepare_message("ND", queue_name + " was deleted"))){
            std::cerr << "SEND_ERROR: ND to " << id << "\n";
        }
    }
}

void subscribe_to_queue(Client client, std::string queue_name) {
    bool valid_op = false;
    
    {
        std::lock_guard<std::mutex> lock(queues_mutex);
        
        auto it = find_queue_by_name(queue_name);
        
        if (it != Existing_Queues.end()) {
            if(!is_client_subscribed(*it, client.id)){
                it->subscribers.push_back(client.id);
                valid_op = true;
            }
        }
    }
    
    if(valid_op){
        std::cout<<"Subscribed client "<<client.id<<" to queue: "<<queue_name<<"\n";
        if(!send_message(client.socket, prepare_message("SS","OK"))){
            std::cerr << "SEND_ERROR: SS:OK to " << client.id << "\n";
        }
        send_messages_to_new_subscriber(client, queue_name);
    }
    else{
        std::cout<<"cant subscribe to queue: "<<queue_name<<"\n";
        if(!send_message(client.socket, prepare_message("SS","ER:NO_QUEUE"))){
            std::cerr << "SEND_ERROR: SS:ER to " << client.id << "\n";
        }
    }
    return;
}

void unsubscribe_from_queue(Client client, std::string queue_name) {  
    bool valid_op = false;
    
    {
        std::lock_guard<std::mutex> lock(queues_mutex);
        
        auto it = find_queue_by_name(queue_name);
        
        if (it != Existing_Queues.end()) {
            auto sub_it = find_subscriber(*it, client.id);
            if(sub_it != it->subscribers.end()){
                it->subscribers.erase(sub_it);
                valid_op = true;
            }
        }
    }
    
    if(valid_op){
        std::cout<<"Unsubscribed client "<<client.id<<" from queue: "<<queue_name<<"\n";
        if(!send_message(client.socket, prepare_message("SU","OK"))){
            std::cerr << "SEND_ERROR: SU:OK to " << client.id << "\n";
        }
    }
    else{
        std::cout<<"cant unsubscribe from queue: "<<queue_name<<"\n";
        std::string msg = prepare_message("SU","ER:NO_QUEUE");
        if(!send_message(client.socket, msg)){
            std::cerr << "SEND_ERROR: SU:ER to " << client.id << "\n";
        }
    }
    return;
}

void create_queue(Client client, const std::string queue_name) {
    Queue new_queue;
    bool valid_op = false;
    {
        std::lock_guard<std::mutex> lock(queues_mutex);

        auto it = find_queue_by_name(queue_name);

        if (it == Existing_Queues.end()) {
            new_queue.name = queue_name;
            Existing_Queues.push_back(new_queue);
            valid_op = true;
        }
    }
    if(valid_op){
        std::cout<<"Created Queue: "<<queue_name<<"\n";
        if(!send_message(client.socket, prepare_message("PC","OK"))){
            std::cerr << "SEND_ERROR: PC:OK to " << client.id << "\n";
        }
        broadcast_queue_list();
    }
    else{
        std::cout<<"cant create queue: "<<queue_name<<"\n";
        std::string msg = prepare_message("PC","ER:QUEUE_EXISTS");
        if(!send_message(client.socket, msg)){
            std::cerr << "SEND_ERROR: PC:ER to " << client.id << "\n";
        }
    }
    return;
}

void delete_queue(Client client, std::string queue_name) {
    bool valid_op = false;
    std::vector<std::string> ids;
    
    {
        std::lock_guard<std::mutex> lock(queues_mutex);
        
        auto it = find_queue_by_name(queue_name);
        
        if (it != Existing_Queues.end()) {
            ids = it->subscribers;
            Existing_Queues.erase(it);
            valid_op = true;
        }
    }

    if (valid_op) {
        std::cout << "Deleted Queue: " << queue_name << "\n";
        std::string msg = prepare_message("PD","OK");
        if(!send_message(client.socket, msg)){
            std::cerr << "SEND_ERROR: PD:OK to " << client.id << "\n";
        }
        
        notify_after_delete(ids, queue_name);
        broadcast_queue_list();
    } else {
        std::cout << "Cannot delete queue: " << queue_name << " (not found)\n";
        std::string msg = prepare_message("PD","ER:NO_QUEUE");
        if(!send_message(client.socket, msg)){
            std::cerr << "SEND_ERROR: PD:ER to " << client.id << "\n";
        }
    }
    
    return;
}

void publish_message_to_queue(Client client, std::string content) {
    
    if (content.length() < 8) {
        if(!send_message(client.socket, prepare_message("PB", "ER:DATA_TOO_SHORT"))){
            std::cerr << "SEND_ERROR: PB:ER to " << client.id << "\n";
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
            std::cerr << "SEND_ERROR: PB:ER to " << client.id << "\n";
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

        if (it != Existing_Queues.end()) {
            it->messages.push_back({message_body,msg_expire});
            
            std::lock_guard<std::mutex> lock_c(clients_mutex);
            for (const std::string& sub_id : it->subscribers) {
                if (clients.count(sub_id)) {
                    subscribers_sockets.push_back(clients[sub_id].socket);
                }
            }
            valid_op = true;
        }
    }


    if (valid_op) {
        if(!send_message(client.socket, prepare_message("PB", "OK"))){
            std::cerr << "SEND_ERROR: PB:OK to " << client.id << "\n";
        }

        for (int sub_sock : subscribers_sockets) {
            Client temp_client; 
            temp_client.socket = sub_sock;
            send_published_message(temp_client, queue_name, message_body);
        }
        
        std::cout << "DEBUG: Published to " << queue_name << " for " << subscribers_sockets.size() << " subs.\n";
    } 
    else {
        if(!send_message(client.socket, prepare_message("PB", "ER:NO_QUEUE"))){
            std::cerr << "SEND_ERROR: PB:ER to " << client.id << "\n";
        }
    }
}