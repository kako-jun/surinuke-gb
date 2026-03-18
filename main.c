#include <gb/gb.h>
#include <stdint.h>
#include <stdio.h>
#include <gb/cgb.h>
#include <gbdk/console.h>

// アンパサンド(&)スプライト（5px幅、列1-5）
const uint8_t player_sprite[] = {
    0x30, 0x30,  // ..##....
    0x48, 0x48,  // .#..#...
    0x48, 0x48,  // .#..1...
    0x30, 0x30,  // ..##....
    0x54, 0x54,  // .#.#.#..
    0x44, 0x44,  // .#...#..
    0x34, 0x34,  // ..##.#..
    0x00, 0x00   // ........
};

// アスタリスク(*)スプライト（5px幅、列1-5、&と同幅）
const uint8_t asterisk_sprite[] = {
    0x00, 0x00,  // ........
    0x28, 0x28,  // ..#.#...
    0x10, 0x10,  // ...#....
    0x54, 0x54,  // .#.#.#..
    0x10, 0x10,  // ...#....
    0x28, 0x28,  // ..#.#...
    0x00, 0x00,  // ........
    0x00, 0x00   // ........
};

// 壁用背景タイル（べた塗り）
const uint8_t wall_tile[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};
// フォントと衝突しないよう高いタイルインデックスを使用
#define WALL_TILE_ID 128
static const uint8_t WALL_TILE_IDX[1] = {WALL_TILE_ID};

// ======== ゲーム定数 ========
#define NUM_LANES     8     // 通路の列数（タイル列6-13）
#define LANE_WIDTH    8     // 1列 = 8px = 1タイル
#define FIRST_LANE_X  56    // レーン0のsprite_x（タイル列6の左端、壁タイル5の右隣）
#define LAST_LANE_X   112   // レーン7のsprite_x（タイル列13）
#define PLAYER_Y      144   // 自機のY座標（画面下部）
#define PLAYER_ROW    18    // PLAYER_Y / LANE_WIDTH = 144 / 8
#define PLAYER_SPEED  2     // 自機の移動速度（px/frame）
#define MAX_ENEMIES   6     // 同時表示する敵スプライト数

// ======== グローバル変数 ========
uint8_t player_x;           // 自機のX座標（ピクセル、滑らかに移動）

typedef struct {
    uint8_t lane;           // 列 (0-7)
    uint8_t y;              // sprite Y座標（ピクセル、滑らかに落下）
    uint8_t sprite_id;
} Enemy;

Enemy enemies[MAX_ENEMIES];
uint16_t score;
uint8_t game_over;
uint8_t fall_speed;         // 落下速度（px/frame）

// 16-bit LFSR擬似乱数生成器（周期65535）
uint16_t lfsr_state = 0xACE1u;
uint8_t lfsr_seeded = 0;

uint8_t game_rand(void) {
    uint8_t i;
    uint16_t bit;
    if (!lfsr_seeded) {
        lfsr_state ^= ((uint16_t)DIV_REG << 8) | DIV_REG;
        if (lfsr_state == 0) lfsr_state = 0xACE1u;
        lfsr_seeded = 1;
    }
    for (i = 0; i < 8; i++) {
        bit = ((lfsr_state >> 0) ^ (lfsr_state >> 1) ^ (lfsr_state >> 3) ^ (lfsr_state >> 12)) & 1u;
        lfsr_state = (lfsr_state >> 1) | (bit << 15);
    }
    return (uint8_t)(lfsr_state & 0xFF);
}

// ======== サウンド ========
void init_sound(void) {
    NR52_REG = 0x80;
    NR51_REG = 0x11;
    NR50_REG = 0x77;
}

void play_move_sound(void) {
    NR10_REG = 0x00;
    NR11_REG = 0x81;
    NR12_REG = 0x43;
    NR13_REG = 0x73;
    NR14_REG = 0x86;
}

void play_hit_sound(void) {
    NR10_REG = 0x00;
    NR11_REG = 0xC1;
    NR12_REG = 0xF3;
    NR13_REG = 0x00;
    NR14_REG = 0x87;
}

// ======== レーン→座標変換 ========
uint8_t lane_to_x(uint8_t lane) {
    return FIRST_LANE_X + lane * LANE_WIDTH;
}

// ======== 壁の描画 ========
void draw_walls(void) {
    uint8_t i;
    set_bkg_data(WALL_TILE_ID, 1, wall_tile);
    for (i = 0; i < 18; i++) {
        set_bkg_tiles(5, i, 1, 1, WALL_TILE_IDX);
        set_bkg_tiles(14, i, 1, 1, WALL_TILE_IDX);
    }
    if (_cpu == CGB_TYPE) {
        VBK_REG = 1;
        for (i = 0; i < 18; i++) {
            set_bkg_tiles(5, i, 1, 1, WALL_TILE_IDX);
            set_bkg_tiles(14, i, 1, 1, WALL_TILE_IDX);
        }
        VBK_REG = 0;
    }
    SHOW_BKG;
}

// ======== パレット ========
void setup_palette(void) {
    if (_cpu == CGB_TYPE) {
        static const uint16_t pal_player[] = {
            RGB(0, 0, 0), RGB(0, 16, 16), RGB(0, 24, 24), RGB(0, 31, 31)
        };
        static const uint16_t pal_enemy[] = {
            RGB(0, 0, 0), RGB(16, 0, 0), RGB(24, 0, 0), RGB(31, 0, 0)
        };
        static const uint16_t pal_bkg[] = {
            RGB(0, 0, 0), RGB(31, 31, 0), RGB(20, 20, 0), RGB(31, 31, 16)
        };
        static const uint16_t pal_wall[] = {
            RGB(0, 0, 0), RGB(8, 8, 8), RGB(16, 16, 16), RGB(12, 12, 12)
        };
        set_sprite_palette(0, 1, pal_player);
        set_sprite_palette(1, 1, pal_enemy);
        set_bkg_palette(0, 1, pal_bkg);
        set_bkg_palette(1, 1, pal_wall);
    }
}

// ======== 初期化 ========
void init_player(void) {
    // 中央レーンの左端に配置
    player_x = lane_to_x(NUM_LANES / 2);
    set_sprite_tile(0, 0);
    if (_cpu == CGB_TYPE) {
        set_sprite_prop(0, 0x00);
    }
    move_sprite(0, player_x, PLAYER_Y);
}

void init_enemies(void) {
    uint8_t i;
    for (i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].sprite_id = i + 1;
        enemies[i].lane = game_rand() % NUM_LANES;
        enemies[i].y = 16 + i * LANE_WIDTH;  // 等間隔で配置
        set_sprite_tile(enemies[i].sprite_id, 1);
        if (_cpu == CGB_TYPE) {
            set_sprite_prop(enemies[i].sprite_id, 0x10);
        }
        move_sprite(enemies[i].sprite_id, lane_to_x(enemies[i].lane), enemies[i].y);
    }
}

void init_game(void) {
    cls();  // 画面クリア（ゴミタイル除去）

    set_sprite_data(0, 1, player_sprite);
    set_sprite_data(1, 1, asterisk_sprite);
    init_player();
    init_enemies();
    init_sound();
    draw_walls();
    setup_palette();
    SHOW_SPRITES;
    DISPLAY_ON;

    score = 0;
    game_over = 0;
    fall_speed = 1;
}

// ======== 難易度 ========
void update_difficulty(void) {
    if (score < 10) {
        fall_speed = 1;
    } else if (score < 30) {
        fall_speed = 2;
    } else if (score < 60) {
        fall_speed = 3;
    } else {
        fall_speed = 4;
    }
}

// ======== プレイヤー更新（滑らかなピクセル移動、マス判定は離散） ========
void update_player(void) {
    uint8_t pad = joypad();

    if (pad & J_LEFT) {
        if (player_x > FIRST_LANE_X) {
            player_x -= PLAYER_SPEED;
            if (player_x < FIRST_LANE_X) player_x = FIRST_LANE_X;
        }
    }
    if (pad & J_RIGHT) {
        if (player_x < LAST_LANE_X) {
            player_x += PLAYER_SPEED;
            if (player_x > LAST_LANE_X) player_x = LAST_LANE_X;
        }
    }
    move_sprite(0, player_x, PLAYER_Y);
}

// 自機の「今いるマス」を取得（sprite_xからレーン番号に変換）
uint8_t get_player_lane(void) {
    return (player_x - FIRST_LANE_X + LANE_WIDTH / 2) / LANE_WIDTH;
}

// ======== 敵更新 ========
void update_enemies(void) {
    uint8_t i, j, min_y;

    for (i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].y += fall_speed;

        // 当たり判定: 同じマス（タイル行）かつ同じ列
        // 敵のマス境界をまたいだ瞬間に「今いる行」が切り替わる
        if (enemies[i].y / LANE_WIDTH == PLAYER_ROW) {
            if (enemies[i].lane == get_player_lane()) {
                game_over = 1;
                play_hit_sound();
                return;
            }
        }

        // 画面下を通過 → スコア加算＋リサイクル
        if (enemies[i].y > PLAYER_Y + LANE_WIDTH) {
            score++;

            // 最も上にいる敵を見つけ、その1行上に配置
            min_y = 255;
            for (j = 0; j < MAX_ENEMIES; j++) {
                if (j != i && enemies[j].y < min_y) {
                    min_y = enemies[j].y;
                }
            }
            enemies[i].y = min_y - LANE_WIDTH;
            enemies[i].lane = game_rand() % NUM_LANES;
        }

        move_sprite(enemies[i].sprite_id, lane_to_x(enemies[i].lane), enemies[i].y);
    }
}

// ======== スコア表示 ========
void display_score(void) {
    gotoxy(9, 17);
    printf("%u  ", score);
}

// ======== ゲームオーバー ========
void show_game_over(void) {
    uint8_t i;

    for (i = 0; i < MAX_ENEMIES; i++) {
        move_sprite(enemies[i].sprite_id, 0, 0);
    }

    // 画面幅20タイル、壁内は列6-13（8タイル幅）
    gotoxy(6, 8);
    printf("GAMEOVER");
    gotoxy(8, 10);
    printf("%u", score);
    gotoxy(4, 12);
    printf("Press Any Key");

    while (joypad()) {
        wait_vbl_done();
    }

    while (1) {
        if (joypad()) {
            score = 0;
            game_over = 0;
            fall_speed = 1;
            lfsr_seeded = 0;
            init_player();
            init_enemies();
            cls();
            draw_walls();
            update_difficulty();
            break;
        }
        wait_vbl_done();
    }
}

// ======== メインループ ========
void main(void) {
    init_game();

    while (1) {
        if (!game_over) {
            update_player();
            update_enemies();
            update_difficulty();
            display_score();
        } else {
            show_game_over();
        }

        wait_vbl_done();
    }
}
