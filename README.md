# surinuke-gb

Game Boy / Game Boy Color 用の回避アクションゲーム。壁に囲まれた狭い通路で、上から降ってくるアスタリスク（*）を自機（&）ですり抜け続ける。

## スクリーンショット

> ビルド後に BGB や SameBoy で実行すると、GBCモードではシアン・赤・グレー・黄のカラー表示。DMGモードではモノクロで動作。

## 遊び方

| 操作 | 機能 |
|------|------|
| 十字キー左右 | 自機を移動 |
| 任意のボタン | ゲームオーバー後にリトライ |

- 上から降ってくるアスタリスク（*）を避ける
- アスタリスクが画面下端を通過するとスコア+1
- 衝突するとゲームオーバー
- 毎回異なるパターンで出現（ハードウェアタイマーによる乱数）
- スコアが上がるほど難しくなる（障害物が増え、速度が上がる）

## ビルド

[GBDK-2020](https://github.com/gbdk-2020/gbdk-2020) v4.3.0 以上が必要。

### ローカルビルド

```bash
export GBDK_HOME=/path/to/gbdk
make
```

### Docker ビルド

```bash
docker build -t surinuke-gb-builder .
docker run --rm -v $(pwd):/work surinuke-gb-builder
```

生成された `surinuke-gb.gb` をエミュレータまたは実機で実行。

## 動作確認済みエミュレータ

| エミュレータ | GBC対応 | 推奨度 |
|---|---|---|
| [SameBoy](https://sameboy.github.io/) | ○ | ★★★ |
| [BGB](https://bgb.bircd.org/) | ○ | ★★★ |
| [mGBA](https://mgba.io/) | ○ | ★★☆ |

## 技術仕様

- **ROM形式**: Game Boy ROM (.gb)
- **対応機種**: DMG（初代Game Boy）/ GBC（Game Boy Color）
- **解像度**: 160×144
- **スプライト**: 最大7個使用（自機＋障害物×6）
- **サウンド**: APUチャンネル1によるクリック音
- **開発環境**: C言語 / GBDK-2020 v4.3.0

### GBCカラーパレット

| 要素 | 色 |
|---|---|
| 自機（&） | シアン |
| 障害物（*） | 赤 |
| 壁 | グレー |
| スコア | 黄 |

## CI/CD

GitHub Actionsでプッシュ時に自動ビルド。成果物（.gb ROM）はArtifactsからダウンロード可能。

## ライセンス

MIT
