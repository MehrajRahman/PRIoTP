/* chat_client.c - Add chat functionality to PRTP_client.c */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include "client_module.h"
#include "messages.h"



static char my_client_id[32] = {0};

/* Send a chat message */
int send_chat_message(int sd, const char* to_client_id, const char* message_text)
{
  struct PRTP_packet* msg = create_iotmsg(CHAT_MESSAGE);
  size_t len = 0;
  
  msg->reliable = true;  // Use reliable delivery for chat
  msg->data.chat.from_client_id = strdup(my_client_id);
  msg->data.chat.to_client_id = to_client_id ? strdup(to_client_id) : NULL;
  msg->data.chat.message_text = strdup(message_text);
  msg->data.chat.timestamp = (uint32_t)time(NULL);
  
  len = send_iotmsg(sd, msg);
  free_iotmsg(msg);
  
  return len;
}

/* Request list of online users */
int request_user_list(int sd)
{
  struct PRTP_packet* msg = create_iotmsg(CHAT_USER_LIST);
  size_t len = send_iotmsg(sd, msg);
  free_iotmsg(msg);
  return len;
}

/* Handle incoming chat message */
void on_chat_message(struct PRTP_packet* msg)
{
  time_t timestamp = msg->data.chat.timestamp;
  struct tm* timeinfo = localtime(&timestamp);
  char time_str[20];
  
  strftime(time_str, sizeof(time_str), "%H:%M:%S", timeinfo);
  
  printf("\n[%s] %s: %s\n", 
         time_str,
         msg->data.chat.from_client_id, 
         msg->data.chat.message_text);
  printf("You> ");  // Re-prompt
  fflush(stdout);
}

/* Handle user list response */
void on_user_list(struct PRTP_packet* msg)
{
  struct iotmsg_node* node;
  int count = 0;
  
  printf("\n=== Online Users ===\n");
  for(node = msg->data.blob; node != NULL; node = node->next) {
    if(strcmp(node->id, my_client_id) != 0) {
      printf("  - %s\n", node->id);
      count++;
    }
  }
  
  if(count == 0) {
    printf("  (No other users online)\n");
  }
  printf("====================\n");
  printf("You> ");
  fflush(stdout);
}

/* Print chat help */
void print_chat_help()
{
  printf("\n=== Chat Commands ===\n");
  printf("  /users          - List online users\n");
  printf("  /msg <user> <text> - Send private message\n");
  printf("  /broadcast <text>  - Send to all users\n");
  printf("  /quit           - Exit chat\n");
  printf("  /help           - Show this help\n");
  printf("====================\n");
}

/* Parse chat command from user input */
int process_chat_input(int sd, const char* input)
{
  char command[20] = {0};
  char target[32] = {0};
  char message[500] = {0};
  
  if(input[0] == '/') {
    // Command mode
    if(sscanf(input, "/%s", command) < 1) {
      return 0;
    }
    
    if(strcmp(command, "quit") == 0) {
      return -1;  // Signal to quit
    }
    else if(strcmp(command, "help") == 0) {
      print_chat_help();
    }
    else if(strcmp(command, "users") == 0) {
      request_user_list(sd);
    }
    else if(strcmp(command, "msg") == 0) {
      if(sscanf(input, "/msg %s %[^\n]", target, message) < 2) {
        printf("Usage: /msg <username> <message>\n");
      } else {
        send_chat_message(sd, target, message);
        printf("[Sent to %s]\n", target);
      }
    }
    else if(strcmp(command, "broadcast") == 0) {
      if(sscanf(input, "/broadcast %[^\n]", message) < 1) {
        printf("Usage: /broadcast <message>\n");
      } else {
        send_chat_message(sd, NULL, message);
        printf("[Broadcast sent]\n");
      }
    }
    else {
      printf("Unknown command: %s (type /help for commands)\n", command);
    }
  } else {
    // Normal message - broadcast by default
    send_chat_message(sd, NULL, input);
  }
  
  return 0;
}

/* Main chat loop - add this to PRTP_client.c */
void chat_loop(int sd)
{
  struct pollfd fds[2];
  int timeout_msecs = KEEP_ALIVE_INTERVAL;
  struct timeval tv1, tv2, tvelapsed;
  int ret = 0;
  size_t len = 0;
  struct PRTP_packet* msg = NULL;
  struct transport aux_tr;
  char input_buffer[512];
  
  aux_tr.sd = sd;
  
  // Generate client ID (you might want to make this more sophisticated)
  snprintf(my_client_id, sizeof(my_client_id), "user_%d", getpid());
  
  printf("\n=================================\n");
  printf("  PRTP Chat Client Started\n");
  printf("  Your ID: %s\n", my_client_id);
  printf("=================================\n");
  print_chat_help();
  printf("\nYou> ");
  fflush(stdout);

  fds[0].fd = sd;        // Socket
  fds[0].events = POLLIN;
  fds[1].fd = STDIN_FILENO;  // Standard input
  fds[1].events = POLLIN;

  for(;;) {
    gettimeofday(&tv1, 0);
    ret = poll(fds, 2, timeout_msecs);
    gettimeofday(&tv2, 0);
    
    if(ret > 0) {
      // Handle user input
      if(fds[1].revents & POLLIN) {
        if(fgets(input_buffer, sizeof(input_buffer), stdin) != NULL) {
          // Remove newline
          input_buffer[strcspn(input_buffer, "\n")] = 0;
          
          if(strlen(input_buffer) > 0) {
            if(process_chat_input(sd, input_buffer) == -1) {
              printf("Goodbye!\n");
              break;  // User wants to quit
            }
          }
          printf("You> ");
          fflush(stdout);
        }
      }
      
      // Handle incoming messages
      if(fds[0].revents & POLLIN) {
        if(!timeval_subtract(&tvelapsed, &tv2, &tv1)) {
          timeout_msecs -= tvelapsed.tv_sec*1000 + tvelapsed.tv_usec/1000;
          if(timeout_msecs < 0) timeout_msecs = 0;
        }
        
        msg = NULL;
        len = transport_receive(&aux_tr, &msg, NULL, NULL);
        
        if(len == -1 || len == 0) {
          printf("\nConnection lost\n");
          break;
        }
        
        if(msg != NULL) {
          if(msg->type == CHAT_MESSAGE) {
            on_chat_message(msg);
          }
          else if(msg->type == CHAT_USER_LIST) {
            on_user_list(msg);
          }
          
          free_iotmsg(msg);
        }
      }
    }
    
    // Send keep-alive
    if((ret == 0 || timeout_msecs == 0)) {
      send_keep_alive(sd);
      timeout_msecs = KEEP_ALIVE_INTERVAL;
    }
  }
}

/* Main function for chat mode */
int main(int argc, char *argv[])
{
  char server_host[255] = "localhost";
  int server_port = 5001;
  int sd;
  int c;
  
  // Parse command line arguments
  while((c = getopt(argc, argv, "hs:p:")) != -1) {
    switch(c) {
      case 's':
        strncpy(server_host, optarg, sizeof(server_host)-1);
        break;
      case 'p':
        server_port = atoi(optarg);
        break;
      case 'h':
      default:
        printf("Usage: %s [-s server_ip] [-p server_port]\n", argv[0]);
        exit(EXIT_SUCCESS);
    }
  }
  
  // Initialize subsystems
  init_logger(stdout, stderr, stderr, "ChatClient");
  init_bson_parser();
  init_client_module();
  init_transport();
  
  // Connect to server
  printf("Connecting to %s:%d...\n", server_host, server_port);
  sd = init_socket(server_host, server_port, false, NULL);
  
  if(sd == -1) {
    fprintf(stderr, "Failed to connect to server\n");
    exit(EXIT_FAILURE);
  }
  
  printf("Connected!\n");
  
  // Enter chat loop
  chat_loop(sd);
  
  // Cleanup
  close(sd);
  shutdown_client_module();
  shutdown_transport();
  shutdown_bson_parser();
  
  return EXIT_SUCCESS;
}