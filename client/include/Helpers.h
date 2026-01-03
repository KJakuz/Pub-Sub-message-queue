#include <string>

// @brief Check if queue name is valid.
//
// Queue name should be 1-64 characters.
// Allowed alphanumeric, underscores, and hyphens.
// Must start with a letter.
bool _is_valid_queue_name(const std::string &name);

// @brief Check if TTL has valid value.
//
// TTL should be 1-3600 seconds.
bool _is_valid_ttl(uint32_t ttl);