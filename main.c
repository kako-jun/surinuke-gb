#include <gb/gb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// スプライトタイル定義（プレイヤー用）
const uint8_t player_sprite[] = {
    0x3C, 0x3C,
    0x7E, 0x7E,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0xFF, 0xFF,
    0x7E, 0x7E,
    0x3C, 0x3C
};

// アスタリスクスプライト
const uint8_t asterisk_sprite[] = {
    0x18, 0x18,
    0x18, 0x18,
    0xFF, 0xFF,
    0x3C, 0x3C,
    0x3C, 0x3C,
    0xFF, 0xFF,
    0x18, 0x18,
    0x18, 0x18
};

// ゲーム定数
#define MAX_ASTERISKS 5
#define PLAYER_SPEED 2
#define ASTEROID_SPEED 1

// プレイヤー構造体
typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t sprite_id;
} Player;

// アスタリスク構造体
typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t active;
    uint8_t sprite_id;
} Asterisk;

// グローバル変数
Player player;
Asterisk asterisks[MAX_ASTERISKS];
uint16_t score;
uint8_t game_over;
uint16_t frame_count;

// 疑似乱数生成（簡易版）
uint8_t rand_seed = 42;
uint8_t simple_rand() {
    rand_seed = (rand_seed * 73 + 17) % 251;
    return rand_seed;
}

// プレイヤー初期化
void init_player() {
    player.x = 80;
    player.y = 136;
    player.sprite_id = 0;

    set_sprite_tile(player.sprite_id, 0);
    move_sprite(player.sprite_id, player.x, player.y);
}

// アスタリスク初期化
void init_asterisks() {
    uint8_t i;
    for (i = 0; i < MAX_ASTERISKS; i++) {
        asterisks[i].active = 0;
        asterisks[i].sprite_id = i + 1;
        asterisks[i].x = 0;
        asterisks[i].y = 0;
        set_sprite_tile(asterisks[i].sprite_id, 1);
        move_sprite(asterisks[i].sprite_id, 0, 0);
    }
}

// 新しいアスタリスクを生成
void spawn_asterisk() {
    uint8_t i;
    for (i = 0; i < MAX_ASTERISKS; i++) {
        if (!asterisks[i].active) {
            asterisks[i].active = 1;
            asterisks[i].x = 16 + (simple_rand() % 144);
            asterisks[i].y = 16;
            break;
        }
    }
}

// プレイヤー更新
void update_player() {
    uint8_t joypad_state = joypad();

    if (joypad_state & J_LEFT) {
        if (player.x > 16) {
            player.x -= PLAYER_SPEED;
        }
    }

    if (joypad_state & J_RIGHT) {
        if (player.x < 152) {
            player.x += PLAYER_SPEED;
        }
    }

    move_sprite(player.sprite_id, player.x, player.y);
}

// アスタリスク更新
void update_asterisks() {
    uint8_t i;
    for (i = 0; i < MAX_ASTERISKS; i++) {
        if (asterisks[i].active) {
            asterisks[i].y += ASTEROID_SPEED;

            // 画面外に出たら非アクティブに
            if (asterisks[i].y > 160) {
                asterisks[i].active = 0;
                move_sprite(asterisks[i].sprite_id, 0, 0);
                score++;
            } else {
                move_sprite(asterisks[i].sprite_id, asterisks[i].x, asterisks[i].y);
            }
        }
    }
}

// 衝突判定
void check_collisions() {
    uint8_t i;
    for (i = 0; i < MAX_ASTERISKS; i++) {
        if (asterisks[i].active) {
            // 簡易的な矩形衝突判定
            if (asterisks[i].x >= player.x - 8 &&
                asterisks[i].x <= player.x + 8 &&
                asterisks[i].y >= player.y - 8 &&
                asterisks[i].y <= player.y + 8) {
                game_over = 1;
            }
        }
    }
}

// スコア表示
void display_score() {
    printf("\x1B[0;0HScore:%u", score);
}

// ゲーム初期化
void init_game() {
    // スプライトデータ設定
    set_sprite_data(0, 1, player_sprite);
    set_sprite_data(1, 1, asterisk_sprite);

    // プレイヤーとアスタリスクを初期化
    init_player();
    init_asterisks();

    // スプライト表示をON
    SHOW_SPRITES;
    DISPLAY_ON;

    score = 0;
    game_over = 0;
    frame_count = 0;
}

// ゲームオーバー画面
void show_game_over() {
    printf("\x1B[8;5HGAME OVER!");
    printf("\x1B[10;4HScore: %u", score);
    printf("\x1B[12;3HPress START");

    while (1) {
        uint8_t joypad_state = joypad();
        if (joypad_state & J_START) {
            // ゲームをリセット
            score = 0;
            game_over = 0;
            frame_count = 0;
            init_player();
            init_asterisks();
            cls();
            break;
        }
        wait_vbl_done();
    }
}

// メインループ
void main() {
    init_game();

    while (1) {
        if (!game_over) {
            // プレイヤー更新
            update_player();

            // アスタリスク更新
            update_asterisks();

            // 衝突判定
            check_collisions();

            // 定期的に新しいアスタリスクを生成
            frame_count++;
            if (frame_count % 60 == 0) {
                spawn_asterisk();
            }

            // スコア表示
            display_score();

        } else {
            show_game_over();
        }

        wait_vbl_done();
    }
}
