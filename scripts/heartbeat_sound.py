#!/usr/bin/env python3
"""心拍数 (BPM) に合わせて, 手術ドラマ/映画風の「ドクン」という心音を鳴らす.

1拍は「ドッ」と「クン」の2打で構成する.
  - ドッ : やや軽めの低い打音
  - クン : 本体. より低く太く, 余韻 (響き) が長い
ドラムっぽさの原因になる「鋭い立ち上がり・高域のアタック・ノイズ」を避け,
  - 柔らかい立ち上がり (十数 ms)
  - 40-60 Hz の太い基音 + 胸越しに聞いたような強いローパス
  - 体内で響いているような短いリバーブ
で, こもった「ドクン」にしている.

使い方:
  python3 scripts/heartbeat_sound.py 72            # 72 BPM で鳴らし続ける
  (実行中に数字を入力して Enter で BPM を変更, q で終了)
  python3 scripts/heartbeat_sound.py 72 --wav out.wav --seconds 10   # WAV 保存

依存: numpy, scipy (再生には aplay / pw-play / paplay のいずれか)
"""

import argparse
import shutil
import subprocess
import sys
import threading
import wave

import numpy as np
from scipy.signal import butter, sosfilt

SR = 44100
BPM_MIN, BPM_MAX = 20.0, 240.0

_LP_BODY = butter(4, 160, "low", fs=SR, output="sos")   # 胸越しのこもり
_LP_FINAL = butter(2, 400, "low", fs=SR, output="sos")  # 歪みで出た倍音のうち耳障りな高域を除去
_HP_DC = butter(2, 25, "high", fs=SR, output="sos")     # スピーカーで出ない超低域/DC を除去


def _hit(f0, f1, attack, tau, dur, rng):
    """柔らかく立ち上がり, 音程がわずかに下がりながら減衰する低音の打撃."""
    t = np.arange(int(dur * SR)) / SR

    # 立ち上がり: sin^2 カーブで attack 秒かけて滑らかに (クリック/アタック感を消す)
    rise = np.where(t < attack, np.sin(0.5 * np.pi * t / attack) ** 2, 1.0)
    env = rise * np.exp(-np.maximum(t - attack, 0) / tau)

    # 基音: f0 -> f1 にゆっくり下降 (「ン」と沈む感じ)
    freq = f1 + (f0 - f1) * np.exp(-t / 0.08)
    phase = 2 * np.pi * np.cumsum(freq) / SR
    body = np.sin(phase) + 0.35 * np.sin(2 * phase + 0.5)  # 2倍音で厚み

    # ごく低域の「ドッ」という圧のノイズ (ローパス後なのでザラつかない)
    thud = sosfilt(_LP_BODY, rng.standard_normal(len(t)))
    thud /= np.max(np.abs(thud)) + 1e-9

    return env * (body + 0.4 * thud)


def _place(buf, sig, start_sec, gain):
    i = int(start_sec * SR)
    n = min(len(sig), len(buf) - i)
    if n > 0:
        buf[i:i + n] += gain * sig[:n]


def _make_reverb(rng, length=0.30, decay=0.06):
    """体内で響くような, 低域だけの短い残響のインパルス応答."""
    t = np.arange(int(length * SR)) / SR
    ir = rng.standard_normal(len(t)) * np.exp(-t / decay)
    ir = sosfilt(_LP_BODY, ir)
    return ir / np.sqrt(np.sum(ir ** 2))


_REVERB_IR = _make_reverb(np.random.default_rng(0))
REVERB_MIX = 0.25


def gap_sec(bpm):
    """ドッ -> クン の間隔. 72 BPM で約 0.20 s, 速いほど詰まる."""
    return float(np.clip(0.24 * 60.0 / bpm, 0.12, 0.22))


def make_beat(bpm, rng):
    """1拍分 (ドッ + クン) を返す. 長さは 1 周期と音の長さの大きい方."""
    period = 60.0 / bpm
    g = gap_sec(bpm)
    length = max(period, g + 0.75)
    buf = np.zeros(int(length * SR))

    k = float(np.clip(period / 0.83, 0.6, 1.0))  # 速い心拍ほど余韻を短く
    jit = lambda s: 1 + s * rng.uniform(-1, 1)  # 拍ごとの微妙なゆらぎ

    # ドッ: 少し高め・短め・控えめ
    _place(buf, _hit(68 * jit(.03), 52, 0.012, 0.040*k, 0.30, rng), 0.0, 0.70 * jit(.05))
    # クン: 低く太く長い. これが「ドクン」の本体
    _place(buf, _hit(58 * jit(.03), 42, 0.018, 0.070*k, 0.45, rng), g, 1.00 * jit(.05))

    buf = sosfilt(_LP_BODY, buf)
    # 柔らかい飽和で倍音 (80-300 Hz) を作り, 小さいスピーカーでも「ドクン」が聞こえるように
    buf = np.tanh(1.6 * buf / (np.max(np.abs(buf)) + 1e-9))
    buf = sosfilt(_LP_FINAL, buf)

    wet = np.convolve(buf, _REVERB_IR)[: len(buf)]
    buf = (1 - REVERB_MIX) * buf + REVERB_MIX * wet
    buf = sosfilt(_HP_DC, buf)
    return 0.9 * buf / (np.max(np.abs(buf)) + 1e-9)


class HeartbeatStream:
    """BPM を途中で変えられる連続心音ジェネレータ."""

    def __init__(self, bpm, volume=0.8, seed=None):
        self.bpm = bpm
        self.volume = volume
        self.rng = np.random.default_rng(seed)
        self._tail = np.zeros(0)
        self.lock = threading.Lock()

    def set_bpm(self, bpm):
        with self.lock:
            self.bpm = float(np.clip(bpm, BPM_MIN, BPM_MAX))

    def next_period(self):
        """次の 1 周期分 (60/bpm 秒) のサンプルを返す. はみ出た音は次周期へ持ち越す."""
        with self.lock:
            bpm = self.bpm
        n = int(SR * 60.0 / bpm)
        beat = make_beat(bpm, self.rng)
        if len(self._tail):
            m = min(len(self._tail), len(beat))
            beat[:m] += self._tail[:m]
            if len(self._tail) > m:
                beat = np.concatenate([beat, self._tail[m:]])
        out, self._tail = beat[:n], beat[n:]
        return np.clip(out * self.volume, -1, 1)


def to_pcm16(x):
    return (x * 32767).astype("<i2").tobytes()


def open_player():
    if shutil.which("aplay"):
        cmd = ["aplay", "-q", "-f", "S16_LE", "-c", "1", "-r", str(SR), "--buffer-time=100000"]
    elif shutil.which("pw-play"):
        cmd = ["pw-play", "--format", "s16", "--channels", "1", "--rate", str(SR), "-"]
    elif shutil.which("paplay"):
        cmd = ["paplay", "--raw", "--format=s16le", "--channels=1", f"--rate={SR}"]
    else:
        sys.exit("aplay / pw-play / paplay が見つかりません. --wav で保存してください.")
    return subprocess.Popen(cmd, stdin=subprocess.PIPE)


def input_loop(stream, stop):
    print("BPM を入力して Enter で変更, q で終了")
    for line in sys.stdin:
        s = line.strip()
        if s in ("q", "quit", "exit"):
            break
        try:
            stream.set_bpm(float(s))
            print(f"-> {stream.bpm:.1f} BPM")
        except ValueError:
            print("数値を入力してください")
    stop.set()


def main():
    p = argparse.ArgumentParser(description="心拍数に合わせて心音を鳴らす")
    p.add_argument("bpm", type=float, help="心拍数 [BPM]")
    p.add_argument("--volume", type=float, default=0.8, help="音量 0-1 (default 0.8)")
    p.add_argument("--wav", help="再生せずに WAV ファイルへ書き出す")
    p.add_argument("--seconds", type=float, default=10.0, help="--wav 時の長さ [s]")
    args = p.parse_args()

    stream = HeartbeatStream(args.bpm, args.volume)
    stream.set_bpm(args.bpm)

    if args.wav:
        chunks, total = [], 0
        while total < args.seconds * SR:
            c = stream.next_period()
            chunks.append(c)
            total += len(c)
        data = np.concatenate(chunks)[: int(args.seconds * SR)]
        with wave.open(args.wav, "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(SR)
            w.writeframes(to_pcm16(data))
        print(f"{args.wav} に保存しました ({args.seconds:.1f} s, {stream.bpm:.1f} BPM)")
        return

    player = open_player()
    stop = threading.Event()
    threading.Thread(target=input_loop, args=(stream, stop), daemon=True).start()
    try:
        while not stop.is_set():
            # パイプへの書き込みがブロックすることで実時間に同期する
            player.stdin.write(to_pcm16(stream.next_period()))
            player.stdin.flush()
    except (KeyboardInterrupt, BrokenPipeError):
        pass
    finally:
        try:
            player.stdin.close()
        except BrokenPipeError:
            pass
        try:
            player.wait(timeout=2)  # バッファを鳴らし切ってから終了
        except subprocess.TimeoutExpired:
            player.terminate()


if __name__ == "__main__":
    main()
