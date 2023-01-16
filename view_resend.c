/* Copyright (C) 2022-2023 Salvatore Sanfilippo -- All Rights Reserved
 * See the LICENSE file for information about the license. */

#include "app.h"
#include <string.h>

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

    if (input.type == InputTypeShort && input.key == InputKeyBack) {
        app->current_view = ViewInfo;
    }
}
