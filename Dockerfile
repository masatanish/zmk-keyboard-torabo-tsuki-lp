# ZMKファームウェアビルド用Dockerイメージ
# ZMKの公式Dockerイメージをベースに使用（Zephyr SDKが既にインストール済み）
FROM docker.io/zmkfirmware/zmk-dev-arm:3.5

# PyYAMLのインストール（build_all.pyで使用）
RUN apt-get update && \
    apt-get install -y python3-yaml && \
    rm -rf /var/lib/apt/lists/*

# Gitの安全なディレクトリ設定（Dockerマウント時の所有権エラーを回避）
RUN git config --global --add safe.directory '*'

# 作業ディレクトリの設定
WORKDIR /workspace

