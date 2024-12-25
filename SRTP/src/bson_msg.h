#ifndef BSON_MSG_H

#define BSON_MSG_H

#include <stdbool.h>
#include "messages.h"

int serialize_iotmsg(const struct srtp_packet* msg, char* buf, uint32_t maxlen);

/* Bson_iotmsg is a set of functions to extract the srtp_packet meaning from bson values */
int bson_iotmsg_string(struct srtp_packet** msg, char* parent, char* key, char* value);
int bson_iotmsg_boolean(struct srtp_packet** msg, char* parent, char* key, bool value);
int bson_iotmsg_int32(struct srtp_packet** msg, char* parent, char* key, int32_t value);
int bson_iotmsg_double(struct srtp_packet** msg, char* parent, char* key, double value);
int bson_iotmsg_binary(struct srtp_packet** msg, char* parent, char* key, void* data);

void init_bson_msg();
void shutdown_bson_msg();

#endif /* end of include guard: BSON_MSG_H */
