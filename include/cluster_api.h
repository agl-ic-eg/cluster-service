/*
 * Copyright (c) 2021, Nippon Seiki Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CLUSTER_API_H
#define CLUSTER_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	IC_HMI_OFF = 0,
	IC_HMI_ON,
} IC_HMI_ON_OFF;

#include <cluster_api_registernotify.h>
#include <cluster_api_shiftposition.h>
#include <cluster_api_speed.h>
#include <cluster_api_tacho.h>
#include <cluster_api_telltale.h>
#include <cluster_api_tripcomputer.h>

bool clusterInit(void);
bool clusterTerm(void);
#ifdef __cplusplus
}
#endif

#endif // CLUSTER_API_H
