/* Copyright (C) 2022-2023 Salvatore Sanfilippo -- All Rights Reserved
 * See the LICENSE file for information about the license. */

#include "app.h"
#include <string.h>
#include <flipper_format.h>
#include <flipper_format_i.h>
#include <applications/main/subghz/subghz_i.h>
#include <lib/subghz/protocols/protocol_items.h>

#define PROTOVIEW_TMP_SUB_FILE_PATH "subghz/protoview.sub"
#define PROTOVIEW_SEND_WAIT_MS 10

void process_send(ProtoViewApp* app);
bool create_tmp_file(ProtoViewApp* app, FuriString* raw_data);

/* Renders the view with the detected message information. */
void render_view_resend(Canvas* const canvas, ProtoViewApp* app) {
    ProtoViewDecoder* decoder = app->signal_info.decoder;

    const char* current_name = decoder->dynamic_values_names[app->current_value_index];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 5, AlignCenter, AlignTop, current_name);

    char* current_value = decoder->get_value_for(app->current_value_index, &app->signal_info);

    bool is_digits_only = strspn(current_value, "0123456789") == strlen(current_value);

    Font font = is_digits_only ? FontBigNumbers : FontPrimary;
    canvas_set_font(canvas, font);

    CanvasFontParameters* params = canvas_get_font_params(canvas, font);

    canvas_draw_str_aligned(canvas, 64, 32 - params->height / 2, AlignCenter, AlignTop, current_value);

    free(current_value);
    free(params);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 53, AlignCenter, AlignBottom, "Click OK to send");
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "Use arrows to modify values");
}

/* Handle input for the settings view. */
void process_input_resend(ProtoViewApp* app, InputEvent input) {
    ProtoViewDecoder* decoder = app->signal_info.decoder;

    if (input.type == InputTypePress && input.key == InputKeyRight) {
        app->current_value_index = (app->current_value_index + 1 >= decoder->dynamic_values_len) ? 0 : app->current_value_index + 1;
    }

    if (input.type == InputTypePress && input.key == InputKeyLeft) {
        app->current_value_index = (app->current_value_index - 1 < 0) ? decoder->dynamic_values_len - 1 : app->current_value_index - 1;
    }

    if (input.type == InputTypePress && (input.key == InputKeyUp || input.key == InputKeyDown)) {
        decoder->update_value_for(app->current_value_index, &app->signal_info, input.key == InputKeyUp);
    }

    if (input.type == InputTypePress && input.key == InputKeyBack) {
        radio_rx(app);
        app->current_view = ViewInfo;
    }

    if (input.type == InputTypePress && input.key == InputKeyOk) {
        process_send(app);
    }
}

void process_send(ProtoViewApp* app) {
    FuriString* raw_data = furi_string_alloc();
    app->signal_info.decoder->get_raw_data_payload(&app->signal_info, raw_data);

    if (!create_tmp_file(app, raw_data)) {
        FURI_LOG_W(TAG, "Unable to create temp file");
        furi_string_free(raw_data);
        return;
    }

    SubGhzEnvironment* environment = subghz_environment_alloc();
    subghz_environment_set_protocol_registry(environment, (void*)&subghz_protocol_registry);

    SubGhzTransmitter* transmitter = subghz_transmitter_alloc_init(environment, "RAW");

    if (transmitter) {
        FlipperFormat* fff_data = flipper_format_string_alloc();
        subghz_protocol_raw_gen_fff_data(fff_data, ANY_PATH(PROTOVIEW_TMP_SUB_FILE_PATH));
        subghz_transmitter_deserialize(transmitter, fff_data);

        if (furi_hal_subghz_start_async_tx(subghz_transmitter_yield, transmitter)) {
            while (!furi_hal_subghz_is_async_tx_complete()) {
                furi_delay_ms(PROTOVIEW_SEND_WAIT_MS);
            }

            furi_hal_subghz_stop_async_tx();
        }

        flipper_format_free(fff_data);
    }

    subghz_transmitter_free(transmitter);
    subghz_environment_free(environment);
    furi_string_free(raw_data);
}

/* I dont like that we have to create file and use it to send data, but belive that antirez will come up with better solution! */
bool create_tmp_file(ProtoViewApp* app, FuriString* raw_data) {
    FuriString* file_content;
    file_content = furi_string_alloc();
    furi_string_printf(file_content, "Flipper SubGhz RAW File\n"
                                     "Version: 1\n"
                                     "Frequency: %ld\n"
                                     "Preset: FuriHalSubGhzPresetOok650Async\n"
                                     "Protocol: RAW\n"
                                     "RAW_Data: %s", app->frequency, furi_string_get_cstr(raw_data));

    FuriString* file_name;
    file_name = furi_string_alloc();
    furi_string_set(file_name, ANY_PATH(PROTOVIEW_TMP_SUB_FILE_PATH));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    bool success = false;
    if (flipper_format_file_open_always(file, furi_string_get_cstr(file_name))) {
        if (flipper_format_write_string(file, "Filetype", file_content)) {
            success = true;
        }
        else {
            FURI_LOG_W(TAG, "Unable to flipper_format_write_string");
        }
    }
    else {
        FURI_LOG_W(TAG, "Unable to flipper_format_file_open_always");
    }

    furi_record_close(RECORD_STORAGE);
    flipper_format_free(file);
    furi_string_free(file_name);
    furi_string_free(file_content);

    return success;
}
