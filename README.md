# sdl-games

SDL2 (C) で書かれた小さな2Dゲームのデモ集。SDL2 が入っていれば Windows（MSYS2 mingw64）・Linux・macOS のどこでもビルドして実行できる。

GPU アクセラレーションが使えない環境（VM、リモートデスクトップ等）では、自動的にソフトウェアレンダラーにフォールバックして起動する（その際はログに一言出る）。

## 内容

- `breakout.c` — ブロック崩し。パドル・ボール・5行×10列のブロックのシンプルな実装。
- `bullethell.c` — 弾幕シューティングのミニデモ。最大2000発の弾をオブジェクトプーリング＋クアッドツリーで衝突判定し、パーティクル演出も入る。`-DUSE_MIXER` を付けてビルドすると SDL2_mixer 経由の効果音も有効になる。

`bullethell.c` の弾は最大2000発ぶんの固定長配列を使い回すオブジェクトプールで管理しており、発射・消滅のたびに malloc/free を行わない設計になっている。衝突判定も、自機と全弾を毎フレーム総当たりで比較すると最大2000回の比較が必要になり負荷が非現実的になるため、画面をクアッドツリーで分割し自機の周辺にある弾だけを候補として絞り込んでから判定している。

`breakout.c` の当たり判定は `SDL_Rect` 同士の軸並行境界ボックス（AABB）交差判定（`rect_intersect`）のみで行っており、ボールの移動後の矩形を生存しているブロック全個に対して毎フレーム総当たりで比較し、最初に交差したブロック1個だけを破壊してy方向の速度を反転させている。

それぞれ単一の `.c` ファイルに完結している。ビルド済みの実行ファイルと `SDL2.dll` はリポジトリに含めていないため、下記の手順でビルドすること。

## 必要なもの（SDL2 の入れ方）

C コンパイラ、CMake（3.16 以上）、SDL2 を用意する。音声版を作る場合は SDL2_mixer も入れる。

```bash
# Windows（MSYS2 の MINGW64 シェルで）
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-pkgconf mingw-w64-x86_64-SDL2
pacman -S --needed mingw-w64-x86_64-SDL2_mixer   # 音声版を作る場合

# Linux（Debian / Ubuntu）
sudo apt install build-essential cmake pkg-config libsdl2-dev
sudo apt install libsdl2-mixer-dev               # 音声版を作る場合

# macOS（Homebrew）
brew install cmake pkg-config sdl2
brew install sdl2_mixer                          # 音声版を作る場合
```

## ビルド方法（CMake）

リポジトリのルートで実行する。ターゲットは `breakout` と `bullethell` の2つ。

```bash
cmake -B build
cmake --build build

# bullethell を音声ありでビルドする場合
cmake -B build -DUSE_MIXER=ON
cmake --build build
```

実行ファイルは `build/` の下にできる（Windows では `breakout.exe` / `bullethell.exe`）。

## ビルド方法（CMake を使わない場合）

pkg-config で SDL2 のフラグを取得して gcc を直接呼ぶ。

```bash
# breakout
gcc breakout.c $(pkg-config --cflags --libs sdl2) -o breakout

# bullethell（音声なし）
gcc bullethell.c $(pkg-config --cflags --libs sdl2) -lm -o bullethell

# bullethell（音声あり）
gcc bullethell.c -DUSE_MIXER $(pkg-config --cflags --libs sdl2 SDL2_mixer) -lm -o bullethell
```

`bullethell.c` は `sinf` / `cosf` / `sqrtf` を使うため、Linux では `-lm` が必須（macOS・Windows では付けても害はない）。MSYS2 では `pkg-config --libs sdl2` が `-lmingw32 -lSDL2main` も含めて返すので、そのまま使える。

## 実行時の注意

- **Windows**: `SDL2.dll`（音声版は加えて `SDL2_mixer.dll`）を生成した `.exe` と同じフォルダに置く必要がある。MSYS2 なら `C:/msys64/mingw64/bin` にある（MINGW64 シェルから起動する場合は PATH が通っているので不要）。
- **音声版の効果音**: `bullethell` は `shot.wav` と `hit.wav` を**実行時のカレントディレクトリ**から読み込む。ファイルが無い場合や音声デバイスを開けない場合でも、エラーにはならず無音で動く。
