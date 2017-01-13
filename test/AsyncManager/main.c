/*-
 * Copyright 2017 TENMYO Masakazu. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <uv.h>

#include <stdio.h>

#include "core/asyncmanager.h"

static const char msg[] = "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\n\r\nHOGEHOGE\r\nHOGEHOGEs\r\n";

void close_cb(Handle_t *handle, void *clientdata) {
  fprintf(stderr, "%s[%d] %s\n", __FILE__, __LINE__, __func__);
}

void write_cb(int status, StreamHandle_t *handle, void *clientdata) {
  fprintf(stderr, "%s[%d] %s %d\n", __FILE__, __LINE__, __func__, status);
  AsyncServer_Close((Handle_t *)handle, &close_cb, NULL);
}

void read_cb(StreamHandle_t *handle, const void *buf, signed long len, void *clientdata) {
  if (len > 0) {
    fprintf(stderr, "%s[%d] %s\n%.*s\n", __FILE__, __LINE__, __func__, len, buf);
  } else {
    fprintf(stderr, "%s[%d] %s %d\n", __FILE__, __LINE__, __func__, len);
    AsyncServer_ReadStop(handle);
    AsyncServer_Close((Handle_t *)handle, &close_cb, NULL);
  }
  AsyncServer_Write(handle, msg, sizeof(msg) - 1, &write_cb, NULL);
}
void accept_cb(int status, StreamHandle_t *handle, const char *host, int port, void *arg) {
  fprintf(stderr, "%s[%d] %s %d\n", __FILE__, __LINE__, __func__, status);
  fprintf(stderr, "connected from %s:%d\n", host, port);
  AsyncServer_ReadStart(handle, &read_cb, NULL);
}

int main(int argc, const char *argv[]) {
  printf("%s\n", uv_version_string());
  AsyncServer_InitTcpServer("127.0.0.1", 8080, 5, &accept_cb, NULL);
  Sleep(60 * 1000);
  return 0;
}
