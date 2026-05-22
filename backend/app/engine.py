from __future__ import annotations

import os
import queue
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[2]


def default_engine_path() -> Path:
    candidates = [
        PROJECT_ROOT / "engine" / "bin" / "chessviz.exe",
        PROJECT_ROOT / "engine" / "bin" / "chessviz",
        PROJECT_ROOT / "engine" / "chessviz.exe",
        PROJECT_ROOT / "engine" / "chessviz",
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[0] if sys.platform.startswith("win") else candidates[1]


def default_stockfish_path() -> Path | None:
    names = ["stockfish.exe", "stockfish"] if sys.platform.startswith("win") else ["stockfish", "stockfish.exe"]
    candidates = [
        PROJECT_ROOT / "tools" / "stockfish" / name
        for name in names
    ] + [
        PROJECT_ROOT / "stockfish" / name
        for name in names
    ]

    for candidate in candidates:
        if candidate.exists():
            return candidate

    for name in names:
        found = shutil.which(name)
        if found:
            return Path(found)

    return None


class UciProcess:
    def __init__(self, engine_path: Path | None = None, chess960: bool = False) -> None:
        self.engine_path = engine_path or default_engine_path()
        self.chess960 = chess960
        self.process: subprocess.Popen[str] | None = None
        self.lines: queue.Queue[str] = queue.Queue()
        self.log: list[str] = []
        self._reader: threading.Thread | None = None

    def start(self) -> None:
        if not self.engine_path.exists():
            raise FileNotFoundError(f"Engine executable not found: {self.engine_path}")

        self.process = subprocess.Popen(
            [str(self.engine_path)],
            cwd=str(PROJECT_ROOT),
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            creationflags=subprocess.CREATE_NO_WINDOW if sys.platform.startswith("win") else 0,
        )
        self._reader = threading.Thread(target=self._read_stdout, daemon=True)
        self._reader.start()

    def close(self) -> None:
        process = self.process
        if process is None:
            return
        try:
            if process.poll() is None:
                self.send("quit")
                process.wait(timeout=1)
        except Exception:
            process.kill()

    def send(self, command: str) -> None:
        process = self.process
        if process is None or process.stdin is None or process.poll() is not None:
            raise RuntimeError("Engine process is not running")
        process.stdin.write(command.strip() + "\n")
        process.stdin.flush()

    def wait_for(self, prefix: str, timeout: float = 3.0) -> str | None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                line = self.lines.get(timeout=0.05)
            except queue.Empty:
                continue
            if line.startswith(prefix):
                return line
        return None

    def bestmove(
        self,
        moves: list[str],
        fen: str | None = None,
        depth: int = 1,
        movetime_ms: int | None = None,
        stockfish_skill: int | None = None,
    ) -> str:
        self.start()
        try:
            self.send("uci")
            if self.wait_for("uciok") is None:
                raise TimeoutError("Engine did not answer uciok")

            if self.chess960:
                self.send("setoption name UCI_Chess960 value true")
            if stockfish_skill is not None:
                self.send(f"setoption name Skill Level value {stockfish_skill}")

            self.send("isready")
            if self.wait_for("readyok") is None:
                raise TimeoutError("Engine did not answer readyok")

            if fen:
                position = f"position fen {fen}"
                if moves:
                    position += " moves " + " ".join(moves)
            else:
                position = "position startpos"
                if moves:
                    position += " moves " + " ".join(moves)
            self.send(position)

            if movetime_ms is not None:
                self.send(f"go movetime {movetime_ms}")
            else:
                self.send(f"go depth {depth}")

            line = self.wait_for("bestmove", timeout=max(3.0, (movetime_ms or 0) / 1000 + 2))
            if line is None:
                raise TimeoutError("Engine did not answer bestmove")
            
            # === ФИКС ЗДЕСЬ: Даем фоновому потоку 10 миллисекунд скопировать последние строки info в log ===
            time.sleep(0.01) 
            
            parts = line.split()
            return parts[1] if len(parts) > 1 else "0000"
        finally:
            self.close()

    def _read_stdout(self) -> None:
        process = self.process
        if process is None or process.stdout is None:
            return
        for raw in process.stdout:
            line = raw.strip()
            if not line:
                continue
            self.log.append(line)
            self.lines.put(line)
