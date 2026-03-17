#include <gb/gb.h>
#include <stdint.h>
#include <stdio.h>
#include <gb/cgb.h>
#include <gbdk/console.h>

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

// アスタリスク(*)スプライト（中央寄せ）
const uint8_t asterisk_sprite[] = {
    0x00, 0x00,  // ........
    0x49, 0x49,  // .#..#..#
    0x2A, 0x2A,  // ..#.#.#.
    0x1C, 0x1C,  // ...###..
    0x2A, 0x2A,  // ..#.#.#.
    0x49, 0x49,  // .#..#..#
    0x00, 0x00,  // ........
    0x00, 0x00   // ........
};

// 壁用背景タイル（縦線）
const uint8_t wall_tile[] = {
    0xFF, 0xFF,  // ########
    0xFF, 0xFF,  // ########
    0xFF, 0xFF,  // ########
    0xFF, 0xFF,  // ########
    0xFF, 0xFF,  // ########
    0xFF, 0xFF,  // ########
    0xFF, 0xFF,  // ########
    0xFF, 0xFF   // ########
};
static const uint8_t WALL_TILE_IDX[1] = {1};

// ゲーム定数
#define GAME_AREA_LEFT 56    // ゲーム領域左端（左壁タイルの右隣）
#define GAME_AREA_RIGHT 104  // ゲーム領域右端（右壁タイルの左隣、スプライト幅考慮）
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

// 疑似乱数生成（DIV_REGで初期シードにエントロピーを追加）
uint8_t rand_seed = 42;
uint8_t rand_seeded = 0;

uint8_t simple_rand() {
    // 初回呼び出し時にDIVレジスタ（タイマー）でシードを撹拌
    if (!rand_seeded) {
        rand_seed ^= DIV_REG;
        rand_seeded = 1;
    }
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
    player.x = (GAME_AREA_LEFT + GAME_AREA_RIGHT) / 2;  // ゲーム領域の中央
    player.sprite_id = 0;

    set_sprite_tile(player.sprite_id, 0);
    // Game Boy Colorでのみカラーパレットを設定
    if (_cpu == CGB_TYPE) {
        set_sprite_prop(player.sprite_id, 0x00);  // パレット0（シアン）を使用
    }
    move_sprite(player.sprite_id, player.x, PLAYER_Y);
}

// アスタリスク初期化
void init_asteroid() {
    asteroid.active = 0;
    asteroid.sprite_id = 1;
    asteroid.x = 0;
    asteroid.y = 0;
    set_sprite_tile(asteroid.sprite_id, 1);
    // Game Boy Colorでのみカラーパレットを設定
    if (_cpu == CGB_TYPE) {
        set_sprite_prop(asteroid.sprite_id, 0x10);  // パレット1（赤）を使用
    }
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

// スコア表示（画面下部中央に数字のみ）
void display_score() {
    gotoxy(9, 17);
    printf("%u  ", score);  // 末尾スペースで前の桁を消去
}

// 壁の描画
void draw_walls() {
    uint8_t i;

    // 背景タイルデータを設定（タイル1に壁を設定）
    set_bkg_data(1, 1, wall_tile);

    // 左右の壁を画面全体に描画（タイル設定）
    for (i = 0; i < 18; i++) {
        set_bkg_tiles(5, i, 1, 1, WALL_TILE_IDX);  // 左の壁
        set_bkg_tiles(14, i, 1, 1, WALL_TILE_IDX); // 右の壁
    }

    // Game Boy Colorでのみ壁の色属性を設定
    if (_cpu == CGB_TYPE) {
        // VRAMバンク1に切り替えて属性を設定（壁をパレット1に）
        VBK_REG = 1;
        for (i = 0; i < 18; i++) {
            set_bkg_tiles(5, i, 1, 1, WALL_TILE_IDX);  // 左の壁の属性（パレット1）
            set_bkg_tiles(14, i, 1, 1, WALL_TILE_IDX); // 右の壁の属性（パレット1）
        }
        VBK_REG = 0;
    }

    SHOW_BKG;
}

// パレット設定（Game Boy Color専用）
// GBCパレットは4色構成: [色0, 色1, 色2, 色3]
// スプライトの色0は常に透明。タイルデータで両ビットプレーンが同値の場合、色3が使われる
void setup_palette() {
    if (_cpu == CGB_TYPE) {
        // スプライトパレット0: プレイヤー（シアン）
        // 色0=透明, 色1=暗シアン, 色2=シアン, 色3=明シアン（実際に表示される色）
        static const uint16_t sprite_palette_player[] = {
            RGB(0, 0, 0), RGB(0, 16, 16), RGB(0, 24, 24), RGB(0, 31, 31)
        };
        // スプライトパレット1: 敵（赤）
        static const uint16_t sprite_palette_enemy[] = {
            RGB(0, 0, 0), RGB(16, 0, 0), RGB(24, 0, 0), RGB(31, 0, 0)
        };
        // 背景パレット0: スコア表示用（黒背景に黄色テキスト）
        static const uint16_t bkg_palette_score[] = {
            RGB(0, 0, 0), RGB(31, 31, 0), RGB(20, 20, 0), RGB(31, 31, 16)
        };
        // 背景パレット1: 壁用（グレー系）
        static const uint16_t bkg_palette_wall[] = {
            RGB(0, 0, 0), RGB(8, 8, 8), RGB(16, 16, 16), RGB(12, 12, 12)
        };

        set_sprite_palette(0, 1, sprite_palette_player);
        set_sprite_palette(1, 1, sprite_palette_enemy);
        set_bkg_palette(0, 1, bkg_palette_score);
        set_bkg_palette(1, 1, bkg_palette_wall);
    }
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

    // 壁を描画
    draw_walls();

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
    // アスタリスクスプライトを画面外に退避
    move_sprite(asteroid.sprite_id, 0, 0);
    asteroid.active = 0;

    printf("\x1B[8;6HGAME OVER");
    printf("\x1B[10;9H%u", score);
    printf("\x1B[12;4HPress Any Key");

    // ボタンが離されるまで待つ（即リスタート防止）
    while (joypad()) {
        wait_vbl_done();
    }

    // ボタン入力を待つ
    while (1) {
        uint8_t joypad_state = joypad();
        if (joypad_state) {
            score = 0;
            game_over = 0;
            frame_count = 0;
            rand_seeded = 0;  // 次のゲームで新しいシードを使う
            init_player();
            init_asteroid();
            cls();
            draw_walls();
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
