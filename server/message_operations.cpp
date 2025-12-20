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


//TODO NOTIFY SUBSCRIBED USERS WHEN QUEUE CHANGED/ DELETE MESSAGE WHEN TTL ENDS

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

    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto const& [id, client] : clients) {
        if (!send_message(client.socket, packet)) {
            std::cout<<"Błąd wysyłki do "<<id<<"\n";
        }
    }
}

void send_messages_to_subscriber(Client client) {  
    std::lock_guard<std::mutex> lock(queues_mutex);
    
    for(auto& queue : Existing_Queues){
        if(is_client_subscribed(queue, client.id) && !queue.messages.empty()){
            std::stringstream ss;
            ss << queue.name << ":\n";
            
            for(const auto& msg : queue.messages){
                ss << "\t " << msg << "\n";
            }
            
            std::string full_response = ss.str();
            ssize_t sent_bytes = send(client.socket, full_response.c_str(), full_response.length(), 0);
            
            if (sent_bytes >= 0) {
                queue.messages.clear();
            } else {
                perror("send failed");
            }
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
        std::string msg = prepare_message("OK","");
        if(!send_message(client.socket, msg)){
            perror("couldnt send message to client: subscribe to queue ok");
        }
    }
    else{
        std::cout<<"cant subscribe to queue: "<<queue_name<<"\n";
        std::string msg = prepare_message("ER","NO_QUEUE");
        if(!send_message(client.socket, msg)){
            perror("couldnt send message to client: subscribe to queue no_queue");
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
        std::string msg = prepare_message("OK","");
        if(!send_message(client.socket, msg)){
            perror("couldnt send message to client: unsubscribe to queue ok");
        }
    }
    else{
        std::cout<<"cant unsubscribe from queue: "<<queue_name<<"\n";
        std::string msg = prepare_message("ER","NO_QUEUE");
        if(!send_message(client.socket, msg)){
            perror("couldnt send message to client: unsubscribe to queue no_queue");
        }
    }
    return;
}

void create_queue(Client client, const std::string queue_name) {
    Queue new_queue;
    bool valid_op = false;
    {
        std::lock_guard<std::mutex> lock(queues_mutex);

        auto it = std::find_if(Existing_Queues.begin(), Existing_Queues.end(), 
            [&queue_name](const Queue& q){return q.name == queue_name;
            });

        if (it == Existing_Queues.end()) {
            new_queue.name = queue_name;
            Existing_Queues.push_back(new_queue);
            valid_op = true;
        }
    }
    if(valid_op){
        std::cout<<"Created Queue: "<<queue_name<<"\n";
        std::string msg = prepare_message("OK","");
        if(!send_message(client.socket, msg)){
            perror("couldnt send message to client: create queue ok");
        }
        broadcast_queue_list();
    }
    else{
        std::cout<<"cant create queue: "<<queue_name<<"\n";
        std::string msg = prepare_message("ER","QUEUE_EXISTS");
        if(!send_message(client.socket, msg)){
            perror("couldnt send message to client: create queue QUEUE_EXISTS");
        }
    }
    return;
}

void delete_queue(Client client, std::string queue_name) {
std::cout << "dq\n";
    bool valid_op = false;
    
    {
        std::lock_guard<std::mutex> lock(queues_mutex);
        
        auto it = std::find_if(Existing_Queues.begin(), Existing_Queues.end(), 
            [&queue_name](const Queue& q){return q.name == queue_name;
            });
        
        if (it != Existing_Queues.end()) {
            Existing_Queues.erase(it);
            valid_op = true;
        }
    }

    if (valid_op) {
        std::cout << "Deleted Queue: " << queue_name << "\n";
        std::string msg = prepare_message("OK","");
        if(!send_message(client.socket, msg)){
            perror("couldnt send message to client: delete queue ok");
        }
        broadcast_queue_list();
    } else {
        std::cout << "Cannot delete queue: " << queue_name << " (not found)\n";
        std::string msg = prepare_message("ER","NO_QUEUE");
        if(!send_message(client.socket, msg)){
            perror("couldnt send message to client: create queue NO_QUEUE");
        }
    }
    
    return;
}

void publish_message_to_queue(Client client, std::string content) {
    bool valid_op = false;
    int queue_name_size;
    std::string queue_name;
    std::string message;

    if (content.length() < 2) {
        std::cout<<"data_to_short publish\n";
        std::string msg = prepare_message("ER","DATA_TO_SHORT");
        if(!send_message(client.socket, msg)){
            perror("couldnt send message to client: publish DATA_TO_SHORT");
        }
        return;
    }

    try {
        queue_name_size = std::stoi(content.substr(0, 2));

        if (content.length() < (2 + queue_name_size)) {
            std::string msg = prepare_message("ER","INVALID_QUEUE_NAME_LENGTH");
            if(!send_message(client.socket, msg)){
                perror("couldnt send message to client: publish INVALID_QUEUE_NAME_LENGTH");
            }
            return;
        }

        queue_name = content.substr(2, queue_name_size);
        message = content.substr(2 + queue_name_size);

        std::cout << "DEBUG: Dlugosc: " << queue_name_size << " Kolejka: " << queue_name << " Msg: " << message << std::endl;
    } 
    catch (...) {
        std::string msg = prepare_message("ER","CONTENT_PARSING_ERROR");
        if(!send_message(client.socket, msg)){
            perror("couldnt send message to client: publish CONTENT_PARSING_ERROR");
        }
        return;
    }


    {
        std::lock_guard<std::mutex> lock(queues_mutex);
        
        auto it = find_queue_by_name(queue_name);
        
        if (it != Existing_Queues.end()) {
            it->messages.push_back(message);
            valid_op = true;
        }
    }
    
    if(valid_op){
        std::cout<<"Published message to queue: "<<queue_name<<"\n";
        std::string msg = prepare_message("OK","");
        if(!send_message(client.socket, msg)){
            perror("couldnt send message to client: publish ok");
        }
    }
    else{
        std::cout<<"cant publish to queue: "<<queue_name<<"\n";
        std::string msg = prepare_message("ER","NO_QUEUE");
        if(!send_message(client.socket, msg)){
            perror("couldnt send message to client: publish NO_QUEUE");
        }
    }
    return;
}
