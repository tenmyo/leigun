/*
  Copyright 2017 TENMYO Masakazu. All rights reserved.
 
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
 
      http://www.apache.org/licenses/LICENSE-2.0
 
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 */

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>

typedef struct TcpHandle_t TcpHandle_t;
typedef void (*AsyncManager_accept_cb)(int status, TcpHandle_t *handle, const char *host, int port, void *clientdata);
typedef void (*AsyncManager_close_cb)(TcpHandle_t *handle, void *clientdata);
typedef void (*AsyncManager_write_cb)(int status, TcpHandle_t *handle, void *clientdata);
typedef void (*AsyncManager_read_cb)(TcpHandle_t *handle, const void *buf, signed int len, void *clientdata);
int AsyncServer_InitTcpServer(const char *ip, int port, int backlog, AsyncManager_accept_cb accept_cb, void *clientdata);
int AsyncServer_Close(TcpHandle_t *handle, AsyncManager_close_cb close_cb, void *clientdata);
int AsyncServer_Write(TcpHandle_t *handle, const void *base, size_t len, AsyncManager_write_cb write_cb, void *clientdata);
int AsyncServer_ReadStart(TcpHandle_t *handle, AsyncManager_read_cb read_cb, void *clientdata);
int AsyncServer_ReadStop(TcpHandle_t *handle);

#ifdef __cplusplus
}
#endif
