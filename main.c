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

// アスタリスク(*)スプライト（対称6点、中央寄せ）
const uint8_t asterisk_sprite[] = {
    0x00, 0x00,  // ........
    0x44, 0x44,  // .#...#..
    0x28, 0x28,  // ..#.#...
    0x10, 0x10,  // ...#....
    0x28, 0x28,  // ..#.#...
    0x44, 0x44,  // .#...#..
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
#define PLAYER_Y 144         // 画面下端から1マス離れた位置

// 障害物の最大数（スプライトID 1〜6を使用）
#define MAX_OBSTACLES 6

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
Asterisk obstacles[MAX_OBSTACLES];
uint16_t score;
uint8_t game_over;
uint16_t frame_count;
uint8_t sound_timer;

// 難易度パラメータ
uint8_t obstacle_speed;     // 落下速度（ピクセル/フレーム）
uint8_t spawn_interval;     // 生成間隔（フレーム数）
uint8_t max_active;         // 同時出現数の上限

// 16-bit LFSR擬似乱数生成器（周期65535）
// Fibonacci LFSR: タップ位置 bit 16, 15, 13, 4 (x^16 + x^15 + x^13 + x^4 + 1)
// 最大長（maximal-length）が保証された多項式
uint16_t lfsr_state = 0xACE1u;
uint8_t lfsr_seeded = 0;

uint8_t game_rand(void) {
    uint8_t i;
    uint16_t bit;
    // 初回呼び出し時にDIVレジスタでシードを撹拌
    if (!lfsr_seeded) {
        lfsr_state ^= ((uint16_t)DIV_REG << 8) | DIV_REG;
        if (lfsr_state == 0) lfsr_state = 0xACE1u;  // 0は禁止
        lfsr_seeded = 1;
    }
    // 8回シフトして8ビットの乱数を生成
    for (i = 0; i < 8; i++) {
        // taps: bit 0 (=x^16), bit 1 (=x^15), bit 3 (=x^13), bit 12 (=x^4)
        bit = ((lfsr_state >> 0) ^ (lfsr_state >> 1) ^ (lfsr_state >> 3) ^ (lfsr_state >> 12)) & 1u;
        lfsr_state = (lfsr_state >> 1) | (bit << 15);
    }
    return (uint8_t)(lfsr_state & 0xFF);
}

// サウンド初期化と再生
void init_sound(void) {
    NR52_REG = 0x80;  // サウンド全体をON
    NR51_REG = 0x11;  // チャンネル1を両方のスピーカーへ
    NR50_REG = 0x77;  // 最大音量
}

void play_click_sound(void) {
    NR10_REG = 0x00;
    NR11_REG = 0x81;  // 短いパルス
    NR12_REG = 0x43;  // 音量エンベロープ
    NR13_REG = 0x73;
    NR14_REG = 0x86;  // トリガー
}

// 難易度更新（スコアに応じて段階的に難しくなる）
void update_difficulty(void) {
    if (score < 5) {
        // レベル1: 入門（障害物1個、遅め、60フレーム間隔）
        obstacle_speed = 1;
        spawn_interval = 60;
        max_active = 1;
    } else if (score < 15) {
        // レベル2: 少し増える（障害物2個、45フレーム間隔）
        obstacle_speed = 1;
        spawn_interval = 45;
        max_active = 2;
    } else if (score < 30) {
        // レベル3: 速くなる（障害物3個、速度2、40フレーム間隔）
        obstacle_speed = 2;
        spawn_interval = 40;
        max_active = 3;
    } else if (score < 50) {
        // レベル4: さらに厳しく（障害物4個、30フレーム間隔）
        obstacle_speed = 2;
        spawn_interval = 30;
        max_active = 4;
    } else if (score < 80) {
        // レベル5: 上級（障害物5個、速度3、25フレーム間隔）
        obstacle_speed = 3;
        spawn_interval = 25;
        max_active = 5;
    } else {
        // レベル6: 最高難度（障害物6個、速度3、20フレーム間隔）
        obstacle_speed = 3;
        spawn_interval = 20;
        max_active = MAX_OBSTACLES;
    }
}

// プレイヤー初期化
void init_player(void) {
    player.x = (GAME_AREA_LEFT + GAME_AREA_RIGHT) / 2;  // ゲーム領域の中央
    player.sprite_id = 0;

    set_sprite_tile(player.sprite_id, 0);
    // Game Boy Colorでのみカラーパレットを設定
    if (_cpu == CGB_TYPE) {
        set_sprite_prop(player.sprite_id, 0x00);  // パレット0（シアン）を使用
    }
    move_sprite(player.sprite_id, player.x, PLAYER_Y);
}

// 全障害物の初期化
void init_obstacles(void) {
    uint8_t i;
    for (i = 0; i < MAX_OBSTACLES; i++) {
        obstacles[i].active = 0;
        obstacles[i].sprite_id = i + 1;  // スプライトID 1〜6
        obstacles[i].x = 0;
        obstacles[i].y = 0;
        set_sprite_tile(obstacles[i].sprite_id, 1);
        // Game Boy Colorでのみカラーパレットを設定
        if (_cpu == CGB_TYPE) {
            set_sprite_prop(obstacles[i].sprite_id, 0x10);  // パレット1（赤）を使用
        }
        move_sprite(obstacles[i].sprite_id, 0, 0);
    }
}

// 新しいアスタリスクを生成（空きスロットに配置）
void spawn_obstacle(void) {
    uint8_t i;
    uint8_t active_count = 0;

    // 現在のアクティブ数をカウント
    for (i = 0; i < MAX_OBSTACLES; i++) {
        if (obstacles[i].active) active_count++;
    }

    // 上限に達していたら生成しない
    if (active_count >= max_active) return;

    // 空きスロットを探して生成
    for (i = 0; i < MAX_OBSTACLES; i++) {
        if (!obstacles[i].active) {
            obstacles[i].active = 1;
            // ゲーム領域内でランダムに配置
            obstacles[i].x = GAME_AREA_LEFT + (game_rand() % (GAME_AREA_RIGHT - GAME_AREA_LEFT));
            obstacles[i].y = 16;
            return;
        }
    }
}

// プレイヤー更新
void update_player(void) {
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

// 全障害物の更新
void update_obstacles(void) {
    uint8_t i;
    for (i = 0; i < MAX_OBSTACLES; i++) {
        if (obstacles[i].active) {
            obstacles[i].y += obstacle_speed;

            // 画面下端を通り抜けたらスコア加算
            if (obstacles[i].y > PLAYER_Y + 8) {
                obstacles[i].active = 0;
                move_sprite(obstacles[i].sprite_id, 0, 0);
                score++;
            } else {
                move_sprite(obstacles[i].sprite_id, obstacles[i].x, obstacles[i].y);
            }
        }
    }
}

// 衝突判定（全障害物をチェック）
void check_collision(void) {
    uint8_t i;
    for (i = 0; i < MAX_OBSTACLES; i++) {
        if (obstacles[i].active) {
            // 簡易的な矩形衝突判定
            if (obstacles[i].x >= player.x - 6 &&
                obstacles[i].x <= player.x + 6 &&
                obstacles[i].y >= PLAYER_Y - 6 &&
                obstacles[i].y <= PLAYER_Y + 6) {
                game_over = 1;
                return;
            }
        }
    }
}

// スコア表示（画面下部中央に数字のみ）
void display_score(void) {
    gotoxy(9, 17);
    printf("%u  ", score);  // 末尾スペースで前の桁を消去
}

// 壁の描画
void draw_walls(void) {
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
void setup_palette(void) {
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
void init_game(void) {
    // スプライトデータ設定
    set_sprite_data(0, 1, player_sprite);
    set_sprite_data(1, 1, asterisk_sprite);

    // プレイヤーと障害物を初期化
    init_player();
    init_obstacles();

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

    // 初期難易度を設定
    update_difficulty();
}

// ゲームオーバー画面
void show_game_over(void) {
    uint8_t i;

    // 全障害物スプライトを画面外に退避
    for (i = 0; i < MAX_OBSTACLES; i++) {
        move_sprite(obstacles[i].sprite_id, 0, 0);
        obstacles[i].active = 0;
    }

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
            lfsr_seeded = 0;  // 次のゲームで新しいシードを使う
            init_player();
            init_obstacles();
            cls();
            draw_walls();
            update_difficulty();
            break;
        }
        wait_vbl_done();
    }
}

// メインループ
void main(void) {
    init_game();

    while (1) {
        if (!game_over) {
            // プレイヤー更新
            update_player();

            // 障害物更新
            update_obstacles();

            // 衝突判定
            check_collision();

            // 難易度をスコアに応じて更新
            update_difficulty();

            // 定期的に新しい障害物を生成
            frame_count++;
            if (frame_count >= spawn_interval) {
                spawn_obstacle();
                frame_count = 0;
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
