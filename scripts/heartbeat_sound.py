#!/usr/bin/env python3
import argparse
import queue
import shutil
import subprocess
import sys
import threading
import time
import wave

import numpy as np
from scipy.signal import butter, fftconvolve, sosfilt

SR = 44100
BPM_MIN, BPM_MAX = 20.0, 240.0


def _place(buf, sig, start_sec, gain):
    i = int(start_sec * SR)
    n = min(len(sig), len(buf) - i)
    if n > 0:
        buf[i:i + n] += gain * sig[:n]


_LP_NB = butter(2, 30, "low", fs=SR, output="sos")
TV_PITCH = 0.8
TV_REVERB = 1.0


def _make_tv_reverb(rng, length=3.0, rt60=3.5):
    t = np.arange(int(length * SR)) / SR
    ir = rng.standard_normal(len(t)) * np.exp(-t * 6.91 / rt60)
    ir = sosfilt(butter(2, [80, 450], "bandpass", fs=SR, output="sos"), ir)
    return ir / np.sqrt(np.sum(ir ** 2))


_TV_REVERB_IR = _make_tv_reverb(np.random.default_rng(1))
_TV_REVERB_GAIN = 0.07
_HP_TV = butter(2, 45, "high", fs=SR, output="sos")
_LP_TV = butter(2, 1000, "low", fs=SR, output="sos")


def _hit_tv(f0, f_mid, f_end, dur, rng, drop=0.045, attack=0.012):
    t = np.arange(int(dur * SR)) / SR

    freq = f_end + (f_mid - f_end) * (1 - t / dur) + (f0 - f_mid) * np.exp(-t / drop)
    phase = 2 * np.pi * np.cumsum(freq * TV_PITCH) / SR

    n = rng.standard_normal(len(t)) + 1j * rng.standard_normal(len(t))
    n = sosfilt(_LP_NB, n.real) + 1j * sosfilt(_LP_NB, n.imag)
    sig = np.real(n * np.exp(1j * phase))
    sig /= np.sqrt(np.mean(sig ** 2)) + 1e-9

    rise = np.where(t < attack, np.sin(0.5 * np.pi * t / attack) ** 2, 1.0)
    fall = (1 - t / dur) ** 1.5
    return rise * fall * sig


def tv_gap_sec(bpm):
    return float(np.clip(0.32 * 60.0 / bpm, 0.15, 0.45))


def make_beat(bpm, rng):
    period = 60.0 / bpm
    g = tv_gap_sec(bpm)
    buf = np.zeros(int(max(period, g + 0.25) * SR))

    k = float(np.clip(period / 0.9, 0.7, 1.0))
    jit = lambda s: 1 + s * rng.uniform(-1, 1)

    _place(buf, _hit_tv(350 * jit(.05), 145 * jit(.03), 85, 0.16 * k, rng), 0.0, 1.00 * jit(.04))
    _place(buf, _hit_tv(330 * jit(.05), 140 * jit(.03), 85, 0.13 * k, rng), g, 0.85 * jit(.04))

    buf = sosfilt(_LP_TV, sosfilt(_HP_TV, buf))
    buf = buf / (np.max(np.abs(buf)) + 1e-9)

    wet = fftconvolve(buf, _TV_REVERB_IR)
    dry = np.concatenate([buf, np.zeros(len(wet) - len(buf))])
    buf = dry + TV_REVERB * _TV_REVERB_GAIN * wet
    return 0.9 * buf / (np.max(np.abs(buf)) + 1e-9)


DEFAULT_VOLUME = 2.0


def soft_limit(x):
    return np.tanh(x)


class HeartbeatStream:
    def __init__(self, bpm, volume=DEFAULT_VOLUME, seed=None):
        self.bpm = bpm
        self.volume = volume
        self.rng = np.random.default_rng(seed)
        self._tail = np.zeros(0)
        self.lock = threading.Lock()

    def set_bpm(self, bpm):
        with self.lock:
            self.bpm = float(np.clip(bpm, BPM_MIN, BPM_MAX))

    def next_period(self):
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
        return soft_limit(out * self.volume)


class TriggeredHeartbeat:
    CHUNK = 1024
    def __init__(self, volume=DEFAULT_VOLUME, seed=None):
        self.volume = volume
        self.rng = np.random.default_rng(seed)
        self._buf = np.zeros(0)
        self._events = queue.Queue()

    def trigger(self, bpm):
        self._events.put(float(np.clip(bpm, BPM_MIN, BPM_MAX)))

    def next_chunk(self):
        while True:
            try:
                bpm = self._events.get_nowait()
            except queue.Empty:
                break
            beat = make_beat(bpm, self.rng)
            if len(self._buf) < len(beat):
                self._buf = np.concatenate([self._buf, np.zeros(len(beat) - len(self._buf))])
            self._buf[: len(beat)] += beat

        out = np.zeros(self.CHUNK)
        n = min(self.CHUNK, len(self._buf))
        out[:n] = self._buf[:n]
        self._buf = self._buf[n:]
        return soft_limit(out * self.volume)


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


def serial_reader(ser, stream, stop):
    while not stop.is_set():
        try:
            line = ser.readline().decode(errors="ignore").strip()
        except Exception as e:
            print(f"シリアル読み込みエラー: {e}")
            stop.set()
            break
        if not line:
            continue
        parts = line.split()
        if parts[0] == "BEAT":
            try:
                stream.trigger(float(parts[1]))
            except (IndexError, ValueError):
                stream.trigger(60.0)
        else:
            print(line)


def serial_input_loop(ser, stop):
    print("BPM を入力して Enter で Arduino の BPM を変更, s で停止, g で再開, q で終了")
    for line in sys.stdin:
        s = line.strip()
        if s in ("q", "quit", "exit"):
            break
        if s in ("s", "g"):
            ser.write(f"{s}\n".encode())
            continue
        try:
            ser.write(f"{float(s):.1f}\n".encode())
        except ValueError:
            print("数値, s, g, q のいずれかを入力してください")
    stop.set()


def run_serial(args):
    import serial

    ser = serial.Serial(args.port, args.baud, timeout=0.2)
    if args.bpm is not None:
        time.sleep(2.0)
        ser.write(f"{args.bpm:.1f}\n".encode())

    stream = TriggeredHeartbeat(args.volume)
    player = open_player()
    stop = threading.Event()
    threading.Thread(target=serial_reader, args=(ser, stream, stop), daemon=True).start()
    threading.Thread(target=serial_input_loop, args=(ser, stop), daemon=True).start()
    try:
        while not stop.is_set():
            player.stdin.write(to_pcm16(stream.next_chunk()))
            player.stdin.flush()
    except (KeyboardInterrupt, BrokenPipeError):
        pass
    finally:
        stop.set()
        ser.close()
        try:
            player.stdin.close()
        except BrokenPipeError:
            pass
        try:
            player.wait(timeout=2)
        except subprocess.TimeoutExpired:
            player.terminate()


def main():
    p = argparse.ArgumentParser(description="Arduino の拍に同期して心音を鳴らす")
    p.add_argument("bpm", type=float, nargs="?", default=None,
                   help="心拍数 [BPM]. 同期時は起動時に Arduino へ送信 (省略時は送らない), --standalone/--wav 時は default 60")
    p.add_argument("--port", default="/dev/ttyUSB0", help="Arduino のシリアルポート (default /dev/ttyUSB0)")
    p.add_argument("--baud", type=int, default=115200, help="ボーレート (default 115200)")
    p.add_argument("--standalone", action="store_true", help="Arduino なしで指定 BPM で鳴らし続ける")
    p.add_argument("--wav", help="再生せずに WAV ファイルへ書き出す")
    p.add_argument("--seconds", type=float, default=10.0, help="--wav 時の長さ [s]")
    p.add_argument("--volume", type=float, default=DEFAULT_VOLUME,
                   help=f"音量ゲイン (default {DEFAULT_VOLUME})")
    args = p.parse_args()

    if not (args.standalone or args.wav):
        run_serial(args)
        return
    if args.bpm is None:
        args.bpm = 60.0

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
            player.wait(timeout=2)
        except subprocess.TimeoutExpired:
            player.terminate()


if __name__ == "__main__":
    main()
