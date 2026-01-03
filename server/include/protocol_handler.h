/**
 * @file protocol_handler.h
 * @brief Protocol handling: sending and receiving messages.
 */

#ifndef PROTOCOL_HANDLER_H
#define PROTOCOL_HANDLER_H

#include "common.h"

// Status of receive operation
enum class recv_status {
    SUCCESS,
    DISCONNECT,
    NETWORK_ERROR,
    PROTOCOL_ERROR,
    PAYLOAD_TOO_LARGE
};

/**
 * @brief Receives a message from socket.
 * @param sock Socket to receive from.
 * @return std::tuple<recv_status, message_type, std::string> that contains status, type and payload of message.
 */
std::tuple<recv_status, message_type, std::string> recv_message(int sock);

/**
 * @brief Prepares a packet: [TYPE(2b)][SIZE(4b)][PAYLOAD]
 * @param message_type Type of message from message_type enum.
 * @param payload Payload of message.
 * @return std::string that contains prepared packet, ready to be sent.
 */
std::string prepare_message(message_type message_type, const std::string &payload);


/** 
 *@brief Sends data through socket.
 *@param sock Socket to send to.
 *@param data Data to send, it is prepared by prepare_message function.
 *@return bool that contains true if send was successful, false otherwise.
*/
bool send_message(int sock, const std::string &data);

#endif