#include "app.h"

#include <flipper_format/flipper_format_i.h>

/* Called after the application initialization in order to setup the
 * subghz system and put it into idle state. If the user wants to start
 * receiving we will call radio_rx() to start a receiving worker and
 * associated thread. */
void radio_begin(ProtoViewApp* app) {
    furi_assert(app);
    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_preset(FuriHalSubGhzPresetOok650Async);
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);
    app->txrx->txrx_state = TxRxStateIDLE;
}

/* Setup subghz to start receiving using a background worker. */
uint32_t radio_rx(ProtoViewApp* app, uint32_t frequency) {
    furi_assert(app);
    if(!furi_hal_subghz_is_frequency_valid(frequency)) {
        furi_crash(TAG" Incorrect RX frequency.");
    }

    if (app->txrx->txrx_state == TxRxStateRx) return frequency;

    furi_hal_subghz_idle(); /* Put it into idle state in case it is sleeping. */
    uint32_t value = furi_hal_subghz_set_frequency_and_path(frequency);
    FURI_LOG_E(TAG, "Switched to frequency: %lu", value);
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);
    furi_hal_subghz_flush_rx();
    furi_hal_subghz_rx();

    furi_hal_subghz_start_async_rx(subghz_worker_rx_callback, app->txrx->worker);
    subghz_worker_start(app->txrx->worker);
    app->txrx->txrx_state = TxRxStateRx;
    return value;
}

/* Stop subghz worker (if active), put radio on idle state. */
void radio_rx_end(ProtoViewApp* app) {
    furi_assert(app);
    if (app->txrx->txrx_state == TxRxStateRx) {
        if(subghz_worker_is_running(app->txrx->worker)) {
            subghz_worker_stop(app->txrx->worker);
            furi_hal_subghz_stop_async_rx();
        }
    }
    furi_hal_subghz_idle();
    app->txrx->txrx_state = TxRxStateIDLE;
}

/* Put radio on sleep. */
void radio_sleep(ProtoViewApp* app) {
    furi_assert(app);
    if (app->txrx->txrx_state == TxRxStateRx) {
        /* We can't go from having an active RX worker to sleeping.
         * Stop the RX subsystems first. */
        radio_rx_end(app);
    }
    furi_hal_subghz_sleep();
    app->txrx->txrx_state = TxRxStateSleep;
}