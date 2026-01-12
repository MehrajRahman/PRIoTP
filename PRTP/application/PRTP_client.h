#ifndef IOTCLIENT_RJLSHY5B
#define IOTCLIENT_RJLSHY5B

#include <stdbool.h>          // Needed for 'bool' type
#include <stdint.h>           // Needed for 'uint32_t' type
#include "../src/messages.h"  // Needed for 'struct PRTP_packet'

enum CLIENT_OPERATION {
  CLIENT_OPERATION_QUERY_SENSOR_LIST = 1,
  CLIENT_OPERATION_SUBSCRIBE_ALL_SENSORS = 2,
  CLIENT_OPERATION_SUBSCRIBE_TO_SENSOR_NONRELIABLE = 3,
  CLIENT_OPERATION_SUBSCRIBE_TO_SENSOR_RELIABLE = 4,
  CLIENT_OPERATION_UNSUBSCRIBE = 5,
  CLIENT_OPERATION_SHUTDOWN = 6
};

#define CHUNK_SIZE (1024)  // 100KB chunks
/* In PRTP_client.h */

typedef struct {
    char filename[256];
    char from_user[32];
    uint32_t total_chunks;
    uint32_t received_chunks;
    uint32_t file_size;
    
    FILE* temp_fp;       // <-- NEW: Handle to write immediately
    bool* received;      // Tracks which chunks we have
    bool active;
} file_transfer_t;

/* * Declare the variable as 'extern'.
 * This tells the compiler: "This variable exists, but it's defined in the .c file"
 */
extern file_transfer_t active_transfer;

/* Function declarations */
int send_client_registration(int sd, const char* client_id);
void on_user_list(struct PRTP_packet* msg);
int send_video_chunked(int sd, const char* to_client_id, const char* video_filename);

// Fixed: Removed the '{' and added ';'
void on_file_transfer_message(struct PRTP_packet* msg);
void on_video_message(struct PRTP_packet* msg);

#endif /* end of include guard: IOTCLIENT_RJLSHY5B */