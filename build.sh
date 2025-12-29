#!/bin/bash
# ZMKファームウェアビルドスクリプト

set -e

# カラー出力
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 使用方法の表示
usage() {
    echo "使用方法: $0 [オプション]"
    echo ""
    echo "オプション:"
    echo "  --init           westワークスペースを初期化（初回のみ）"
    echo "  --all            build.yamlのすべてのビルドを実行"
    echo "  --left-central   左側（central）をビルド"
    echo "  --right-central  右側（central）をビルド"
    echo "  --left-peripheral 左側（peripheral）をビルド"
    echo "  --right-peripheral 右側（peripheral）をビルド"
    echo "  --split          right-centralとleft-peripheralを同時にビルド"
    echo "  --clean          ビルド成果物を削除"
    echo "  --help           このヘルプを表示"
    echo ""
    echo "例:"
    echo "  $0 --init              # 初回セットアップ"
    echo "  $0 --left-central      # 左側（central）をビルド"
    echo "  $0 --split             # right-centralとleft-peripheralを同時にビルド"
    echo "  $0 --all               # すべてのビルドを実行"
    echo "  $0 --clean             # ビルド成果物を削除"
}

# Dockerコンテナの起動確認
ensure_container() {
    if ! docker ps --format '{{.Names}}' | grep -q '^zmk-keyboard-build$'; then
        echo -e "${YELLOW}Dockerコンテナを起動しています...${NC}"
        docker compose up -d
        # コンテナが完全に起動するまで待機
        sleep 3
        # コンテナが起動しているか再確認
        if ! docker ps --format '{{.Names}}' | grep -q '^zmk-keyboard-build$'; then
            echo -e "${YELLOW}コンテナの起動を待っています...${NC}"
            sleep 2
        fi
    fi
}

# westワークスペースの初期化
init_workspace() {
    ensure_container
    echo -e "${BLUE}westワークスペースを初期化しています...${NC}"
    docker compose exec zmk-build bash -c "
        # Gitの安全なディレクトリとして追加（所有権エラーを回避）
        git config --global --add safe.directory '*'
        
        if [ ! -d 'zmk' ]; then
            west init -l config/
            west update
            west zephyr-export
            echo '初期化が完了しました。'
        else
            echo 'ワークスペースは既に初期化されています。'
            echo '再初期化する場合は、zmkディレクトリを削除してください。'
        fi
    "
}

# 単一ビルドの実行
build_single() {
    local board=$1
    local shield=$2
    local cmake_args=$3
    local artifact_name=$4

    ensure_container
    echo -e "${GREEN}ビルドを開始します: ${artifact_name}${NC}"
    
    # ビルドディレクトリを個別に指定（artifact-nameを使用）
    local build_dir="build/${artifact_name}"
    
    docker compose exec zmk-build bash -c "
        # /workspace/zephyr/Kconfigを一時的にリネーム（Kconfigの再帰的読み込みを回避）
        if [ -f /workspace/zephyr/Kconfig ]; then
            mv /workspace/zephyr/Kconfig /workspace/zephyr/Kconfig.backup
        fi
        
        # ビルド実行（studio-rpc-usb-uartスニペットを追加）
        if [ -f /workspace/zephyr/module.yml ]; then
            west build -p always -s zmk/app -d ${build_dir} -b ${board} -S studio-rpc-usb-uart -- -DZMK_CONFIG=/workspace/config -DZMK_EXTRA_MODULES=/workspace -DSHIELD=${shield} ${cmake_args} || BUILD_ERROR=\$?
        else
            west build -p always -s zmk/app -d ${build_dir} -b ${board} -S studio-rpc-usb-uart -- -DZMK_CONFIG=/workspace/config -DSHIELD=${shield} ${cmake_args} || BUILD_ERROR=\$?
        fi
        
        # /workspace/zephyr/Kconfigを復元
        if [ -f /workspace/zephyr/Kconfig.backup ]; then
            mv /workspace/zephyr/Kconfig.backup /workspace/zephyr/Kconfig
        fi
        
        # エラーがあれば終了
        if [ -n \"\${BUILD_ERROR}\" ]; then
            exit \${BUILD_ERROR}
        fi

        # 成果物をコピー
        if [ -f ${build_dir}/zephyr/zmk.uf2 ]; then
            mkdir -p /workspace/build/out
            cp ${build_dir}/zephyr/zmk.uf2 /workspace/build/out/${artifact_name}.uf2
        fi
    "
    echo -e "${GREEN}ビルドが完了しました: ${artifact_name}${NC}"
}

# build.yamlのすべてのビルドを実行
build_all() {
    ensure_container
    echo -e "${BLUE}build.yamlのすべてのビルドを実行します...${NC}"
    docker compose exec zmk-build bash -c "
        python3 /workspace/build_all.py
    "
}

# ビルド成果物のクリーンアップ
clean() {
    echo -e "${YELLOW}ビルド成果物を削除しています...${NC}"
    
    # ホスト側から削除（root権限で作成されたファイルのためsudoが必要）
    # マウントされたボリュームなので、ホスト側から削除すればコンテナ内からも見えなくなる
    if [ -d "build" ]; then
        sudo rm -rf build/*
        echo -e "${GREEN}ビルド成果物を削除しました${NC}"
    else
        echo -e "${YELLOW}buildディレクトリが存在しません${NC}"
    fi
    
    echo -e "${GREEN}クリーンアップが完了しました${NC}"
}

# メイン処理
main() {
    ensure_container

    if [ $# -eq 0 ]; then
        usage
        exit 1
    fi

    case "$1" in
        --init)
            init_workspace
            ;;
        --all)
            build_all
            ;;
        --left-central)
            build_single "bmp_boost" "torabo_tsuki_lp_left" "-DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=y" "torabo_tsuki_lp_left_central"
            ;;
        --right-central)
            build_single "bmp_boost" "torabo_tsuki_lp_right" "-DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=y" "torabo_tsuki_lp_right_central"
            ;;
        --left-peripheral)
            build_single "bmp_boost" "torabo_tsuki_lp_left" "" "torabo_tsuki_lp_left_peripheral"
            ;;
        --right-peripheral)
            build_single "bmp_boost" "torabo_tsuki_lp_right" "" "torabo_tsuki_lp_right_peripheral"
            ;;
        --split)
            echo -e "${BLUE}right-centralとleft-peripheralを同時にビルドします...${NC}"
            build_single "bmp_boost" "torabo_tsuki_lp_right" "-DCONFIG_ZMK_SPLIT_ROLE_CENTRAL=y" "torabo_tsuki_lp_right_central"
            build_single "bmp_boost" "torabo_tsuki_lp_left" "" "torabo_tsuki_lp_left_peripheral"
            echo -e "${GREEN}すべてのビルドが完了しました${NC}"
            ;;
        --clean)
            clean
            ;;
        --help)
            usage
            ;;
        *)
            echo -e "${YELLOW}不明なオプション: $1${NC}"
            usage
            exit 1
            ;;
    esac
}

main "$@"

