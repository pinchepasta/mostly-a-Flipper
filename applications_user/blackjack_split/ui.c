#include <stdio.h>
#include <notification/notification_messages.h>

#include "ui.h"

#define LINE_HEIGHT 16
#define ITEM_PADDING 4

const char MoneyMul[4] = {
    'K', 'B', 'T', 'S'
};

static void draw_tag(
    Canvas* canvas,
    int16_t x,
    int16_t y,
    const char* text) {

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, x, y, 34, 10, 2);

    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        x + 17,
        y + 5,
        AlignCenter,
        AlignCenter,
        text);

    canvas_set_color(canvas, ColorBlack);
}

static void draw_status(
    Canvas* canvas,
    const char* label,
    uint8_t amount) {

    char status_text[12];
    snprintf(status_text, sizeof(status_text), "%s:%u", label, amount);

    const int16_t box_x = 99;
    const int16_t box_y = 3;
    const int16_t box_w = 26;
    const int16_t box_h = 10;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, box_x, box_y, box_w, box_h, 2);

    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        box_x + box_w / 2,
        box_y + box_h / 2 + 1,
        AlignCenter,
        AlignCenter,
        status_text);

    canvas_set_color(canvas, ColorBlack);
}

static void draw_cards(
    Canvas* canvas,
    const Card* cards,
    uint8_t count,
    int16_t x,
    int16_t y,
    uint8_t limit) {

    uint8_t visible = count > limit ? limit : count;

    for(uint8_t i = 0; i < visible; i++) {
        draw_card_at(
            x + (int16_t)i * 13,
            y,
            cards[i].pip,
            cards[i].character,
            canvas);
    }

    if(count > visible) {
        char more[8];
        snprintf(more, sizeof(more), "+%u", count - visible);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(
            canvas,
            x + (int16_t)visible * 13,
            y + 12,
            more);
    }
}

void draw_player_scene(Canvas* const canvas, const GameState* game_state) {
    canvas_set_color(canvas, ColorBlack);

    draw_tag(canvas, 3, 2, "DEALER");

    if(game_state->dealer_card_count > 0) {
        draw_card_back_at(42, 2, canvas);
    }

    if(game_state->dealer_card_count > 1) {
        draw_cards(
            canvas,
            &game_state->dealer_cards[1],
            game_state->dealer_card_count - 1,
            61,
            2,
            2);
    }

    canvas_draw_line(canvas, 3, 27, 124, 27);

    if(game_state->split_active && game_state->split_card_count > 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 3, 36, "H1");

        draw_cards(
            canvas,
            game_state->player_cards,
            game_state->player_card_count,
            22,
            29,
            3);

        canvas_draw_str(canvas, 66, 36, "H2");

        draw_cards(
            canvas,
            game_state->split_cards,
            game_state->split_card_count,
            85,
            29,
            3);

        draw_status(
            canvas,
            game_state->playing_split_hand ? "H2" : "H1",
            game_state->playing_split_hand ?
                hand_count(
                    game_state->split_cards,
                    game_state->split_card_count) :
                hand_count(
                    game_state->player_cards,
                    game_state->player_card_count));
    } else {
        draw_tag(canvas, 3, 31, "YOU");

        if(game_state->player_card_count > 0) {
            draw_cards(
                canvas,
                game_state->player_cards,
                game_state->player_card_count,
                42,
                29,
                4);
        }

        draw_status(
            canvas,
            "P",
            hand_count(
                game_state->player_cards,
                game_state->player_card_count));
    }
}

void draw_dealer_scene(Canvas* const canvas, const GameState* game_state) {
    canvas_set_color(canvas, ColorBlack);

    draw_tag(canvas, 3, 2, "DEALER");

    if(game_state->dealer_card_count > 0) {
        draw_cards(
            canvas,
            game_state->dealer_cards,
            game_state->dealer_card_count,
            42,
            2,
            4);
    }

    draw_status(
        canvas,
        "D",
        hand_count(
            game_state->dealer_cards,
            game_state->dealer_card_count));

    canvas_draw_line(canvas, 3, 27, 124, 27);

    draw_tag(canvas, 3, 31, "YOU");

    if(game_state->player_card_count > 0) {
        draw_cards(
            canvas,
            game_state->player_cards,
            game_state->player_card_count,
            42,
            29,
            4);
    }
}

void popup_frame(Canvas* const canvas) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rbox(canvas, 33, 20, 62, 15, 3);

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 33, 20, 62, 15, 3);

    canvas_set_font(canvas, FontSecondary);
}

void draw_play_menu(Canvas* const canvas, const GameState* game_state) {
    UNUSED(canvas);
    UNUSED(game_state);
}

void draw_screen(Canvas* const canvas, const bool* points) {
    for(uint8_t x = 0; x < 128; x++) {
        for(uint8_t y = 0; y < 64; y++) {
            if(points[y * 128 + x]) {
                canvas_draw_dot(canvas, x, y);
            }
        }
    }
}

void draw_score(Canvas* const canvas, bool top, uint8_t amount) {
    UNUSED(canvas);
    UNUSED(top);
    UNUSED(amount);
}

void draw_money(Canvas* const canvas, uint32_t score) {
    char text[16];
    uint32_t amount = score;

    if(amount < 1000) {
        snprintf(text, sizeof(text), "$%lu", (unsigned long)amount);
    } else {
        char suffix = 'K';

        for(uint8_t i = 0; i < 4; i++) {
            amount /= 1000;

            if(amount < 1000) {
                suffix = MoneyMul[i];
                break;
            }
        }

        snprintf(
            text,
            sizeof(text),
            "$%lu%c",
            (unsigned long)amount,
            suffix);
    }

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rbox(canvas, 89, 53, 36, 10, 2);

    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(
        canvas,
        107,
        58,
        AlignCenter,
        AlignCenter,
        text);

    canvas_set_color(canvas, ColorBlack);
}

void draw_menu(
    Canvas* const canvas,
    const char* text,
    const char* value,
    int8_t y,
    bool left_caret,
    bool right_caret,
    bool selected) {

    if(y < -15 || y >= 64) return;

    if(selected) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, y, 122, LINE_HEIGHT);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }

    canvas_set_font(canvas, FontSecondary);

    canvas_draw_str_aligned(
        canvas,
        4,
        y + ITEM_PADDING + 4,
        AlignLeft,
        AlignCenter,
        text);

    if(left_caret) {
        canvas_draw_str_aligned(
            canvas,
            82,
            y + ITEM_PADDING + 4,
            AlignCenter,
            AlignCenter,
            "<");
    }

    canvas_draw_str_aligned(
        canvas,
        101,
        y + ITEM_PADDING + 4,
        AlignCenter,
        AlignCenter,
        value);

    if(right_caret) {
        canvas_draw_str_aligned(
            canvas,
            119,
            y + ITEM_PADDING + 4,
            AlignCenter,
            AlignCenter,
            ">");
    }

    canvas_set_color(canvas, ColorBlack);
}

void settings_page(Canvas* const canvas, const GameState* game_state) {
    char draw_char[10];
    int start_y = 0;

    if(LINE_HEIGHT * (game_state->selectedMenu + 1) >= 64) {
        start_y -= (LINE_HEIGHT * (game_state->selectedMenu + 1)) - 64;
    }

    int scroll_height = 11 + ITEM_PADDING * 2;
    int scroll_pos =
        (64 * (game_state->selectedMenu + 1)) / 6 - ITEM_PADDING * 2;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 123, scroll_pos, 4, scroll_height);
    canvas_draw_box(canvas, 125, 0, 1, 64);

    snprintf(
        draw_char,
        sizeof(draw_char),
        "%lu",
        (unsigned long)game_state->settings.starting_money);

    draw_menu(
        canvas,
        "Start money",
        draw_char,
        0 * LINE_HEIGHT + start_y,
        game_state->settings.starting_money > game_state->settings.round_price,
        game_state->settings.starting_money < 400,
        game_state->selectedMenu == 0);

    snprintf(
        draw_char,
        sizeof(draw_char),
        "%lu",
        (unsigned long)game_state->settings.round_price);

    draw_menu(
        canvas,
        "Round price",
        draw_char,
        1 * LINE_HEIGHT + start_y,
        game_state->settings.round_price > 10,
        game_state->settings.round_price < game_state->settings.starting_money,
        game_state->selectedMenu == 1);

    snprintf(
        draw_char,
        sizeof(draw_char),
        "%lu",
        (unsigned long)game_state->settings.animation_duration);

    draw_menu(
        canvas,
        "Anim. length",
        draw_char,
        2 * LINE_HEIGHT + start_y,
        game_state->settings.animation_duration > 0,
        game_state->settings.animation_duration < 2000,
        game_state->selectedMenu == 2);

    snprintf(
        draw_char,
        sizeof(draw_char),
        "%lu",
        (unsigned long)game_state->settings.message_duration);

    draw_menu(
        canvas,
        "Popup time",
        draw_char,
        3 * LINE_HEIGHT + start_y,
        game_state->settings.message_duration > 0,
        game_state->settings.message_duration < 2000,
        game_state->selectedMenu == 3);
}
