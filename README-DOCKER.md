# Dockerビルド環境の使い方

このリポジトリには、ZMKファームウェアをビルドするためのDocker環境が含まれています。

## 前提条件

- Docker と Docker Compose がインストールされていること

## セットアップ

### 1. Dockerイメージのビルド

```bash
docker-compose build
```

### 2. コンテナの起動

```bash
docker-compose up -d
```

### 3. コンテナ内でシェルを開く

```bash
docker-compose exec zmk-build bash
```

## ビルド方法

### 方法1: ビルドスクリプトを使用（推奨）

#### 初回セットアップ

```bash
./build.sh --init
```

これにより、westワークスペースが初期化され、必要な依存関係がダウンロードされます。

#### 個別ビルド

```bash
# 左側（central）をビルド
./build.sh --left-central

# 右側（central）をビルド
./build.sh --right-central

# 左側（peripheral）をビルド
./build.sh --left-peripheral

# 右側（peripheral）をビルド
./build.sh --right-peripheral
```

#### すべてのビルドを実行

```bash
./build.sh --all
```

### 方法2: コンテナ内で手動ビルド

コンテナ内のシェルで以下を実行：

```bash
# コンテナ内のシェルを開く
docker-compose exec zmk-build bash

# 初回のみ: westワークスペースの初期化
west init -l config/
west update

# ビルドの実行（例: 左側、central）
west build -p always -s zmk/app -b bmp_boost -- -DSHIELD=torabo_tsuki_lp_left -DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=y
```

### 方法3: build.yamlを使用したビルド

ZMKのビルドシステムを使用する場合（コンテナ内で実行）：

```bash
cd zmk
./scripts/build-build.sh /workspace/build.yaml
```

## ビルド成果物

ビルドが成功すると、`build/zephyr/` ディレクトリに以下のファイルが生成されます：

- `zmk.uf2` - ファームウェアファイル（書き込み用）

## よく使うコマンド

### コンテナの停止

```bash
docker-compose down
```

### コンテナの再起動

```bash
docker-compose restart
```

### ビルドキャッシュのクリア

```bash
docker-compose down -v
docker-compose build --no-cache
```

### コンテナ内のログを確認

```bash
docker-compose logs zmk-build
```

## トラブルシューティング

### westワークスペースの再初期化

```bash
docker-compose exec zmk-build bash -c "rm -rf zmk && west init -l config/ && west update"
```

### ビルドディレクトリのクリア

```bash
docker-compose exec zmk-build bash -c "rm -rf build"
```

## 注意事項

- 初回の `west update` は時間がかかることがあります（依存関係のダウンロード）
- ビルド成果物は `build/` ディレクトリに保存されます
- westのキャッシュはDockerボリュームに保存されるため、コンテナを削除しても保持されます

