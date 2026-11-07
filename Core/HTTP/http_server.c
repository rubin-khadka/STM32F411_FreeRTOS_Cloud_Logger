/*
 * http_server.c
 *
 *  Created on: Nov 7, 2026
 *      Author: Rubin Khadka
 */

#include "http_server.h"
#include "usart2.h"
#include "socket.h"
#include <string.h>
#include <stdio.h>

#define HTTP_SOCKET     0U
#define HTTP_PORT       80U
#define HTTP_RX_SIZE    512U
#define HTTP_TX_SIZE    512U

static uint8_t s_rx[HTTP_RX_SIZE];
static uint8_t s_tx[HTTP_TX_SIZE];

/* ---------- Response builders ---------- */

static void send_html(uint8_t sock, const char *html)
{
  int n = snprintf((char*) s_tx, HTTP_TX_SIZE, "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html\r\n"
      "Content-Length: %d\r\n"
      "Connection: close\r\n"
      "\r\n"
      "%s", (int) strlen(html), html);
  if((n > 0) && (n < (int) HTTP_TX_SIZE))
  {
    (void) send(sock, s_tx, (uint16_t) n);
  }
}

static void send_not_found(uint8_t sock)
{
  const char *body = "404 Not Found";
  int n = snprintf((char*) s_tx, HTTP_TX_SIZE, "HTTP/1.1 404 Not Found\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: %d\r\n"
      "Connection: close\r\n"
      "\r\n"
      "%s", (int) strlen(body), body);
  if((n > 0) && (n < (int) HTTP_TX_SIZE))
  {
    (void) send(sock, s_tx, (uint16_t) n);
  }
}

/* ---------- Handlers ---------- */

static void handle_index(uint8_t sock)
{
  const char *page = "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
      "<title>STM32</title><style>"
      "body{background:#1a1a2e;color:#fff;text-align:center;"
      "font-family:Arial;padding:40px}"
      ".c{background:#16213e;padding:40px;border-radius:15px;"
      "max-width:400px;margin:40px auto}"
      "h1{color:#4ecdc4}</style></head><body>"
      "<div class='c'><h1>Hello from STM32</h1>"
      "<p>HTTP server working.</p></div>"
      "</body></html>";
  send_html(sock, page);
}

/* ---------- Init / Poll ---------- */

void HTTP_Server_Init(void)
{
  if(socket(HTTP_SOCKET, Sn_MR_TCP, HTTP_PORT, 0U) != HTTP_SOCKET)
  {
    (void) USART2_Printf("HTTP: socket() FAILED\r\n");
    return;
  }
  if(listen(HTTP_SOCKET) != SOCK_OK)
  {
    (void) USART2_Printf("HTTP: listen() FAILED\r\n");
    (void) close(HTTP_SOCKET);
    return;
  }
}

void HTTP_Server_Poll(void)
{
  uint8_t sr = getSn_SR(HTTP_SOCKET);

  switch(sr)
  {
    case SOCK_ESTABLISHED:
    case SOCK_CLOSE_WAIT:
    {
      uint16_t avail = getSn_RX_RSR(HTTP_SOCKET);
      if(avail > 0U)
      {
        uint16_t want = (avail < HTTP_RX_SIZE) ? avail : (HTTP_RX_SIZE - 1U);
        int32_t got = recv(HTTP_SOCKET, s_rx, want);
        if(got > 0)
        {
          s_rx[got] = '\0';

          /* Parse "METHOD PATH VERSION" */
          char *method = (char*) s_rx;
          char *path = NULL;
          int i = 0;
          while((i < got) && (s_rx[i] != ' ') && (s_rx[i] != '\0'))
            i++;
          if(s_rx[i] == ' ')
          {
            s_rx[i] = '\0';
            path = (char*) &s_rx[i + 1];
            int j = i + 1;
            while((j < got) && (s_rx[j] != ' ') && (s_rx[j] != '\0'))
              j++;
            if(s_rx[j] == ' ')
            {
              s_rx[j] = '\0';
            }
          }

          if((method != NULL) && (path != NULL) && (strcmp(method, "GET") == 0))
          {
            /* Extend this chain for new routes */
            if((strcmp(path, "/") == 0) || (strcmp(path, "/index.html") == 0))
            {
              handle_index(HTTP_SOCKET);
            }
            else
            {
              send_not_found(HTTP_SOCKET);
            }
          }
        }
      }
      (void) disconnect(HTTP_SOCKET);
      break;
    }

    case SOCK_CLOSED:
      if(socket(HTTP_SOCKET, Sn_MR_TCP, HTTP_PORT, 0U) == HTTP_SOCKET)
      {
        (void) listen(HTTP_SOCKET);
      }
      break;

    case SOCK_INIT:
      (void) listen(HTTP_SOCKET);
      break;

    default:
      break;
  }
}

