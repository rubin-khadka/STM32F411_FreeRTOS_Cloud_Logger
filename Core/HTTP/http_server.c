/*
 * http_server.c
 *
 *  Created on: Nov 7, 2026
 *      Author: Rubin Khadka
 */

#include "http_server.h"
#include "sensor_data.h"
#include "usart2.h"
#include "socket.h"
#include <string.h>

#define HTTP_SOCKET     0U
#define HTTP_PORT       80U
#define HTTP_RX_SIZE    512U
#define HTTP_TX_SIZE    4096U

static uint8_t s_rx[HTTP_RX_SIZE];
static uint8_t s_tx[HTTP_TX_SIZE];

static char* put_str(char *p, const char *s)
{
  while(*s != '\0')
  {
    *p++ = *s++;
  }
  return p;
}

static char* put_uint(char *p, uint32_t v)
{
  char tmp[10];
  uint8_t n = 0U;
  if(v == 0U)
  {
    *p++ = '0';
    return p;
  }
  while(v > 0U)
  {
    tmp[n++] = (char) ('0' + (v % 10U));
    v /= 10U;
  }
  while(n > 0U)
  {
    *p++ = tmp[--n];
  }
  return p;
}

static char* put_2digit(char *p, uint8_t v)
{
  *p++ = (char) ('0' + (v / 10U));
  *p++ = (char) ('0' + (v % 10U));
  return p;
}

static char* put_scaled(char *p, int32_t scaled, uint8_t decimals)
{
  if(scaled < 0)
  {
    *p++ = '-';
    scaled = -scaled;
  }

  uint32_t div = 1U;
  for(uint8_t i = 0U; i < decimals; i++)
  {
    div *= 10U;
  }

  uint32_t ip = (uint32_t) scaled / div;
  uint32_t fp = (uint32_t) scaled % div;

  p = put_uint(p, ip);

  if(decimals > 0U)
  {
    *p++ = '.';
    div /= 10U;
    while(div > 0U)
    {
      *p++ = (char) ('0' + (fp / div));
      fp %= div;
      div /= 10U;
    }
  }
  return p;
}

static int32_t round_scaled(float v, float scale)
{
  float s = v * scale;
  s += (s >= 0.0f) ? 0.5f : -0.5f;
  return (int32_t) s;
}

/* ============================================================ */
/* HTTP response writers                                       */
/* ============================================================ */

static void send_response(uint8_t sock, const char *ctype, const char *body, uint16_t body_len)
{
  char *p = (char*) s_tx;

  p = put_str(p, "HTTP/1.1 200 OK\r\nContent-Type: ");
  p = put_str(p, ctype);
  p = put_str(p, "\r\nContent-Length: ");
  p = put_uint(p, body_len);
  p = put_str(p, "\r\nConnection: close\r\n\r\n");

  uint16_t hdr = (uint16_t) (p - (char*) s_tx);
  if((uint16_t) (hdr + body_len) > HTTP_TX_SIZE)
  {
    return;
  }

  (void) memcpy(p, body, body_len);
  p += body_len;

  (void) send(sock, s_tx, (uint16_t) (p - (char*) s_tx));
}

/* ============================================================ */
/* HTML dashboard                                              */
/* ============================================================ */

static const char s_html_page[] = "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
    "<title>STM32 Sensor Monitor</title><style>"
    "body{background:#1a1a2e;color:#fff;text-align:center;"
    "font-family:Arial,sans-serif;padding:20px;margin:0}"
    ".c{background:#16213e;padding:20px;border-radius:15px;"
    "max-width:500px;margin:20px auto;box-shadow:0 4px 12px rgba(0,0,0,0.3)}"
    "h1{color:#4ecdc4;margin:0 0 20px 0}"
    "h3{margin:0 0 12px 0;color:#4ecdc4;font-size:15px;"
    "font-weight:normal;letter-spacing:1px}"
    ".tm{font-size:32px;font-weight:bold;color:#4ecdc4}"
    ".dt{color:#888;font-size:14px;margin-top:4px}"
    ".row{display:flex;justify-content:space-between;"
    "padding:6px 0;border-bottom:1px solid #0f3460}"
    ".row:last-child{border-bottom:none}"
    ".label{color:#888;text-align:left}"
    ".num{font-weight:bold}"
    ".temp{color:#e94560}.hum{color:#4CAF50}"
    ".accel{color:#4ecdc4}.gyro{color:#ffa500}"
    ".err{color:#666;font-style:italic}"
    "</style></head><body>"
    "<h1>STM32 Sensor Monitor</h1>"

    "<div class='c'><div class='tm' id='tm'>--:--:--</div>"
    "<div class='dt' id='dt'>----/--/--</div></div>"

    "<div class='c'><h3>ENVIRONMENT</h3>"
    "<div class='row'><span class='label'>Temperature</span>"
    "<span class='num temp' id='tp'>--</span></div>"
    "<div class='row'><span class='label'>Humidity</span>"
    "<span class='num hum' id='hm'>--</span></div></div>"

    "<div class='c'><h3>ACCELEROMETER (g)</h3>"
    "<div class='row'><span class='label'>X</span>"
    "<span class='num accel' id='ax'>--</span></div>"
    "<div class='row'><span class='label'>Y</span>"
    "<span class='num accel' id='ay'>--</span></div>"
    "<div class='row'><span class='label'>Z</span>"
    "<span class='num accel' id='az'>--</span></div></div>"

    "<div class='c'><h3>GYROSCOPE (dps)</h3>"
    "<div class='row'><span class='label'>X</span>"
    "<span class='num gyro' id='gx'>--</span></div>"
    "<div class='row'><span class='label'>Y</span>"
    "<span class='num gyro' id='gy'>--</span></div>"
    "<div class='row'><span class='label'>Z</span>"
    "<span class='num gyro' id='gz'>--</span></div></div>"

    "<script>"
    "function u(){"
    "fetch('/api/data').then(r=>r.json()).then(d=>{"
    "document.getElementById('tm').innerHTML=d.time;"
    "document.getElementById('dt').innerHTML=d.date;"
    "document.getElementById('tp').innerHTML=d.dht_ok?(d.temp+'&#176;C'):'--';"
    "document.getElementById('hm').innerHTML=d.dht_ok?(d.hum+'%'):'--';"
    "document.getElementById('ax').innerHTML=d.imu_ok?d.ax.toFixed(3):'--';"
    "document.getElementById('ay').innerHTML=d.imu_ok?d.ay.toFixed(3):'--';"
    "document.getElementById('az').innerHTML=d.imu_ok?d.az.toFixed(3):'--';"
    "document.getElementById('gx').innerHTML=d.imu_ok?d.gx.toFixed(2):'--';"
    "document.getElementById('gy').innerHTML=d.imu_ok?d.gy.toFixed(2):'--';"
    "document.getElementById('gz').innerHTML=d.imu_ok?d.gz.toFixed(2):'--';"
    "}).catch(e=>console.log('fetch',e));"
    "}"
    "u();setInterval(u,1000);"
    "</script></body></html>";

/* ============================================================ */
/* JSON API                                                    */
/* ============================================================ */

static void handle_api_data(uint8_t sock)
{
  SensorData_t s;
  SensorData_GetSnapshot(&s);

  char json[400];
  char *p = json;

  /* Temperature / humidity */
  p = put_str(p, "{\"temp\":");
  p = put_scaled(p, round_scaled(s.temp_c, 10.0f), 1U);
  p = put_str(p, ",\"hum\":");
  p = put_scaled(p, round_scaled(s.hum_pct, 10.0f), 1U);

  /* Accelerometer */
  p = put_str(p, ",\"ax\":");
  p = put_scaled(p, round_scaled(s.ax_g, 1000.0f), 3U);
  p = put_str(p, ",\"ay\":");
  p = put_scaled(p, round_scaled(s.ay_g, 1000.0f), 3U);
  p = put_str(p, ",\"az\":");
  p = put_scaled(p, round_scaled(s.az_g, 1000.0f), 3U);

  /* Gyroscope */
  p = put_str(p, ",\"gx\":");
  p = put_scaled(p, round_scaled(s.gx_dps, 100.0f), 2U);
  p = put_str(p, ",\"gy\":");
  p = put_scaled(p, round_scaled(s.gy_dps, 100.0f), 2U);
  p = put_str(p, ",\"gz\":");
  p = put_scaled(p, round_scaled(s.gz_dps, 100.0f), 2U);

  /* Time / date from RTC */
  p = put_str(p, ",\"time\":\"");
  if(s.rtc_valid)
  {
    p = put_2digit(p, s.rtc.hour);
    *p++ = ':';
    p = put_2digit(p, s.rtc.min);
    *p++ = ':';
    p = put_2digit(p, s.rtc.sec);
  }
  else
  {
    p = put_str(p, "--:--:--");
  }
  p = put_str(p, "\",\"date\":\"");
  if(s.rtc_valid)
  {
    p = put_str(p, "20");
    p = put_2digit(p, s.rtc.year);
    *p++ = '/';
    p = put_2digit(p, s.rtc.month);
    *p++ = '/';
    p = put_2digit(p, s.rtc.date);
  }
  else
  {
    p = put_str(p, "----/--/--");
  }
  p = put_str(p, "\"");

  /* Validity flags */
  p = put_str(p, ",\"dht_ok\":");
  p = put_uint(p, s.dht_valid);
  p = put_str(p, ",\"imu_ok\":");
  p = put_uint(p, s.imu_valid);
  p = put_str(p, ",\"rtc_ok\":");
  p = put_uint(p, s.rtc_valid);
  *p++ = '}';
  *p = '\0';

  send_response(sock, "application/json", json, (uint16_t) (p - json));
}

/* ============================================================ */
/* Init / Poll                                                 */
/* ============================================================ */

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
            if((strcmp(path, "/") == 0) || (strcmp(path, "/index.html") == 0))
            {
              send_response(HTTP_SOCKET, "text/html", s_html_page, (uint16_t) (sizeof(s_html_page) - 1U));
            }
            else if(strcmp(path, "/api/data") == 0)
            {
              handle_api_data(HTTP_SOCKET);
            }
            else
            {
              const char *body = "{\"error\":\"not found\"}";
              send_response(HTTP_SOCKET, "application/json", body, (uint16_t) strlen(body));
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
