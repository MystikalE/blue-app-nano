/*******************************************************************************
*   RaiBlock Wallet for Ledger Nano S & Blue
*   (c) 2018 Mart Roosmaa
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#ifndef __RAI_BAGL_H__

#define __RAI_BAGL_H__

#include "rai.h"

#define RAI_BAGL_COLOR_APP 0xFCB653

void rai_bagl_display_address(void);
void rai_bagl_display_address_callback(bool confirmed);

void rai_bagl_confirm_sign_block();
void rai_bagl_confirm_sign_block_callback(bool confirmed);

#endif