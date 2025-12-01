# surinuke-gb

ゲームボーイ用アスタリスク回避ゲーム

## 遊び方

- **操作**: 十字キー左右で自機（&）を移動
- **目的**: 上から降ってくるアスタリスク（*）を避け続ける
- **ゲームオーバー**: アスタリスクに当たると終了
- **リトライ**: ゲームオーバー後、任意のボタンで再開

## ビルド

### ローカル

```bash
# GBDK-2020が必要
make
```

### Docker

```bash
docker build -t surinuke-gb-builder .
docker run --rm -v $(pwd):/work surinuke-gb-builder
```

## 実行

生成された `surinuke-gb.gb` をエミュレータで実行:

- [BGB](https://bgb.bircd.org/)
- [mGBA](https://mgba.io/)
- [SameBoy](https://sameboy.github.io/)

## 技術仕様

- **開発**: C (GBDK-2020)
- **対応機種**: Game Boy / Game Boy Color
- **カラー**: GBCではシアン(自機)、赤(敵)、グレー(壁)、黄(スコア)

## ライセンス

MIT
