# surinuke-gb 開発者向けドキュメント

Game Boy / Game Boy Color 用の回避アクションゲーム。GBDK-2020 v4.3.0 + C言語。

## プロジェクト構造

```
surinuke-gb/
├── main.c          # ゲーム全体（単一ファイル）
├── Makefile        # ビルド設定（lcc）
├── Dockerfile      # Docker環境（GBDK 4.3.0）
├── docs/           # 仕様書
└── .github/        # CI/CD
```

## ビルド

```bash
make          # ビルド（surinuke-gb.gb を生成）
make clean    # クリーンアップ
```

## 技術メモ

### スプライトデータ形式
- 各行は2バイト: [低ビットプレーン, 高ビットプレーン]
- 両プレーンが同値 → 色0（透明）と色3のみ使用
- GBCパレットは4色フル定義が必要（色3が実際の表示色）

### 座標系
- スプライトX: 8 = 画面左端、Y: 16 = 画面上端
- 背景タイル: 列N = 画面ピクセル N×8
- GAME_AREA_LEFT/RIGHT はスプライト座標（壁タイルの内側に設定）

### 乱数
- DIV_REG（ハードウェアタイマー）で初期シードにエントロピーを追加
- ゲームリスタート時にシードを再撹拌

### GBC判定
- `_cpu == CGB_TYPE` でGBCかDMGかを判定
- DMGではモノクロで動作（パレット設定をスキップ）
