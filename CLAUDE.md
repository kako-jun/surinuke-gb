# surinuke-gb 開発者向けドキュメント

ゲームボーイ用アスタリスク回避ゲーム。GBDK-2020 + C言語。

## コンセプト

- 縦長画面レイアウト、黒背景
- 自機（&）は画面下部で左右移動のみ
- 敵（*）が上から降ってくる
- カタカタ音のシンプルな効果音

## プロジェクト構造

```
surinuke-gb/
├── main.c          # ゲーム全体
├── Makefile        # ビルド設定
├── Dockerfile      # Docker環境
└── .github/        # CI/CD
```

## ゲーム定数

```c
#define GAME_AREA_LEFT 48    // ゲーム領域左端
#define GAME_AREA_RIGHT 112  // ゲーム領域右端（幅64px）
#define PLAYER_SPEED 2       // 移動速度
#define ASTEROID_SPEED 1     // 落下速度
#define PLAYER_Y 144         // プレイヤーY座標
```

## データ構造

### プレイヤー

```c
typedef struct {
    uint8_t x;
    uint8_t sprite_id;
} Player;
```

### アスタリスク

```c
typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t active;
    uint8_t sprite_id;
} Asterisk;
```

## スプライト設計

```c
// アンパサンド(&) - 8x8ピクセル
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

// アスタリスク(*) - 8x8ピクセル
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
```

## GBC カラーパレット

```c
// スプライトパレット0: プレイヤー（シアン）
RGB(0, 31, 31)

// スプライトパレット1: 敵（赤）
RGB(31, 0, 0)

// 背景パレット0: スコア（黄）
RGB(31, 31, 0)

// 背景パレット1: 壁（グレー）
RGB(12, 12, 12)
```

## サウンド実装

```c
void play_click_sound() {
    NR10_REG = 0x00;
    NR11_REG = 0x81;  // 短いパルス
    NR12_REG = 0x43;  // 音量エンベロープ
    NR13_REG = 0x73;
    NR14_REG = 0x86;  // トリガー
}
```

## ゲームループ

```c
while (1) {
    if (!game_over) {
        update_player();
        update_asteroid();
        check_collision();

        // 60フレームごとに新しいアスタリスク生成
        if (frame_count % 60 == 0) spawn_asteroid();

        // 15フレームごとにカタカタ音
        if (sound_timer % 15 == 0) play_click_sound();

        display_score();
    } else {
        show_game_over();
    }
    wait_vbl_done();
}
```

## 衝突判定

```c
// 簡易的な矩形衝突判定（6ピクセルの当たり判定）
if (asteroid.x >= player.x - 6 &&
    asteroid.x <= player.x + 6 &&
    asteroid.y >= PLAYER_Y - 6 &&
    asteroid.y <= PLAYER_Y + 6) {
    game_over = 1;
}
```

## 乱数生成

```c
uint8_t rand_seed = 42;
uint8_t simple_rand() {
    rand_seed = (rand_seed * 73 + 17) % 251;
    return rand_seed;
}
```

## ビルド

```bash
make          # ビルド
make clean    # クリーンアップ
```

## CI/CD

- GitHub Actions: mainブランチへのプッシュで自動ビルド
- Artifacts: ROMファイルをダウンロード可能
