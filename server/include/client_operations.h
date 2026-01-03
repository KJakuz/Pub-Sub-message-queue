/**
 * @file client_operations.h
 * @brief Client login and reconnection handling.
 */

#ifndef CLIENT_OPERATIONS_H
#define CLIENT_OPERATIONS_H

#include "common.h"
#include "protocol_handler.h"

/**
 * @brief Handles client first connection login or reconnection.
 * @param client Client struct to handle.
 * @param id Id to assign.
 * @return Updated client struct with assigned ID.
 */
Client get_client_id(Client client, std::string &id);

#endif
