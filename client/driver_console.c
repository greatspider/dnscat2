#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "log.h"
#include "memory.h"
#include "message.h"
#include "select_group.h"
#include "session.h"
#include "types.h"

#include "driver_console.h"

/* There can only be one driver_console, so store these as global variables. */
static SELECT_RESPONSE_t console_stdin_recv(void *group, int socket, uint8_t *data, size_t length, char *addr, uint16_t port, void *d)
{
  driver_console_t *driver_console = (driver_console_t*) d;

  message_t *message = message_data_out_create(driver_console->session_id, data, length);
  message_post(message);
  message_destroy(message);

  return SELECT_OK;
}

static SELECT_RESPONSE_t console_stdin_closed(void *group, int socket, void *d)
{
  driver_console_t *driver_console = (driver_console_t*) d;
  message_t *message = message_destroy_session_create(driver_console->session_id);
  message_post(message);
  message_destroy(message);

  return SELECT_OK;
}

/* This is called after the drivers are created, to kick things off. */
static void handle_start(driver_console_t *driver)
{
  message_t *message = message_create_session_create();
  message_post(message);
  driver->session_id = message->message.create.out.session_id;
  message_destroy(message);
}

static void handle_data_in(driver_console_t *driver, uint8_t *data, size_t length)
{
  size_t i;

  for(i = 0; i < length; i++)
    fputc(data[i], stdout);
}

static void handle_destroy(driver_console_t *driver)
{
  /* TODO: This can probably be handled better... */
  printf("Pipe broken [stdin]...\n");
  exit(1);
}

static void handle_message(message_t *message, void *d)
{
  driver_console_t *driver = (driver_console_t*) d;

  switch(message->type)
  {
    case MESSAGE_START:
      handle_start(driver);
      break;

    case MESSAGE_DATA_IN:
      handle_data_in(driver, message->message.data.data, message->message.data.length);
      break;

    case MESSAGE_DESTROY_SESSION:
      handle_destroy(driver);
      break;

    default:
      LOG_FATAL("driver_console received an invalid message!");
      abort();
      exit(1);
  }
}

message_handler_t *driver_console_get_message_handler(driver_console_t *driver)
{
  return message_handler_create(handle_message, driver);
}

void driver_console_close_message_handler(message_handler_t *handler)
{
  message_handler_destroy(handler);
}

driver_console_t *driver_console_create(select_group_t *group)
{
  driver_console_t *driver_console = (driver_console_t*) safe_malloc(sizeof(driver_console_t));

#ifdef WIN32
  /* On Windows, the stdin_handle is quite complicated, and involves a sub-thread. */
  HANDLE stdin_handle = get_stdin_handle();
  select_group_add_pipe(ui_stdin->group, -1, stdin_handle, driver_console);
  select_set_recv(ui_stdin->group, -1, console_stdin_recv);
  select_set_closed(ui_stdin->group, -1, console_closed);
#else
  /* On Linux, the stdin_handle is easy. */
  int stdin_handle = STDIN_FILENO;
  select_group_add_socket(group, stdin_handle, SOCKET_TYPE_STREAM, driver_console);
  select_set_recv(group, stdin_handle, console_stdin_recv);
  select_set_closed(group, stdin_handle, console_stdin_closed);
#endif

  /* Save the socket group in case we need it later. */
  driver_console->group              = group;

  /* Create and save a message handler. */
  driver_console->my_message_handler = driver_console_get_message_handler(driver_console);

  /* Subscribe to the messages we care about. */
  message_subscribe(MESSAGE_START,           driver_console->my_message_handler);
  message_subscribe(MESSAGE_DATA_IN,         driver_console->my_message_handler);
  message_subscribe(MESSAGE_DESTROY_SESSION, driver_console->my_message_handler);

  return driver_console;
}
