#include <gb/gb.h>
#include <stdint.h>
#include <stdio.h>

// アンパサンド(&)スプライト
const uint8_t player_sprite[] = {
    0x18, 0x18,  // ...##...
    0x24, 0x24,  // ..#..#..
    0x24, 0x24,  // ..#..#..
    0x18, 0x18,  // ...##...
    0x7E, 0x7E,  // .######.
    0x42, 0x42,  // .#....#.
    0x66, 0x66,  // .##..##.
    0x00, 0x00   // ........
};

// アスタリスク(*)スプライト
const uint8_t asterisk_sprite[] = {
    0x00, 0x00,  // ........
    0x88, 0x88,  // #...#...
    0x50, 0x50,  // .#.#....
    0x20, 0x20,  // ..#.....
    0x50, 0x50,  // .#.#....
    0x88, 0x88,  // #...#...
    0x00, 0x00,  // ........
    0x00, 0x00   // ........
};

// ゲーム定数
#define GAME_AREA_LEFT 48    // ゲーム領域左端（中央寄せ）
#define GAME_AREA_RIGHT 112  // ゲーム領域右端（幅64ピクセル）
#define PLAYER_SPEED 2
#define ASTEROID_SPEED 1
#define PLAYER_Y 144         // 画面下端から1マス離れた位置

// プレイヤー構造体
typedef struct {
    uint8_t x;
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
Asterisk asteroid;  // 1つだけ
uint16_t score;
uint8_t game_over;
uint16_t frame_count;
uint8_t sound_timer;

// 疑似乱数生成（簡易版）
uint8_t rand_seed = 42;
uint8_t simple_rand() {
    rand_seed = (rand_seed * 73 + 17) % 251;
    return rand_seed;
}

// サウンド初期化と再生
void init_sound() {
    NR52_REG = 0x80;  // サウンド全体をON
    NR51_REG = 0x11;  // チャンネル1を両方のスピーカーへ
    NR50_REG = 0x77;  // 最大音量
}

void play_click_sound() {
    NR10_REG = 0x00;
    NR11_REG = 0x81;  // 短いパルス
    NR12_REG = 0x43;  // 音量エンベロープ
    NR13_REG = 0x73;
    NR14_REG = 0x86;  // トリガー
}

// プレイヤー初期化
void init_player() {
    player.x = 80;  // 中央
    player.sprite_id = 0;

    set_sprite_tile(player.sprite_id, 0);
    move_sprite(player.sprite_id, player.x, PLAYER_Y);
}

// アスタリスク初期化
void init_asteroid() {
    asteroid.active = 0;
    asteroid.sprite_id = 1;
    asteroid.x = 0;
    asteroid.y = 0;
    set_sprite_tile(asteroid.sprite_id, 1);
    move_sprite(asteroid.sprite_id, 0, 0);
}

// 新しいアスタリスクを生成
void spawn_asteroid() {
    if (!asteroid.active) {
        asteroid.active = 1;
        // ゲーム領域内でランダムに配置
        asteroid.x = GAME_AREA_LEFT + (simple_rand() % (GAME_AREA_RIGHT - GAME_AREA_LEFT));
        asteroid.y = 16;
    }
}

// プレイヤー更新
void update_player() {
    uint8_t joypad_state = joypad();

    if (joypad_state & J_LEFT) {
        if (player.x > GAME_AREA_LEFT) {
            player.x -= PLAYER_SPEED;
        }
    }

    if (joypad_state & J_RIGHT) {
        if (player.x < GAME_AREA_RIGHT) {
            player.x += PLAYER_SPEED;
        }
    }

    move_sprite(player.sprite_id, player.x, PLAYER_Y);
}

// アスタリスク更新
void update_asteroid() {
    if (asteroid.active) {
        asteroid.y += ASTEROID_SPEED;

        // 画面下端を通り抜けたらスコア加算
        if (asteroid.y > PLAYER_Y + 8) {
            asteroid.active = 0;
            move_sprite(asteroid.sprite_id, 0, 0);
            score++;
        } else {
            move_sprite(asteroid.sprite_id, asteroid.x, asteroid.y);
        }
    }
}

// 衝突判定
void check_collision() {
    if (asteroid.active) {
        // 簡易的な矩形衝突判定
        if (asteroid.x >= player.x - 6 &&
            asteroid.x <= player.x + 6 &&
            asteroid.y >= PLAYER_Y - 6 &&
            asteroid.y <= PLAYER_Y + 6) {
            game_over = 1;
        }
    }
}

// スコア表示
void display_score() {
    printf("\x1B[0;0HScore:%u", score);
}

// パレット設定
void setup_palette() {
    // スプライトパレット0: プレイヤー（黄色系）
    set_sprite_palette(0, 0, RGB(31, 31, 0));

    // スプライトパレット1: 敵（白）
    set_sprite_palette(1, 0, RGB(31, 31, 31));

    // 背景は黒（デフォルト）
}

// ゲーム初期化
void init_game() {
    // スプライトデータ設定
    set_sprite_data(0, 1, player_sprite);
    set_sprite_data(1, 1, asterisk_sprite);

    // プレイヤーとアスタリスクを初期化
    init_player();
    init_asteroid();

    // サウンド初期化
    init_sound();

    // パレット設定（CGB用）
    setup_palette();

    // スプライト表示をON
    SHOW_SPRITES;
    DISPLAY_ON;

    score = 0;
    game_over = 0;
    frame_count = 0;
    sound_timer = 0;
}

// ゲームオーバー画面
void show_game_over() {
    printf("\x1B[8;6HGAME OVER");
    printf("\x1B[10;5HScore: %u", score);
    printf("\x1B[12;4HPress Any Key");

    while (1) {
        uint8_t joypad_state = joypad();
        // どのボタンでもリトライ可能
        if (joypad_state) {
            // ゲームをリセット
            score = 0;
            game_over = 0;
            frame_count = 0;
            init_player();
            init_asteroid();
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
            update_asteroid();

            // 衝突判定
            check_collision();

            // 定期的に新しいアスタリスクを生成（1行に1つ）
            frame_count++;
            if (frame_count % 60 == 0) {
                spawn_asteroid();
            }

            // カタカタ音を定期的に再生
            sound_timer++;
            if (sound_timer % 15 == 0) {
                play_click_sound();
            }

            // スコア表示
            display_score();

        } else {
            show_game_over();
        }

        wait_vbl_done();
    }
}
