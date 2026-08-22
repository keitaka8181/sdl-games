# sdl-games

SDL2 (C) で書かれた小さな2Dゲームのデモ集。Windows / MSYS2 mingw64 環境を前提にしている。

## 内容

- `breakout.c` — ブロック崩し。パドル・ボール・5行×10列のブロックのシンプルな実装。
- `bullethell.c` — 弾幕シューティングのミニデモ。最大2000発の弾をオブジェクトプーリング＋クアッドツリーで衝突判定し、パーティクル演出も入る。`-DUSE_MIXER` を付けてビルドすると SDL2_mixer 経由の効果音も有効になる。

それぞれ単一の `.c` ファイルに完結している。ビルド済みの `.exe` と実行に必要な `SDL2.dll` も同梱されている。

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
