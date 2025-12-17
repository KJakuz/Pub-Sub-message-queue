#include "MessageQueueClient.h"
#include "Protocol.h"

int main(int argc, char**argv) {

    auto client = MessageQueueClient();
    client.create_queue("Kolejka1");
    // for (unsigned char c : msg)
    // {
    //     printf("%02X ", c);
    // }
    // printf("\n");

    return 0;
}