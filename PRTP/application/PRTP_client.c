// // // #include <stdlib.h>
// // // #include <stdio.h>
// // // #include <string.h>
// // // #include <unistd.h>
// // // #include <errno.h>
// // // #include <poll.h>
// // // #include <sys/time.h>
// // // #include <signal.h>
// // // #include <assert.h>

// // // #include "PRTP_client.h"
// // // #include "../src/utils.h"
// // // #include "../src/client_module.h"
// // // #include "../src/logger.h"
// // // #include "../src/bson_parser.h"
// // // #include "../src/bson_msg.h"
// // // #include "../src/messages.h"
// // // #include "../src/subscriptions.h"
// // // #include "../src/sensor_logger.h"
// // // #include "../src/fragment_buffer.h"


// // // #define HOSTNAME_SIZE 255
// // // #define INPUTSIZE 10

// // // static int operation = -1;
// // // static bool all_reliable = false;
// // // static bool ui_enabled = false;
// // // static struct logger* l = NULL;


// // // /* Client tracks seq numbers and reports missing packets with NACKs when
// // //    unreliable connections are used
// // //    */
// // // static bool active_flow_enabled = false;

// // // void usage() {
// // //   printf("iotclient [-l log_dir] [-a|-A] [-q] [-s <server_ip>] [-p <server_port>] [-r sensor_id] [sensor_id_1, sensor_id_2, ...]\n");
// // //   printf("\t-l\t\t  Specifies the directory for output sensor logs\n");
// // //   printf("\t-s <server_ip>\t  The IP address of the IoT server, default localhost\n");
// // //   printf("\t-p <server_port>  The port number of the IoT server, default 5001\n");
// // //   printf("\t-r <sensor_id>\t  Subscribes reliably to the sensor\n");
// // //   printf("\t-H <client_ip>\t  The IP address of the client, default localhost\n");
// // //   printf("\t-a(-A)\t\t  Subscribes (reliably) to all sensors returned by the server\n");
// // //   printf("\t-q\t\t  Queries the list of available sensor IDs from server and prints IDs into stdout and terminates the execution\n");
// // //   printf("\t-g\t\t  Turns on prompting for operation mode\n");
// // //   printf("\t-N\t\t  Turns on NACKs when weakly realible transport is not used\n");
// // // }

// // // void on_subscription_ack_sids(struct iotmsg_node * node) 
// // // {
// // //   for (; node != NULL; node = node->next ) {
// // //      log_print( l, "what is happening\n");
// // //     switch (((struct iotmsg_subscribe_ack_node *) node)->status) {
// // //     case SUBSCRIBE_OK:
// // //       log_print( l, "subscription ok for %s \n", node->id);
// // //       add_sensor_logger( node->id );
// // //       add_fragment_buffer( node->id );
// // //       /* XXX: No need to add flows with reliable subscription here */
// // //       if( active_flow_enabled )
// // //         add_active_flow( node->id );
// // //       if( parse_sensor_type( node->id ) == CAMERA ) add_sensor_dump(node->id);
// // //       break;
// // //     case SUBSCRIBE_ALREADY_EXISTS:
// // //       log_print( l, "subscription already exists for %s \n", node->id);
// // //       break;
// // //     case SUBSCRIBE_NOT_FOUND:
// // //       log_print( l, "subscription failed, no such sensor: %s \n", node->id);
// // //       break;
// // //     default:
// // //       log_print( l, "unknown status code (%d) in subscription ack for %s \n",
// // //           ((struct iotmsg_subscribe_ack_node *) node)->status, node->id);
// // //       break;
// // //     }
// // //     remove_pending_sub_request( node );
// // //   }
// // // }


// // // // void on_update_msg(int sd, struct PRTP_packet* upd_msg)
// // // // {
// // // //   log_error(l,"---before writing in log upd_msg->seq_no= %d\n", upd_msg->seq_no);
// // // //   log_error(l,"---before writing in log upd_msg->type= %d\n", upd_msg->type);
// // // //   log_error(l,"---before writing in log upd_msg->frag_no= %d\n", upd_msg->frag_no);
// // // //   log_error(l,"---before writing in log upd_msg->reliable= %d\n", upd_msg->reliable);
// // // //   log_error(l,"---before writing in log upd_msg->data.sensor_type= %d\n", upd_msg->data.sensor_type);
// // // //   log_error(l,"---before writing in log upd_msg->data.sid= %s\n", upd_msg->data.sid);

// // // //   sensor_log( upd_msg->data.sid, upd_msg->seq_no, upd_msg->data.sensor_type, &(upd_msg->data) );
// // // //   log_error(l,"---after writing in log upd_msg->seq_no = %d\n", upd_msg->seq_no);
// // // //   log_error(l,"---after writing in log upd_msg->data.sensor_type = %d\n", upd_msg->data.sensor_type);
// // // //   log_error(l,"---after writing in log upd_msg->reliable = %d\n", upd_msg->reliable);

// // // //   if( upd_msg->data.sensor_type == CAMERA ) {
// // // //     if( strncmp(((char *)(upd_msg->data.blob)), "NO_MOTION", 9 ) != 0 ) {
// // // //       sensor_dump( upd_msg->data.sid, &(upd_msg->data) );
// // // //     }
// // // //   }
 
// // // //   if( upd_msg->reliable ) {
// // // //     log_debug( l, "Sending acknowledgment.\n");
// // // //     send_update_ack( sd, upd_msg );
// // // //   } else if( active_flow_enabled ) {
// // // //     log_debug( l, "Checking active (unreliable) flows for losses.\n");
// // // //     check_active_flow( upd_msg, sd );
// // // //   }
// // // // }
// // // void on_update_msg(int sd, struct PRTP_packet* upd_msg)
// // // {
// // //   log_error(l,"---before writing in log upd_msg->seq_no= %d\n", upd_msg->seq_no);
// // //   log_error(l,"---before writing in log upd_msg->type= %d\n", upd_msg->type);
// // //   log_error(l,"---before writing in log upd_msg->frag_no= %d\n", upd_msg->frag_no);
// // //   log_error(l,"---before writing in log upd_msg->reliable= %d\n", upd_msg->reliable);
  
// // //   /* Fix: Access through union for UPDATE message */
// // //   log_error(l,"---before writing in log upd_msg->data.sensor_type= %d\n", 
// // //             upd_msg->data.update.sensor_type);
// // //   log_error(l,"---before writing in log upd_msg->data.sid= %s\n", 
// // //             upd_msg->data.update.sid);

// // //   /* Fix: Pass the correct union members */
// // //   sensor_log(upd_msg->data.update.sid, 
// // //              upd_msg->seq_no, 
// // //              upd_msg->data.update.sensor_type, 
// // //              &(upd_msg->data.update.blob));
  
// // //   log_error(l,"---after writing in log upd_msg->seq_no = %d\n", upd_msg->seq_no);
// // //   log_error(l,"---after writing in log upd_msg->data.sensor_type = %d\n", 
// // //             upd_msg->data.update.sensor_type);
// // //   log_error(l,"---after writing in log upd_msg->reliable = %d\n", upd_msg->reliable);

// // //   /* Fix: Access sensor_type through union */
// // //   if(upd_msg->data.update.sensor_type == CAMERA) {
// // //     if(strncmp((char *)(upd_msg->data.update.blob.blob), "NO_MOTION", 9) != 0) {
// // //       /* Fix: Pass the blob struct */
// // //       sensor_dump(upd_msg->data.update.sid, &(upd_msg->data.update.blob));
// // //     }
// // //   }
 
// // //   if(upd_msg->reliable) {
// // //     log_debug(l, "Sending acknowledgment.\n");
// // //     send_update_ack(sd, upd_msg);
// // //   } else if(active_flow_enabled) {
// // //     log_debug(l, "Checking active (unreliable) flows for losses.\n");
// // //     check_active_flow(upd_msg, sd);
// // //   }
// // // }

// // // int client_interface()
// // // {
// // //   int selected;
// // //   printf("Client operation options: \n");
// // //   printf(" 1. Query sensor list \n 2. Subscribe all \n 3. subscribe a particular sensor \n 4. Subscribe with reliability \n 5. Unsubscribe \n 6. Quit\n\n Enter your choice:");
// // //   scanf("%d", &selected);
// // //   return selected;
// // // }


// // // void process_user_input(int sd, int* client_operation)
// // // {
// // //   struct PRTP_packet* sub_msg = NULL;
// // //   char input_sensor[INPUTSIZE];
// // //   switch(*client_operation){
// // //   case CLIENT_OPERATION_QUERY_SENSOR_LIST:
// // //     query_sensor_list(sd);
// // //     break;
// // //   case CLIENT_OPERATION_SUBSCRIBE_ALL_SENSORS:
// // //     /*subscribe to all sensors*/
// // //     query_sensor_list(sd);
// // //     break;
// // //   case CLIENT_OPERATION_SUBSCRIBE_TO_SENSOR_NONRELIABLE:
// // //     /*subscribe to specific sensor*/
// // //     printf("Enter sensor_id =");
// // //     scanf("%s", input_sensor);
// // //     if(!sub_msg ) sub_msg = create_iotmsg(SUBSCRIBE);  
// // //     iotmsg_add_sid(sub_msg, input_sensor);
// // //     break;
// // //   case CLIENT_OPERATION_SUBSCRIBE_TO_SENSOR_RELIABLE:
// // //     /*subscribe to specific sensor with reliability*/
// // //     printf("Enter sensor_id =");
// // //     scanf("%s", input_sensor);
// // //     if(!sub_msg ) sub_msg = create_iotmsg(SUBSCRIBE);
// // //     iotmsg_add_sid(sub_msg, input_sensor );
// // //     iotmsg_set_sid_reliable( sub_msg, true );					
// // //     break;
// // //   case CLIENT_OPERATION_UNSUBSCRIBE: /* 5 */
// // //     /*unsubscribe*/
// // //     send_unsubscribe(sd);
// // //     break;
// // //   case CLIENT_OPERATION_SHUTDOWN:
// // //     /*shut down client*/
// // //     return;
// // //   default:
// // //     break;
// // //   }

// // //   if( sub_msg ) {
// // //     add_pending_subscription((struct PRTP_packet *) sub_msg);
// // //     send_iotmsg(sd, sub_msg);
// // //   }
// // // }

// // // void main_loop(int sd)
// // // {
// // //   struct pollfd fds[2];
// // //   int timeout_msecs = KEEP_ALIVE_INTERVAL;
// // //   struct timeval tv1, tv2, tvelapsed;
// // //   int ret = 0;
// // //   size_t len = 0;
// // //   struct PRTP_packet* msg = NULL;
// // //   int client_operation=-1;
// // //   struct transport aux_tr;
// // //   aux_tr.sd = sd;


// // //   fds[0].fd = sd;
// // //   fds[0].events = POLLIN;
// // //   fds[1].fd = 0;
// // //   fds[1].events = POLLIN;

// // //   for(;;) {
// // //     gettimeofday(&tv1, 0);
// // //     if( ui_enabled ) ret = poll(fds, 2, timeout_msecs);
// // //     else ret = poll(fds, 1, timeout_msecs);
// // //     gettimeofday(&tv2, 0);
// // //     if( ret > 0 ) {
// // //       if( fds[1].revents & POLLIN ) {
// // //         if ( ui_enabled )
// // //           if ((client_operation=client_interface())) {
// // //             process_user_input(sd, &client_operation);
// // //           }
// // //       }
// // //       if( fds[0].revents & POLLIN ) {
// // //         if( !timeval_subtract( &tvelapsed, &tv2, &tv1 ) ) {
// // //           timeout_msecs -= tvelapsed.tv_sec*1000 + tvelapsed.tv_usec/1000;
// // //           if( timeout_msecs < 0 ) timeout_msecs = 0;
// // //         }
		
// // //         msg = NULL;
// // //         len = transport_receive(&aux_tr, &msg, NULL, NULL);
// // //         if( len == -1 ) {
// // //           log_error(l, "Couldn't get a message from the server.\n");
// // //           break;
// // //         } else if( len == 0 ) {
// // //           log_error(l, "Got nothing from the server.\n");
// // //           break;
// // //         } else {
// // //           if(msg == NULL) {
// // //             log_error(l, "Got a malformed message or a fragment from the server.\n");
// // //             continue;
// // //           }
// // //           else {
// // //             log_print(l, "Got a message from the server.\n");
// // //             log_print(l, "Message type is %d\n", msg->type);
// // //           }

// // //           if( msg->type == LIST_RESPONSE ) {
// // //             print_list_response( msg);
// // //             if( client_operation == 2||operation==2 ) {
// // //               log_print(l, "Subscribing to all sensors.\n" );
// // //               send_subscribe_all( sd, (msg)->data.blob, all_reliable );
// // //             }
// // //           }
// // //           else if( msg->type == UPDATE ) {
// // //            log_print(l, "---------Update message arrived------------ %d\n", client_operation!=CLIENT_OPERATION_UNSUBSCRIBE );
// // //             if(client_operation!=CLIENT_OPERATION_UNSUBSCRIBE)
// // //               on_update_msg( sd, (msg) );
// // //           }
// // //           else if( msg->type == SUBSCRIBE_ACK ) {
// // //             log_debug( l, "subscribe ack\n" );
// // //             on_subscription_ack_sids( ((struct PRTP_packet *)msg)->data.blob );
// // //             aux_tr.frag_buffer = get_frag_buffer(NULL);
// // //           }
// // //           else {
// // //             log_print(l, "No handler for message.\n");
// // //           }
// // //           free_iotmsg( msg );
// // //         }
// // //       }
// // //     }
// // //     /* Timeout, need to send KEEP_ALIVE. If client unsubscribe it will not send any keep_alive message */
// // //     if( (ret == 0 || timeout_msecs == 0)&&(client_operation!=CLIENT_OPERATION_UNSUBSCRIBE))
// // //     {
// // //       log_debug(l, "Timeout reached, sending keep_alive.\n");
// // //       send_keep_alive(sd);
// // //       update_pending_subscriptions(sd);
// // //       timeout_msecs = KEEP_ALIVE_INTERVAL;
// // //     }
// // //   } 
// // // }

// // // void shutdown_client()
// // // {
// // //   shutdown_logger(l);
// // //   shutdown_bson_parser();
// // //   shutdown_sensor_logger();
// // //   shutdown_transport();

// // //   shutdown_client_module();
// // // }

// // // void signal_handler(int signum)
// // // {
// // //   log_print(l, "Shutdown.\n");
// // //   shutdown_client();
// // //   exit(EXIT_SUCCESS);
// // // }

// // // int main(int argc, char *argv[])
// // // {
// // //   char server_host[HOSTNAME_SIZE] = "localhost";
// // //   char log_dir[HOSTNAME_SIZE] = "./client_sensor_log";
// // //   char *localaddr = NULL;
// // //   char *endptr;
// // //   int server_port = 5001;
// // //   int c;
// // //   int sd;
// // //   struct PRTP_packet* submsg = NULL;

// // //   if( signal(SIGINT, signal_handler) == SIG_IGN )
// // //     signal(SIGINT, SIG_IGN);
  
// // //   while ((c = getopt(argc, argv, "hl:s:p:qaAr:gNH:")) != -1) {
// // //     switch(c) {
// // //       case 'l':
// // //         if (strlen(optarg) < HOSTNAME_SIZE) {
// // //           strcpy(log_dir, optarg);
// // //         } else {
// // //           log_error(l, "Error: log directory name too long\n");
// // //         }
// // //         break;

// // //       case 's':
// // //         if (strlen(optarg) < HOSTNAME_SIZE) {
// // //           strcpy(server_host, optarg);
// // //         } else {
// // //           log_error(l, "Error: hostname too long\n");
// // //           exit(EXIT_FAILURE);
// // //         }
// // //         break;

// // //       case 'p':
// // //         errno = 0;
// // //         server_port = strtol(optarg, &endptr, 10);
// // //         if ( server_port < 1 || server_port > 65535 ) {
// // //           log_error(l, "Error: invalid port number (should be between 1...65535)\n");
// // //           exit(EXIT_FAILURE);
// // //         }
// // //         break;

// // //       case 'q':
// // //         operation = CLIENT_OPERATION_QUERY_SENSOR_LIST;
// // //         break;

// // //       case 'a':
// // //         operation = CLIENT_OPERATION_SUBSCRIBE_ALL_SENSORS;
// // //         break;

// // //       case 'A':
// // //         operation = CLIENT_OPERATION_SUBSCRIBE_ALL_SENSORS;
// // //         all_reliable = true;
// // //         break;

// // //       case 'g':
// // //         ui_enabled = true;
// // //         break;

// // //       case 'N':
// // //         active_flow_enabled = true;
// // //         break;
// // //       case 'H':
// // //         localaddr = (char*)xalloc(strlen(optarg) + 1);
// // //         strcpy(localaddr, optarg);
// // //         break;
// // //       case 'r':
// // //         if( !submsg ) submsg = create_iotmsg(SUBSCRIBE);
// // //         iotmsg_add_sid( submsg, optarg );
// // //         iotmsg_set_sid_reliable( submsg, true );
// // //         break;
// // //       case 'h':
// // //       default:
// // //         usage();
// // //         exit(EXIT_SUCCESS);
// // //     }
// // //   }

// // //   l = init_logger(stdout, stderr, stderr, "Client");
// // //   init_bson_parser();
// // //   init_client_module();
// // //   init_transport();
// // //   init_sensor_logger(log_dir);

// // //   printf("connceting to %s\n", server_host);
// // //   if (localaddr != NULL) {
// // //     sd = init_socket(server_host, server_port, false, localaddr);
// // //   } else {
// // //     sd = init_socket(server_host, server_port, false, NULL);
// // //   }
// // //   if( sd == -1 ) {
// // //     log_error(l, "Socket init failed\n");
// // //     exit(EXIT_FAILURE);
// // //   }
  
// // //   if( (operation == CLIENT_OPERATION_QUERY_SENSOR_LIST) || (operation == CLIENT_OPERATION_SUBSCRIBE_ALL_SENSORS) ) {
// // //     query_sensor_list(sd);
// // //   } else if (optind < argc) { /* List of sensor IDs given as cmd line parameters */
// // //     uint32_t i;
// // //     /* Might be already created with reliable sids */
// // //     if( !submsg) submsg = create_iotmsg(SUBSCRIBE);
// // //     for (i=optind; i < argc; i++) {
// // //       iotmsg_add_sid(submsg, argv[i]);
// // //     }
// // //   }

// // //   if( submsg ) {
// // //     add_pending_subscription((struct PRTP_packet *) submsg);
// // //     send_iotmsg(sd, submsg);
// // //   }

// // //   if( operation == -1 && !submsg ) {
// // //     usage();
// // //     exit(EXIT_SUCCESS);
// // //   }
// // //   main_loop(sd);

// // //   shutdown_client();
// // //   return EXIT_SUCCESS;
// // // }




// // #include <stdlib.h>
// // #include <stdio.h>
// // #include <string.h>
// // #include <unistd.h>
// // #include <errno.h>
// // #include <poll.h>
// // #include <sys/time.h>
// // #include <signal.h>
// // #include <assert.h>
// // #include <time.h>

// // #include "PRTP_client.h"
// // #include "../src/utils.h"
// // #include "../src/client_module.h"
// // #include "../src/logger.h"
// // #include "../src/bson_parser.h"
// // #include "../src/bson_msg.h"
// // #include "../src/messages.h"
// // #include "../src/subscriptions.h"
// // #include "../src/sensor_logger.h"
// // #include "../src/fragment_buffer.h"

// // #define HOSTNAME_SIZE 255
// // #define INPUTSIZE 512

// // static int operation = -1;
// // static bool all_reliable = false;
// // static bool ui_enabled = false;
// // static struct logger* l = NULL;
// // static bool active_flow_enabled = false;

// // /* Chat-specific global variables */
// // static char my_client_id[32] = {0};

// // /* --- Chat Support Functions (Integrated from chat_client.c) --- */

// // int send_chat_message(int sd, const char* to_client_id, const char* message_text) {
// //     struct PRTP_packet* msg = create_iotmsg(CHAT_MESSAGE);
// //     size_t len = 0;
// //     msg->reliable = true;
// //     msg->data.chat.from_client_id = strdup(my_client_id);
// //     msg->data.chat.to_client_id = to_client_id ? strdup(to_client_id) : NULL;
// //     msg->data.chat.message_text = strdup(message_text);
// //     msg->data.chat.timestamp = (uint32_t)time(NULL);
// //     len = send_iotmsg(sd, msg);
// //     free_iotmsg(msg);
// //     return len;
// // }

// // int request_user_list(int sd) {
// //     struct PRTP_packet* msg = create_iotmsg(CHAT_USER_LIST);
// //     size_t len = send_iotmsg(sd, msg);
// //     free_iotmsg(msg);
// //     return len;
// // }

// // void on_chat_message(struct PRTP_packet* msg) {
// //     time_t timestamp = msg->data.chat.timestamp;
// //     struct tm* timeinfo = localtime(&timestamp);
// //     char time_str[20];
// //     strftime(time_str, sizeof(time_str), "%H:%M:%S", timeinfo);
// //     printf("\n[%s] %s: %s\n", time_str, msg->data.chat.from_client_id, msg->data.chat.message_text);
// //     printf("You> ");
// //     fflush(stdout);
// // }

// // void on_user_list(struct PRTP_packet* msg) {
// //     struct iotmsg_node* node;
// //     int count = 0;
// //     printf("\n=== Online Users ===\n");
// //     for(node = msg->data.blob; node != NULL; node = node->next) {
// //         if(strcmp(node->id, my_client_id) != 0) {
// //             printf("  - %s\n", node->id);
// //             count++;
// //         }
// //     }
// //     if(count == 0) printf("  (No other users online)\n");
// //     printf("====================\n");
// //     printf("You> ");
// //     fflush(stdout);
// // }

// // /* --- Existing Sensor Logic (Modified for Union Access) --- */

// // void on_update_msg(int sd, struct PRTP_packet* upd_msg) {
// //     log_error(l,"---Update for sensor: %s\n", upd_msg->data.update.sid);
// //     sensor_log(upd_msg->data.update.sid, upd_msg->seq_no, upd_msg->data.update.sensor_type, &(upd_msg->data.update.blob));
// //     if(upd_msg->data.update.sensor_type == CAMERA) {
// //         if(strncmp((char *)(upd_msg->data.update.blob.blob), "NO_MOTION", 9) != 0) {
// //             sensor_dump(upd_msg->data.update.sid, &(upd_msg->data.update.blob));
// //         }
// //     }
// //     if(upd_msg->reliable) { send_update_ack(sd, upd_msg); }
// //     else if(active_flow_enabled) { check_active_flow(upd_msg, sd); }
// // }

// // /* --- Unified Input Processing --- */

// // int process_combined_input(int sd, char* input) {
// //     char command[20] = {0};
// //     char target[32] = {0};
// //     char message[500] = {0};

// //     if(input[0] == '/') {
// //         if(sscanf(input, "/%s", command) < 1) return 0;
        
// //         if(strcmp(command, "quit") == 0) return -1;
// //         else if(strcmp(command, "users") == 0) request_user_list(sd);
// //         else if(strcmp(command, "msg") == 0) {
// //             if(sscanf(input, "/msg %s %[^\n]", target, message) < 2) printf("Usage: /msg <user> <text>\n");
// //             else send_chat_message(sd, target, message);
// //         }
// //         else if(strcmp(command, "broadcast") == 0) {
// //             if(sscanf(input, "/broadcast %[^\n]", message) < 1) printf("Usage: /broadcast <text>\n");
// //             else send_chat_message(sd, NULL, message);
// //         }
// //         else printf("Unknown command. Use /users, /msg, /broadcast, or /quit\n");
// //     } else {
// //         // Default behavior: broadcast chat
// //         send_chat_message(sd, NULL, input);
// //     }
// //     return 0;
// // }

// // void on_subscription_ack_sids(struct iotmsg_node * node) 
// // {
// //   for (; node != NULL; node = node->next ) {
// //     switch (((struct iotmsg_subscribe_ack_node *) node)->status) {
// //     case SUBSCRIBE_OK:
// //       log_print( l, "subscription ok for %s \n", node->id);
// //       add_sensor_logger( node->id );
// //       add_fragment_buffer( node->id );
// //       if( active_flow_enabled )
// //         add_active_flow( node->id );
// //       if( parse_sensor_type( node->id ) == CAMERA ) add_sensor_dump(node->id);
// //       break;
// //     case SUBSCRIBE_ALREADY_EXISTS:
// //       log_print( l, "subscription already exists for %s \n", node->id);
// //       break;
// //     case SUBSCRIBE_NOT_FOUND:
// //       log_print( l, "subscription failed, no such sensor: %s \n", node->id);
// //       break;
// //     default:
// //       log_print( l, "unknown status code (%d) in subscription ack for %s \n",
// //           ((struct iotmsg_subscribe_ack_node *) node)->status, node->id);
// //       break;
// //     }
// //     remove_pending_sub_request( node );
// //   }
// // }

// // void main_loop(int sd) {
// //     struct pollfd fds[2];
// //     int timeout_msecs = KEEP_ALIVE_INTERVAL;
// //     struct timeval tv1, tv2, tvelapsed;
// //     struct transport aux_tr;
// //     char input_buffer[INPUTSIZE];
// //     aux_tr.sd = sd;

// //     snprintf(my_client_id, sizeof(my_client_id), "user_%d", getpid());
// //     printf("Chat and Sensor Client Active. ID: %s\nType /users to see who is online.\n", my_client_id);
// //     printf("You> ");
// //     fflush(stdout);

// //     fds[0].fd = sd; fds[0].events = POLLIN;
// //     fds[1].fd = STDIN_FILENO; fds[1].events = POLLIN;

// //     for(;;) {
// //         gettimeofday(&tv1, 0);
// //         int ret = poll(fds, 2, timeout_msecs);
// //         gettimeofday(&tv2, 0);

// //         if(ret > 0) {
// //             // Handle User Keyboard Input
// //             if(fds[1].revents & POLLIN) {
// //                 if(fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
// //                     input_buffer[strcspn(input_buffer, "\n")] = 0;
// //                     if(strlen(input_buffer) > 0) {
// //                         if(process_combined_input(sd, input_buffer) == -1) break;
// //                     }
// //                     printf("You> ");
// //                     fflush(stdout);
// //                 }
// //             }

// //             // Handle Incoming Network Messages
// //             if(fds[0].revents & POLLIN) {
// //                 struct PRTP_packet* msg = NULL;
// //                 size_t len = transport_receive(&aux_tr, &msg, NULL, NULL);
                
// //                 if(len <= 0) { printf("\nConnection lost\n"); break; }
                
// //                 if(msg != NULL) {
// //                     if(msg->type == UPDATE) on_update_msg(sd, msg);
// //                     else if(msg->type == CHAT_MESSAGE) on_chat_message(msg);
// //                     else if(msg->type == CHAT_USER_LIST) on_user_list(msg);
// //                     else if(msg->type == SUBSCRIBE_ACK) on_subscription_ack_sids(msg->data.blob);
                    
// //                     free_iotmsg(msg);
// //                 }
// //             }
// //         }

// //         if(ret == 0 || timeout_msecs <= 0) {
// //             send_keep_alive(sd);
// //             timeout_msecs = KEEP_ALIVE_INTERVAL;
// //         }
// //     } 
// // }

// // /* --- Main function remains largely the same but triggers the unified loop --- */
// // int main(int argc, char *argv[]) {
// //     // ... (Keep existing argument parsing logic for -s, -p, -r, etc.) ...
// //     // Ensure all subsystems are initialized
// //     l = init_logger(stdout, stderr, stderr, "PRTP_Client");
// //     init_bson_parser();
// //     init_client_module();
// //     init_transport();

// //     int sd = init_socket("localhost", 5001, false, NULL); // Simplified for example
// //     if(sd == -1) exit(EXIT_FAILURE);

// //     main_loop(sd);

// //     close(sd);
// //     return EXIT_SUCCESS;
// // }


// #include <stdlib.h>
// #include <stdio.h>
// #include <string.h>
// #include <unistd.h>
// #include <errno.h>
// #include <poll.h>
// #include <sys/time.h>
// #include <signal.h>
// #include <assert.h>
// #include <time.h>

// #include "PRTP_client.h"
// #include "../src/utils.h"
// #include "../src/client_module.h"
// #include "../src/logger.h"
// #include "../src/bson_parser.h"
// #include "../src/bson_msg.h"
// #include "../src/messages.h"
// #include "../src/subscriptions.h"
// #include "../src/sensor_logger.h"
// #include "../src/fragment_buffer.h"

// #define HOSTNAME_SIZE 255
// #define INPUTSIZE 512

// static struct logger* l = NULL;
// static bool active_flow_enabled = false;
// static char my_client_id[32] = {0};

// /* --- Subscription Handler (Restored to fix Undefined Reference) --- */
// void on_subscription_ack_sids(struct iotmsg_node * node) 
// {
//   for (; node != NULL; node = node->next ) {
//     switch (((struct iotmsg_subscribe_ack_node *) node)->status) {
//     case SUBSCRIBE_OK:
//       log_print( l, "subscription ok for %s \n", node->id);
//       add_sensor_logger( node->id );
//       add_fragment_buffer( node->id );
//       if( active_flow_enabled )
//         add_active_flow( node->id );
//       if( parse_sensor_type( node->id ) == CAMERA ) add_sensor_dump(node->id);
//       break;
//     case SUBSCRIBE_ALREADY_EXISTS:
//       log_print( l, "subscription already exists for %s \n", node->id);
//       break;
//     case SUBSCRIBE_NOT_FOUND:
//       log_print( l, "subscription failed, no such sensor: %s \n", node->id);
//       break;
//     default:
//       log_print( l, "unknown status code (%d) in subscription ack for %s \n",
//           ((struct iotmsg_subscribe_ack_node *) node)->status, node->id);
//       break;
//     }
//     remove_pending_sub_request( node );
//   }
// }

// /* --- Chat Support Functions --- */
// int send_chat_message(int sd, const char* to_client_id, const char* message_text) {
//     struct PRTP_packet* msg = create_iotmsg(CHAT_MESSAGE);
//     msg->reliable = true;
//     msg->data.chat.from_client_id = strdup(my_client_id);
//     msg->data.chat.to_client_id = to_client_id ? strdup(to_client_id) : NULL;
//     msg->data.chat.message_text = strdup(message_text);
//     msg->data.chat.timestamp = (uint32_t)time(NULL);
//     size_t len = send_iotmsg(sd, msg);
//     free_iotmsg(msg);
//     return (int)len;
// }

// int request_user_list(int sd) {
//     struct PRTP_packet* msg = create_iotmsg(CHAT_USER_LIST);
//     size_t len = send_iotmsg(sd, msg);
//     free_iotmsg(msg);
//     return (int)len;
// }

// void on_chat_message(struct PRTP_packet* msg) {
//     time_t timestamp = msg->data.chat.timestamp;
//     struct tm* timeinfo = localtime(&timestamp);
//     char time_str[20];
//     strftime(time_str, sizeof(time_str), "%H:%M:%S", timeinfo);
//     printf("\n[%s] %s: %s\n", time_str, msg->data.chat.from_client_id, msg->data.chat.message_text);
//     printf("You> ");
//     fflush(stdout);
// }

// void on_user_list(struct PRTP_packet* msg) {
//     struct iotmsg_node* node;
//     int count = 0;
//     printf("\n=== Online Users ===\n");
//     for(node = msg->data.blob; node != NULL; node = node->next) {
//         if(strcmp(node->id, my_client_id) != 0) {
//             printf("  - %s\n", node->id);
//             count++;
//         }
//     }
//     if(count == 0) printf("  (No other users online)\n");
//     printf("====================\n");
//     printf("You> ");
//     fflush(stdout);
// }

// /* --- Sensor Update Logic --- */
// void on_update_msg(int sd, struct PRTP_packet* upd_msg) {
//     sensor_log(upd_msg->data.update.sid, upd_msg->seq_no, upd_msg->data.update.sensor_type, &(upd_msg->data.update.blob));
//     if(upd_msg->data.update.sensor_type == CAMERA) {
//         if(strncmp((char *)(upd_msg->data.update.blob.blob), "NO_MOTION", 9) != 0) {
//             sensor_dump(upd_msg->data.update.sid, &(upd_msg->data.update.blob));
//         }
//     }
//     if(upd_msg->reliable) { send_update_ack(sd, upd_msg); }
//     else if(active_flow_enabled) { check_active_flow(upd_msg, sd); }
// }

// /* --- Unified Input Processing --- */
// int process_combined_input(int sd, char* input) {
//     char command[20] = {0};
//     char target[32] = {0};
//     char message[500] = {0};

//     if(input[0] == '/') {
//         if(sscanf(input, "/%s", command) < 1) return 0;
//         if(strcmp(command, "quit") == 0) return -1;
//         else if(strcmp(command, "users") == 0) request_user_list(sd);
//         else if(strcmp(command, "msg") == 0) {
//             if(sscanf(input, "/msg %s %[^\n]", target, message) < 2) printf("Usage: /msg <user> <text>\n");
//             else send_chat_message(sd, target, message);
//         }
//         else if(strcmp(command, "broadcast") == 0) {
//             if(sscanf(input, "/broadcast %[^\n]", message) < 1) printf("Usage: /broadcast <text>\n");
//             else send_chat_message(sd, NULL, message);
//         }
//         else printf("Unknown command.\n");
//     } else {
//         send_chat_message(sd, NULL, input);
//     }
//     return 0;
// }
// void main_loop(int sd) {
//     struct pollfd fds[2];
//     int timeout_msecs = KEEP_ALIVE_INTERVAL;
//     struct timeval tv1, tv2, tvelapsed;
//     struct transport aux_tr;
//     char input_buffer[INPUTSIZE];
//     aux_tr.sd = sd;

//     snprintf(my_client_id, sizeof(my_client_id), "user_%d", getpid());
//     printf("Chat/Sensor Client Started. ID: %s\n", my_client_id);
    
//     // Send ONLY the registration message with client ID
//     send_client_registration(sd, my_client_id);
    
//     // Small delay to ensure registration is processed
//     usleep(100000); // 100ms
    
//     printf("You> ");
//     fflush(stdout);

//     fds[0].fd = sd; fds[0].events = POLLIN;
//     fds[1].fd = STDIN_FILENO; fds[1].events = POLLIN;

//     for(;;) {
//         gettimeofday(&tv1, 0);
//         int ret = poll(fds, 2, timeout_msecs);
//         gettimeofday(&tv2, 0);

//         if(ret > 0) {
//             if(fds[1].revents & POLLIN) {
//                 if(fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
//                     input_buffer[strcspn(input_buffer, "\n")] = 0;
//                     if(strlen(input_buffer) > 0) {
//                         if(process_combined_input(sd, input_buffer) == -1) break;
//                     }
//                     printf("You> ");
//                     fflush(stdout);
//                 }
//             }
//             if(fds[0].revents & POLLIN) {
//                 if(!timeval_subtract(&tvelapsed, &tv2, &tv1)) {
//                     timeout_msecs -= (int)(tvelapsed.tv_sec*1000 + tvelapsed.tv_usec/1000);
//                     if(timeout_msecs < 0) timeout_msecs = 0;
//                 }
//                 struct PRTP_packet* msg = NULL;
//                 size_t len = transport_receive(&aux_tr, &msg, NULL, NULL);
//                 if(len <= 0) break;
//                 if(msg != NULL) {
//                     if(msg->type == UPDATE) on_update_msg(sd, msg);
//                     else if(msg->type == CHAT_MESSAGE) on_chat_message(msg);
//                     else if(msg->type == CHAT_ROOM_JOIN) {
//                         printf("\n[SYSTEM] %s joined the chat\n", msg->data.chat.from_client_id);
//                         printf("You> ");
//                         fflush(stdout);
//                     }
//                     else if(msg->type == CHAT_USER_LIST) on_user_list(msg);
//                     else if(msg->type == SUBSCRIBE_ACK) on_subscription_ack_sids(msg->data.blob);
                    
//                     free_iotmsg(msg);
//                 }
//             }
//         }
//         if(ret == 0 || timeout_msecs <= 0) {
//             send_keep_alive(sd);
//             timeout_msecs = KEEP_ALIVE_INTERVAL;
//         }
//     } 
// }
// void on_user_list(struct PRTP_packet* msg) {
//     struct iotmsg_node* node;
//     int count = 0;
    
//     printf("\n=== DEBUG: User List Received ===\n");
//     printf("Message type: %d\n", msg->type);
//     printf("Data blob pointer: %p\n", (void*)msg->data.blob);
    
//     printf("\n=== Online Users ===\n");
//     for(node = msg->data.blob; node != NULL; node = node->next) {
//         printf("DEBUG: Found node with ID: %s\n", node->id);
//         if(strcmp(node->id, my_client_id) != 0) {
//             printf("  - %s\n", node->id);
//             count++;
//         } else {
//             printf("DEBUG: Skipping self: %s\n", node->id);
//         }
//     }
//     if(count == 0) printf("  (No other users online)\n");
//     printf("====================\n");
//     printf("DEBUG: Total nodes: %d, Other users: %d\n", count + (node ? 1 : 0), count);
//     printf("You> ");
//     fflush(stdout);
// }
// void signal_handler(int signum) {
//     exit(EXIT_SUCCESS);
// }
// int send_client_registration(int sd, const char* client_id) {
//     struct PRTP_packet* msg = create_iotmsg(CHAT_ROOM_JOIN);
//     msg->reliable = true;
//     msg->data.chat.from_client_id = strdup(client_id);
//     msg->data.chat.to_client_id = NULL;
//     msg->data.chat.message_text = strdup("");  // Empty message
//     msg->data.chat.timestamp = (uint32_t)time(NULL);
//     size_t len = send_iotmsg(sd, msg);
//     free_iotmsg(msg);
//     return (int)len;
// }
// int main(int argc, char *argv[]) {
//     char server_host[HOSTNAME_SIZE] = "localhost";
//     char log_dir[HOSTNAME_SIZE] = "./client_sensor_log";
//     int server_port = 5001;
//     int c;
//     struct PRTP_packet* submsg = NULL;

//     signal(SIGINT, signal_handler);

//     // FIX: Add 'l:' and 'r:' to the option string
//     while ((c = getopt(argc, argv, "l:s:p:r:aAN")) != -1) {
//         switch(c) {
//             case 'l':
//                 if (strlen(optarg) < HOSTNAME_SIZE) {
//                     strcpy(log_dir, optarg);
//                 } else {
//                     fprintf(stderr, "Error: log directory name too long\n");
//                     exit(EXIT_FAILURE);
//                 }
//                 break;
//             case 's': 
//                 strncpy(server_host, optarg, HOSTNAME_SIZE-1); 
//                 break;
//             case 'p': 
//                 server_port = atoi(optarg); 
//                 break;
//             case 'r':
//                 if(!submsg) submsg = create_iotmsg(SUBSCRIBE);
//                 iotmsg_add_sid(submsg, optarg);
//                 iotmsg_set_sid_reliable(submsg, true);
//                 break;
//             case 'a':
//                 // Subscribe to all sensors (non-reliable)
//                 // You'll need to implement query_sensor_list
//                 break;
//             case 'A':
//                 // Subscribe to all sensors (reliable)
//                 break;
//             case 'N': 
//                 active_flow_enabled = true; 
//                 break;
//             case 'h':
//             default:
//                 fprintf(stderr, "Usage: %s [-l log_dir] [-s server_ip] [-p port] [-r sensor_id] [-N]\n", argv[0]);
//                 exit(EXIT_FAILURE);
//         }
//     }

//     l = init_logger(stdout, stderr, stderr, "Client");
//     init_bson_parser();
//     init_client_module();
//     init_transport();
//     init_sensor_logger(log_dir);  // Use the log_dir variable

//     int sd = init_socket(server_host, server_port, false, NULL);
//     if(sd == -1) {
//         fprintf(stderr, "Failed to connect to server\n");
//         exit(EXIT_FAILURE);
//     }

//     // Send subscription message if created
//     if(submsg) {
//         send_iotmsg(sd, submsg);
//         free_iotmsg(submsg);
//     }

//     main_loop(sd);
//     return EXIT_SUCCESS;
// }


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h> // Required for getcwd
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/time.h>
#include <signal.h>
#include <assert.h>
#include <time.h>

#include "PRTP_client.h"
#include "../src/utils.h"
#include "../src/client_module.h"
#include "../src/logger.h"
#include "../src/bson_parser.h"
#include "../src/bson_msg.h"
#include "../src/messages.h"
#include "../src/subscriptions.h"
#include "../src/sensor_logger.h"
#include "../src/fragment_buffer.h"

#define HOSTNAME_SIZE 255
#define INPUTSIZE 512

static struct logger* l = NULL;
static bool active_flow_enabled = false;
static char my_client_id[32] = {0};
file_transfer_t active_transfer = {0};

/* --- Chat Support Functions --- */
int send_chat_message(int sd, const char* to_client_id, const char* message_text) {
    struct PRTP_packet* msg = create_iotmsg(CHAT_MESSAGE);
    msg->reliable = true;
    msg->data.chat.from_client_id = strdup(my_client_id);
    msg->data.chat.to_client_id = to_client_id ? strdup(to_client_id) : NULL;
    msg->data.chat.message_text = strdup(message_text);
    msg->data.chat.timestamp = (uint32_t)time(NULL);
    size_t len = send_iotmsg(sd, msg);
    free_iotmsg(msg);
    return (int)len;
}

int request_user_list(int sd) {
    struct PRTP_packet* msg = create_iotmsg(CHAT_USER_LIST);
    size_t len = send_iotmsg(sd, msg);
    free_iotmsg(msg);
    return (int)len;
}

// void on_chat_message(struct PRTP_packet* msg) {
//     time_t timestamp = msg->data.chat.timestamp;
//     struct tm* timeinfo = localtime(&timestamp);
//     char time_str[20];
//     strftime(time_str, sizeof(time_str), "%H:%M:%S", timeinfo);
//     printf("\n[%s] %s: %s\n", time_str, msg->data.chat.from_client_id, msg->data.chat.message_text);
//     printf("You> ");
//     fflush(stdout);
// }

void on_user_list(struct PRTP_packet* msg) {
    struct iotmsg_node* node;
    int count = 0;
    
    printf("\n=== DEBUG: User List Received ===\n");
    printf("Message type: %d\n", msg->type);
    printf("Data blob pointer: %p\n", (void*)msg->data.blob);
    
    printf("\n=== Online Users ===\n");
    for(node = msg->data.blob; node != NULL; node = node->next) {
        printf("DEBUG: Found node with ID: %s\n", node->id);
        if(strcmp(node->id, my_client_id) != 0) {
            printf("  - %s\n", node->id);
            count++;
        } else {
            printf("DEBUG: Skipping self: %s\n", node->id);
        }
    }
    if(count == 0) printf("  (No other users online)\n");
    printf("====================\n");
    printf("DEBUG: Total other users: %d\n", count);
    printf("You> ");
    fflush(stdout);
}

/* --- Subscription Handler --- */
void on_subscription_ack_sids(struct iotmsg_node * node) 
{
  for (; node != NULL; node = node->next ) {
    switch (((struct iotmsg_subscribe_ack_node *) node)->status) {
    case SUBSCRIBE_OK:
      log_print( l, "subscription ok for %s \n", node->id);
      add_sensor_logger( node->id );
      add_fragment_buffer( node->id );
      if( active_flow_enabled )
        add_active_flow( node->id );
      if( parse_sensor_type( node->id ) == CAMERA ) add_sensor_dump(node->id);
      break;
    case SUBSCRIBE_ALREADY_EXISTS:
      log_print( l, "subscription already exists for %s \n", node->id);
      break;
    case SUBSCRIBE_NOT_FOUND:
      log_print( l, "subscription failed, no such sensor: %s \n", node->id);
      break;
    default:
      log_print( l, "unknown status code (%d) in subscription ack for %s \n",
          ((struct iotmsg_subscribe_ack_node *) node)->status, node->id);
      break;
    }
    remove_pending_sub_request( node );
  }
}

/* --- Sensor Update Logic --- */
void on_update_msg(int sd, struct PRTP_packet* upd_msg) {
    sensor_log(upd_msg->data.update.sid, upd_msg->seq_no, upd_msg->data.update.sensor_type, &(upd_msg->data.update.blob));
    if(upd_msg->data.update.sensor_type == CAMERA) {
        if(strncmp((char *)(upd_msg->data.update.blob.blob), "NO_MOTION", 9) != 0) {
            sensor_dump(upd_msg->data.update.sid, &(upd_msg->data.update.blob));
        }
    }
    if(upd_msg->reliable) { send_update_ack(sd, upd_msg); }
    else if(active_flow_enabled) { check_active_flow(upd_msg, sd); }
}

/* --- Input Processing --- */
// int process_combined_input(int sd, char* input) {
//     char command[20] = {0};
//     char target[32] = {0};
//     char message[500] = {0};

//     if(input[0] == '/') {
//         if(sscanf(input, "/%s", command) < 1) return 0;
//         if(strcmp(command, "quit") == 0) return -1;
//         else if(strcmp(command, "users") == 0) request_user_list(sd);
//         else if(strcmp(command, "msg") == 0) {
//             if(sscanf(input, "/msg %s %[^\n]", target, message) < 2) printf("Usage: /msg <user> <text>\n");
//             else send_chat_message(sd, target, message);
//         }
//         else if(strcmp(command, "broadcast") == 0) {
//             if(sscanf(input, "/broadcast %[^\n]", message) < 1) printf("Usage: /broadcast <text>\n");
//             else send_chat_message(sd, NULL, message);
//         }
//         else printf("Unknown command.\n");
//     } else {
//         send_chat_message(sd, NULL, input);
//     }
//     return 0;
// }













/* Send video in chunks */
int send_video_chunked(int sd, const char* to_client_id, const char* video_filename) {
    // Remove quotes if present
    char clean_filename[512];
    const char* src = video_filename;
    char* dst = clean_filename;
    
    while(*src && dst < clean_filename + sizeof(clean_filename) - 1) {
        if(*src != '"' && *src != '\'') {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';
    
    FILE* fp = fopen(clean_filename, "rb");
    if(!fp) {
        printf("✗ Error: Cannot open video file '%s'\n", clean_filename);
        return -1;
    }
    
    // Get file size
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    uint32_t total_chunks = (file_size + CHUNK_SIZE - 1) / CHUNK_SIZE;
    
    printf("\n Sending: %s\n", clean_filename);
    printf("   Size: %.2f MB (%ld bytes)\n", file_size / (1024.0 * 1024.0), file_size);
    printf("   Chunks: %u (100KB each)\n", total_chunks);
    printf("   To: %s\n\n", to_client_id);
    
    // Extract just the filename (remove path)
    const char* base_filename = strrchr(clean_filename, '/');
    base_filename = base_filename ? base_filename + 1 : clean_filename;
    
    // Send START message
    char start_msg[512];
    snprintf(start_msg, sizeof(start_msg), 
             "FILE_START:%s:%ld:%u", base_filename, file_size, total_chunks);
    send_chat_message(sd, to_client_id, start_msg);
    
    usleep(50000); // 50ms delay for START to arrive first
    
    // Send chunks
    char* buffer = malloc(CHUNK_SIZE);
    for(uint32_t chunk = 0; chunk < total_chunks; chunk++) {
        size_t bytes_read = fread(buffer, 1, CHUNK_SIZE, fp);
        
        // Encode chunk as base64 to safely send in text message
        // For simplicity, we'll use hex encoding (2 chars per byte)
        char* hex_data = malloc(bytes_read * 2 + 1);
        for(size_t i = 0; i < bytes_read; i++) {
            sprintf(hex_data + i * 2, "%02x", (unsigned char)buffer[i]);
        }
        hex_data[bytes_read * 2] = '\0';
        
        // Send chunk message: "FILE_CHUNK:<chunk_num>:<total>:<hex_data>"
        char* chunk_msg = malloc(strlen(hex_data) + 100);
        sprintf(chunk_msg, "FILE_CHUNK:%u:%u:%s", chunk, total_chunks, hex_data);
        
        send_chat_message(sd, to_client_id, chunk_msg);
        
        free(hex_data);
        free(chunk_msg);
        
        // Progress
        if(chunk % 10 == 0 || chunk == total_chunks - 1) {
            printf("\r   Progress: %u/%u chunks (%.1f%%)   ", 
                   chunk + 1, total_chunks, 
                   (chunk + 1) * 100.0 / total_chunks);
            fflush(stdout);
        }
        
        // Small delay to avoid overwhelming the network
        usleep(50000);  // 5ms between chunks
    }
    
    printf("\n✓ Transfer complete!\n");
    
    free(buffer);
    fclose(fp);
    return 0;
}

/* In PRTP_client.c */

void on_file_transfer_message(struct PRTP_packet* msg) {
    const char* text = msg->data.chat.message_text;
    
    // --- 1. Handle FILE_START ---
    if(strncmp(text, "FILE_START:", 11) == 0) {
        char filename[256];
        uint32_t file_size, total_chunks;
        
        if(sscanf(text, "FILE_START:%255[^:]:%u:%u", 
                  filename, &file_size, &total_chunks) == 3) {
            
            // Cleanup previous transfer if it got stuck
            if(active_transfer.active) {
                printf("\n⚠️  Warning: New transfer started, closing previous\n");
                if(active_transfer.temp_fp) fclose(active_transfer.temp_fp);
                if(active_transfer.received) free(active_transfer.received);
            }
            
            // Init new transfer
            memset(&active_transfer, 0, sizeof(active_transfer));
            strncpy(active_transfer.filename, filename, sizeof(active_transfer.filename) - 1);
            strncpy(active_transfer.from_user, msg->data.chat.from_client_id, 
                    sizeof(active_transfer.from_user) - 1);
            
            active_transfer.file_size = file_size;
            active_transfer.total_chunks = total_chunks;
            active_transfer.received = calloc(total_chunks, sizeof(bool));
            active_transfer.active = true;
            
            // Open file IMMEDIATELY for writing
            char output_filename[300];
            snprintf(output_filename, sizeof(output_filename), "received_%s", filename);
            active_transfer.temp_fp = fopen(output_filename, "r+b"); // Try open existing
            if(!active_transfer.temp_fp) {
                active_transfer.temp_fp = fopen(output_filename, "wb"); // Create new
            }
            
            if(!active_transfer.temp_fp) {
                printf("✗ Critical Error: Cannot create file on disk!\n");
                active_transfer.active = false;
                return;
            }

            printf("\n Receiving file: %s (Streaming to disk)\n", filename);
            printf("   Size: %.2f MB | Chunks: %u\n\n", file_size / (1024.0 * 1024.0), total_chunks);
        }
        return;
    }
    
    // --- 2. Handle FILE_CHUNK ---
    if(strncmp(text, "FILE_CHUNK:", 11) == 0) {
        if(!active_transfer.active || !active_transfer.temp_fp) return;
        
        uint32_t chunk_num, total_chunks;
        const char* hex_data_start = strchr(text + 11, ':');
        if(!hex_data_start) return;
        hex_data_start = strchr(hex_data_start + 1, ':');
        if(!hex_data_start) return;
        hex_data_start++; // Point to data
        
        if(sscanf(text, "FILE_CHUNK:%u:%u:", &chunk_num, &total_chunks) != 2) return;
        
        // Ignore duplicates
        if(chunk_num >= active_transfer.total_chunks || active_transfer.received[chunk_num]) return;

        // Decode Hex
        size_t hex_len = strlen(hex_data_start);
        size_t data_len = hex_len / 2;
        unsigned char* data = malloc(data_len);
        
        for(size_t i = 0; i < data_len; i++) {
            sscanf(hex_data_start + i * 2, "%2hhx", &data[i]);
        }
        
        // --- CRITICAL FIX: Write to Disk Immediately ---
        // Calculate offset: Chunk_ID * Chunk_Size
        long offset = (long)chunk_num * CHUNK_SIZE;
        
        fseek(active_transfer.temp_fp, offset, SEEK_SET);
        fwrite(data, 1, data_len, active_transfer.temp_fp);
        // -----------------------------------------------

        free(data); // Clear RAM immediately
        
        active_transfer.received[chunk_num] = true;
        active_transfer.received_chunks++;
        
        // Print Progress (only occasionally)
        if(active_transfer.received_chunks % 10 == 0 || 
           active_transfer.received_chunks == active_transfer.total_chunks) {
            printf("\r   Receiving: %u/%u (%.1f%%)   ", 
                   active_transfer.received_chunks,
                   active_transfer.total_chunks,
                   active_transfer.received_chunks * 100.0 / active_transfer.total_chunks);
            fflush(stdout);
        }
        
        // Completion Check
        if(active_transfer.received_chunks == active_transfer.total_chunks) {
            printf("\n✓ Transfer complete!\n");
            fclose(active_transfer.temp_fp);
            active_transfer.temp_fp = NULL;
            
            if(active_transfer.received) free(active_transfer.received);
            memset(&active_transfer, 0, sizeof(active_transfer));
            
            printf("You> ");
            fflush(stdout);
        }
        return;
    }
}
/* Update on_chat_message to handle file transfers */
void on_chat_message(struct PRTP_packet* msg) {
    // Check if it's a file transfer message
    if(msg->data.chat.message_text) {
        if(strncmp(msg->data.chat.message_text, "FILE_START:", 11) == 0 ||
           strncmp(msg->data.chat.message_text, "FILE_CHUNK:", 11) == 0) {
            on_file_transfer_message(msg);
            return;
        }
    }
    
    // Regular chat message
    time_t timestamp = msg->data.chat.timestamp;
    struct tm* timeinfo = localtime(&timestamp);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%H:%M:%S", timeinfo);
    printf("\n[%s] %s: %s\n", time_str, 
           msg->data.chat.from_client_id, 
           msg->data.chat.message_text);
    printf("You> ");
    fflush(stdout);
}

/* Update command handler */
int process_combined_input(int sd, char* input) {
    char command[20] = {0};
    char target[32] = {0};
    char filename[500] = {0};
    char message[500] = {0};

    if(input[0] == '/') {
        if(sscanf(input, "/%s", command) < 1) return 0;
        
        if(strcmp(command, "quit") == 0) return -1;
        else if(strcmp(command, "users") == 0) request_user_list(sd);
        else if(strcmp(command, "msg") == 0) {
            if(sscanf(input, "/msg %s %[^\n]", target, message) < 2) 
                printf("Usage: /msg <user> <text>\n");
            else send_chat_message(sd, target, message);
        }
        else if(strcmp(command, "sendvideo") == 0 || strcmp(command, "sendfile") == 0) {
            if(sscanf(input + strlen(command) + 2, "%s %[^\n]", target, filename) < 2) {
                printf("Usage: /sendfile <user> <filepath>\n");
                printf("Example: /sendfile user_12345 Team_Abyss_1.mp4\n");
            } else {
                send_video_chunked(sd, target, filename);
            }
        }
        else if(strcmp(command, "broadcast") == 0) {
            if(sscanf(input, "/broadcast %[^\n]", message) < 1) 
                printf("Usage: /broadcast <text>\n");
            else send_chat_message(sd, NULL, message);
        }
        else printf("Unknown command. Try /users, /msg, /sendfile, /broadcast, or /quit\n");
    } else {
        send_chat_message(sd, NULL, input);
    }
    return 0;
}




















void main_loop(int sd) {
    struct pollfd fds[2];
    int timeout_msecs = KEEP_ALIVE_INTERVAL;
    struct timeval tv1, tv2, tvelapsed;
    struct transport aux_tr;
    char input_buffer[INPUTSIZE];
    aux_tr.sd = sd;

    snprintf(my_client_id, sizeof(my_client_id), "user_%d", getpid());
    printf("Chat/Sensor Client Started. ID: %s\n", my_client_id);
    
    send_client_registration(sd, my_client_id);
    usleep(100000); // 100ms delay
    
    printf("You> ");
    fflush(stdout);

    fds[0].fd = sd; fds[0].events = POLLIN;
    fds[1].fd = STDIN_FILENO; fds[1].events = POLLIN;

    for(;;) {
        gettimeofday(&tv1, 0);
        int ret = poll(fds, 2, timeout_msecs);
        gettimeofday(&tv2, 0);

        if(ret > 0) {
            if(fds[1].revents & POLLIN) {
                if(fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
                    input_buffer[strcspn(input_buffer, "\n")] = 0;
                    if(strlen(input_buffer) > 0) {
                        if(process_combined_input(sd, input_buffer) == -1) break;
                    }
                    printf("You> ");
                    fflush(stdout);
                }
            }
            if(fds[0].revents & POLLIN) {
                if(!timeval_subtract(&tvelapsed, &tv2, &tv1)) {
                    timeout_msecs -= (int)(tvelapsed.tv_sec*1000 + tvelapsed.tv_usec/1000);
                    if(timeout_msecs < 0) timeout_msecs = 0;
                }
                struct PRTP_packet* msg = NULL;
                size_t len = transport_receive(&aux_tr, &msg, NULL, NULL);
                if(len <= 0) break;
                if(msg != NULL) {
                    if(msg->type == UPDATE) on_update_msg(sd, msg);
                    else if(msg->type == CHAT_MESSAGE) on_chat_message(msg);
                    else if(msg->type == CHAT_ROOM_JOIN) {
                        printf("\n[SYSTEM] %s joined the chat\n", msg->data.chat.from_client_id);
                        printf("You> ");
                        fflush(stdout);
                    }
                    else if(msg->type == CHAT_USER_LIST) on_user_list(msg);
                    else if(msg->type == SUBSCRIBE_ACK) on_subscription_ack_sids(msg->data.blob);
                    
                    free_iotmsg(msg);
                }
            }
        }
        if(ret == 0 || timeout_msecs <= 0) {
            send_keep_alive(sd);
            timeout_msecs = KEEP_ALIVE_INTERVAL;
        }
    } 
}

void signal_handler(int signum) {
    exit(EXIT_SUCCESS);
}

int send_client_registration(int sd, const char* client_id) {
    struct PRTP_packet* msg = create_iotmsg(CHAT_ROOM_JOIN);
    msg->reliable = true;
    msg->data.chat.from_client_id = strdup(client_id);
    msg->data.chat.to_client_id = NULL;
    msg->data.chat.message_text = strdup("");
    msg->data.chat.timestamp = (uint32_t)time(NULL);
    size_t len = send_iotmsg(sd, msg);
    free_iotmsg(msg);
    return (int)len;
}

int main(int argc, char *argv[]) {
    char server_host[HOSTNAME_SIZE] = "localhost";
    char log_dir[HOSTNAME_SIZE] = "./client_sensor_log";
    int server_port = 5001;
    int c;
    struct PRTP_packet* submsg = NULL;

    signal(SIGINT, signal_handler);

    while ((c = getopt(argc, argv, "l:s:p:r:aAN")) != -1) {
        switch(c) {
            case 'l':
                if (strlen(optarg) < HOSTNAME_SIZE) {
                    strcpy(log_dir, optarg);
                } else {
                    fprintf(stderr, "Error: log directory name too long\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 's': 
                strncpy(server_host, optarg, HOSTNAME_SIZE-1); 
                break;
            case 'p': 
                server_port = atoi(optarg); 
                break;
            case 'r':
                if(!submsg) submsg = create_iotmsg(SUBSCRIBE);
                iotmsg_add_sid(submsg, optarg);
                iotmsg_set_sid_reliable(submsg, true);
                break;
            case 'N': 
                active_flow_enabled = true; 
                break;
            case 'h':
            default:
                fprintf(stderr, "Usage: %s [-l log_dir] [-s server_ip] [-p port] [-r sensor_id] [-N]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    l = init_logger(stdout, stderr, stderr, "Client");
    init_bson_parser();
    init_client_module();
    init_transport();
    init_sensor_logger(log_dir);

    int sd = init_socket(server_host, server_port, false, NULL);
    if(sd == -1) {
        fprintf(stderr, "Failed to connect to server\n");
        exit(EXIT_FAILURE);
    }

    if(submsg) {
        send_iotmsg(sd, submsg);
        free_iotmsg(submsg);
    }

    main_loop(sd);
    return EXIT_SUCCESS;
}