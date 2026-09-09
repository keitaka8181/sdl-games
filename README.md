# sdl-games

SDL2 (C) で書かれた小さな2Dゲームのデモ集。Windows / MSYS2 mingw64 環境を前提にしている。

## 内容

- `breakout.c` — ブロック崩し。パドル・ボール・5行×10列のブロックのシンプルな実装。
- `bullethell.c` — 弾幕シューティングのミニデモ。最大2000発の弾をオブジェクトプーリング＋クアッドツリーで衝突判定し、パーティクル演出も入る。`-DUSE_MIXER` を付けてビルドすると SDL2_mixer 経由の効果音も有効になる。

`bullethell.c` の弾は最大2000発ぶんの固定長配列を使い回すオブジェクトプールで管理しており、発射・消滅のたびに malloc/free を行わない設計になっている。衝突判定も、自機と全弾を毎フレーム総当たりで比較すると最大2000回の比較が必要になり負荷が非現実的になるため、画面をクアッドツリーで分割し自機の周辺にある弾だけを候補として絞り込んでから判定している。

`breakout.c` の当たり判定は `SDL_Rect` 同士の軸並行境界ボックス（AABB）交差判定（`rect_intersect`）のみで行っており、ボールの移動後の矩形を生存しているブロック全個に対して毎フレーム総当たりで比較し、最初に交差したブロック1個だけを破壊してy方向の速度を反転させている。

それぞれ単一の `.c` ファイルに完結している。ビルド済みの `.exe` と `SDL2.dll` はリポジトリに含めていないため、下記の手順でビルドすること。

## ビルド方法

MSYS2 mingw64 の gcc を使う。各ソースの先頭コメントにもコンパイルコマンドが書かれている。

```bash
# breakout
gcc breakout.c -IC:/msys64/mingw64/include/SDL2 -LC:/msys64/mingw64/lib -lmingw32 -lSDL2main -lSDL2 -o breakout.exe

# bullethell（音声なし）
gcc bullethell.c -IC:/msys64/mingw64/include/SDL2 -LC:/msys64/mingw64/lib -lmingw32 -lSDL2main -lSDL2 -o bullethell.exe

# bullethell（音声あり）
gcc bullethell.c -DUSE_MIXER -IC:/msys64/mingw64/include/SDL2 -LC:/msys64/mingw64/lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_mixer -o bullethell.exe
```

`SDL2.dll`（および音声ありの場合は `SDL2_mixer.dll`）を生成した `.exe` と同じフォルダに置く必要がある。

`.vscode/tasks.json` には `main.c` をビルドするタスクが定義されているが、実際のソースファイル名は `breakout.c` / `bullethell.c` なので、VSCode のビルドタスクをそのまま使う場合はファイル名を合わせるかタスク側を書き換える必要がある。

## ライセンス

未定
