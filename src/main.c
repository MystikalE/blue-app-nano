/*******************************************************************************
*   RaiBlock Wallet for Ledger Nano S & Blue
*   (c) 2018 Mart Roosmaa
*   (c) 2016 Ledger
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

#include "string.h"

#include "os.h"
#include "os_io_seproxyhal.h"
#include "cx.h"

#include "rai_internal.h"
#include "rai_bagl.h"

#ifdef HAVE_U2F

#include "u2f_service.h"
#include "u2f_transport.h"

volatile uint8_t u2fMessageBuffer[U2F_MAX_MESSAGE_SIZE];

#endif // HAVE_U2F

extern void USB_power_U2F(bool enabled, bool fido);

uint8_t G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

ux_state_t ux;

#ifdef HAVE_U2F
volatile u2f_service_t u2fService;

void u2f_proxy_response(u2f_service_t *service, uint16_t tx) {
    os_memset(service->messageBuffer, 0, 5);
    os_memmove(service->messageBuffer + 5, G_io_apdu_buffer, tx);
    service->messageBuffer[tx + 5] = 0x90;
    service->messageBuffer[tx + 6] = 0x00;
    u2f_send_fragmented_response(service, U2F_CMD_MSG, service->messageBuffer,
                                 tx + 7, true);
}

#endif // HAVE_U2F

// override point, but nothing more to do
void io_seproxyhal_display(const bagl_element_t *element) {
    io_seproxyhal_display_default((bagl_element_t *)element);
}

uint16_t io_exchange_al(uint8_t channel, uint16_t tx_len) {
    switch (channel & ~(IO_FLAGS)) {
    case CHANNEL_KEYBOARD:
        break;

    // multiplexed io exchange over a SPI channel and TLV encapsulated protocol
    case CHANNEL_SPI:
        if (tx_len) {
            io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);

            if (channel & IO_RESET_AFTER_REPLIED) {
                reset();
            }
            return 0; // nothing received from the master so far (it's a tx
                      // transaction)
        } else {
            return io_seproxyhal_spi_recv(G_io_apdu_buffer,
                                          sizeof(G_io_apdu_buffer), 0);
        }

    default:
        THROW(INVALID_PARAMETER);
    }
    return 0;
}

uint8_t io_event(uint8_t channel) {
    // nothing done with the event, throw an error on the transport layer if
    // needed

    // can't have more than one tag in the reply, not supported yet.
    switch (G_io_seproxyhal_spi_buffer[0]) {
    case SEPROXYHAL_TAG_FINGER_EVENT:
        UX_FINGER_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT:
        UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_STATUS_EVENT:
        if (G_io_apdu_media == IO_APDU_MEDIA_USB_HID &&
            !(U4BE(G_io_seproxyhal_spi_buffer, 3) &
              SEPROXYHAL_TAG_STATUS_EVENT_FLAG_USB_POWERED)) {
            THROW(EXCEPTION_IO_RESET);
        }
    // no break is intentional
    default:
        UX_DEFAULT_EVENT();
        break;

    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        UX_DISPLAYED_EVENT({});
        break;

    case SEPROXYHAL_TAG_TICKER_EVENT:
        UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {
            ui_ticker_event(UX_ALLOWED);
        });
        break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

void app_exit(void) {
    BEGIN_TRY_L(exit) {
        TRY_L(exit) {
            os_sched_exit(-1);
        }
        FINALLY_L(exit) {
        }
    }
    END_TRY_L(exit);
}

__attribute__((section(".boot"))) int main(void) {
    // exit critical section
    __asm volatile("cpsie i");

    // ensure exception will work as planned
    os_boot();

    for (;;) {
        UX_INIT();
        BEGIN_TRY {
            TRY {
                io_seproxyhal_init();

                rai_context_init();

                // deactivate usb before activating
                USB_power_U2F(false, false);

#ifdef HAVE_U2F
                os_memset((uint8_t *)&u2fService, 0, sizeof(u2fService));
                u2fService.inputBuffer = G_io_apdu_buffer;
                u2fService.outputBuffer = G_io_apdu_buffer;
                u2fService.messageBuffer = (uint8_t *)u2fMessageBuffer;
                u2fService.messageBufferSize = U2F_MAX_MESSAGE_SIZE;
                u2f_initialize_service((u2f_service_t *)&u2fService);

                USB_power_U2F(true, N_rai.fidoTransport);
#else
                USB_power_U2F(true, false);
#endif

#ifdef HAVE_BLE
                BLE_power(false, NULL);
                BLE_power(true, "Ledger Wallet");
#endif // HAVE_BLE

#if defined(TARGET_BLUE)
                // setup the status bar colors (remembered after wards, even
                // more if another app does not resetup after app switch)
                UX_SET_STATUS_BAR_COLOR(0xFFFFFF, RAI_BAGL_COLOR_APP);
#endif // TARGET_BLUE

                ui_idle();

                app_main();
            }
            CATCH(EXCEPTION_IO_RESET) {
                // reset IO and UX
                continue;
            }
            CATCH_ALL {
                break;
            }
            FINALLY {
            }
        }
        END_TRY;
    }
    app_exit();

    return 0;
}
