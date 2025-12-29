#!/usr/bin/env python3
"""
build.yamlをパースして、すべてのビルドを実行するスクリプト
"""
import yaml
import subprocess
import sys
import os

def build_firmware(board, shield, snippet=None, cmake_args=None, artifact_name=None):
    """単一のファームウェアをビルド"""
    print(f"\n{'='*60}")
    print(f"ビルド開始: {artifact_name or shield}")
    print(f"  Board: {board}")
    print(f"  Shield: {shield}")
    if snippet:
        print(f"  Snippet: {snippet}")
    if cmake_args:
        print(f"  CMake Args: {cmake_args}")
    print(f"{'='*60}\n")
    
    # ビルドディレクトリを個別に指定（artifact-nameを使用）
    build_dir = f"build/{artifact_name or shield}"
    
    # /workspace/zephyr/Kconfigを一時的にリネーム（Kconfigの再帰的読み込みを回避）
    zephyr_kconfig_backup = None
    zephyr_kconfig_path = "/workspace/zephyr/Kconfig"
    if os.path.exists(zephyr_kconfig_path):
        zephyr_kconfig_backup = f"{zephyr_kconfig_path}.backup"
        try:
            # 既存のバックアップがあれば削除
            if os.path.exists(zephyr_kconfig_backup):
                os.remove(zephyr_kconfig_backup)
            os.rename(zephyr_kconfig_path, zephyr_kconfig_backup)
            # リネームが成功したことを確認
            if not os.path.exists(zephyr_kconfig_path) and os.path.exists(zephyr_kconfig_backup):
                print(f"Kconfigをリネームしました: {zephyr_kconfig_path} -> {zephyr_kconfig_backup}")
        except Exception as e:
            print(f"警告: {zephyr_kconfig_path}のリネームに失敗しました: {e}")
            zephyr_kconfig_backup = None
    
    try:
        # west buildコマンドの構築
        cmd = ["west", "build", "-p", "always", "-s", "zmk/app", "-d", build_dir, "-b", board]
        
        # snippetの処理（複数のsnippetがある場合はスペースで区切られている）
        # snippetはwest buildの-Sオプションで指定（CMake引数ではない）
        if snippet:
            snippets = snippet.split()
            for snip in snippets:
                cmd.extend(["-S", snip])
        
        # CMake引数の構築
        # ZMK_CONFIGを指定（ユーザー設定ディレクトリ）
        # zephyr/module.ymlが存在する場合は、ZMK_EXTRA_MODULESも指定
        cmake_args_list = [f"-DZMK_CONFIG=/workspace/config"]
        if os.path.exists("/workspace/zephyr/module.yml"):
            cmake_args_list.append(f"-DZMK_EXTRA_MODULES=/workspace")
        cmake_args_list.append(f"-DSHIELD={shield}")
        
        # cmake-argsの追加
        if cmake_args:
            # cmake-argsが文字列の場合は分割
            if isinstance(cmake_args, str):
                args = cmake_args.split()
            else:
                args = [cmake_args]
            cmake_args_list.extend(args)
        
        # CMake引数をコマンドに追加
        cmd.extend(["--"] + cmake_args_list)
        
        print(f"実行コマンド: {' '.join(cmd)}\n")
        
        # ビルドの実行
        result = subprocess.run(cmd, cwd="/workspace")
        
        if result.returncode == 0:
            print(f"\n✓ ビルド成功: {artifact_name or shield}\n")
            # 成果物をコピー
            uf2_path = os.path.join(build_dir, "zephyr", "zmk.uf2")
            if os.path.exists(uf2_path):
                out_dir = "/workspace/build/out"
                os.makedirs(out_dir, exist_ok=True)
                dest = os.path.join(out_dir, f"{artifact_name or shield}.uf2")
                subprocess.run(["cp", uf2_path, dest], check=False)
            return True
        else:
            print(f"\n✗ ビルド失敗: {artifact_name or shield}\n")
            return False
    finally:
        # /workspace/zephyr/Kconfigを復元
        if zephyr_kconfig_backup and os.path.exists(zephyr_kconfig_backup):
            try:
                os.rename(zephyr_kconfig_backup, zephyr_kconfig_path)
            except Exception as e:
                print(f"警告: {zephyr_kconfig_path}の復元に失敗しました: {e}")

def main():
    """メイン処理"""
    build_yaml_path = "/workspace/build.yaml"
    
    if not os.path.exists(build_yaml_path):
        print(f"エラー: {build_yaml_path} が見つかりません")
        sys.exit(1)
    
    # west zephyr-exportを実行（Zephyr CMakeパッケージを登録）
    print("Zephyr CMakeパッケージを登録しています...")
    result = subprocess.run(["west", "zephyr-export"], cwd="/workspace", capture_output=True, text=True)
    if result.returncode != 0:
        print(f"警告: west zephyr-export の実行に失敗しました: {result.stderr}")
    else:
        print("Zephyr CMakeパッケージの登録が完了しました。\n")
    
    # build.yamlの読み込み
    with open(build_yaml_path, 'r') as f:
        config = yaml.safe_load(f)
    
    if 'include' not in config:
        print("エラー: build.yamlに'include'キーが見つかりません")
        sys.exit(1)
    
    builds = config['include']
    total = len(builds)
    success = 0
    failed = 0
    
    print(f"\n{'='*60}")
    print(f"build.yamlから {total} 個のビルドを検出しました")
    print(f"{'='*60}\n")
    
    # 各ビルドを実行
    for i, build_config in enumerate(builds, 1):
        print(f"\n[{i}/{total}] ビルドを実行中...")
        
        # 必須パラメータの確認
        if 'board' not in build_config or 'shield' not in build_config:
            print(f"警告: ビルド設定 {i} にboardまたはshieldがありません。スキップします。")
            failed += 1
            continue
        
        board = build_config['board']
        shield = build_config['shield']
        snippet = build_config.get('snippet')
        cmake_args = build_config.get('cmake-args')
        artifact_name = build_config.get('artifact-name')
        
        if build_firmware(board, shield, snippet, cmake_args, artifact_name):
            success += 1
        else:
            failed += 1
    
    # 結果の表示
    print(f"\n{'='*60}")
    print(f"ビルド完了")
    print(f"  成功: {success}/{total}")
    print(f"  失敗: {failed}/{total}")
    print(f"{'='*60}\n")
    
    if failed > 0:
        sys.exit(1)

if __name__ == "__main__":
    main()

