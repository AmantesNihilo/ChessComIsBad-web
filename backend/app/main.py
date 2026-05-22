from __future__ import annotations

from pathlib import Path

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles

from .engine import PROJECT_ROOT, UciProcess, default_engine_path, default_stockfish_path
from .schemas import BestMoveRequest, BestMoveResponse, HealthResponse, ThemeResponse


ASSETS_ROOT = PROJECT_ROOT / "assets"

app = FastAPI(title="TRPO Chess API", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173", "http://127.0.0.1:5173"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

if ASSETS_ROOT.exists():
    app.mount("/assets", StaticFiles(directory=str(ASSETS_ROOT)), name="assets")


@app.get("/api/health", response_model=HealthResponse)
def health() -> HealthResponse:
    engine_path = default_engine_path()
    stockfish_path = default_stockfish_path()
    return HealthResponse(
        ok=True,
        engine_exists=engine_path.exists(),
        engine_path=str(engine_path),
        stockfish_exists=stockfish_path is not None and stockfish_path.exists(),
        stockfish_path=str(stockfish_path) if stockfish_path else "",
    )


@app.get("/api/themes", response_model=ThemeResponse)
def themes() -> ThemeResponse:
    piece_root = ASSETS_ROOT / "themes"
    board_root = ASSETS_ROOT / "boards"
    piece_themes = sorted(path.name for path in piece_root.iterdir() if (path / "wK.svg").exists()) if piece_root.exists() else []
    boards = (
        sorted(path.name for path in board_root.iterdir() if path.suffix.lower() in {".jpg", ".jpeg", ".png", ".webp"})
        if board_root.exists()
        else []
    )
    return ThemeResponse(piece_themes=piece_themes, boards=boards)


import re
SCORE_CP_REGEX = re.compile(r"score\s+cp\s+(-?\d+)")
SCORE_MATE_REGEX = re.compile(r"score\s+mate\s+(-?\d+)")
@app.post("/api/engine/bestmove", response_model=BestMoveResponse)
def bestmove(payload: BestMoveRequest) -> BestMoveResponse:
    if payload.engine == "stockfish":
        stockfish_path = default_stockfish_path()
        if stockfish_path is None or not stockfish_path.exists():
            raise HTTPException(
                status_code=500,
                detail={
                    "message": "Stockfish was not found. Put stockfish.exe into tools/stockfish or add it to PATH.",
                    "log": [],
                },
            )
        session = UciProcess(stockfish_path, chess960=payload.chess960)
    else:
        if payload.chess960:
            raise HTTPException(
                status_code=400,
                detail={"message": "Alpha-beta engine does not support Chess960", "log": []},
            )
        session = UciProcess()
    try:
        move = session.bestmove(
            moves=payload.moves,
            fen=payload.fen,
            depth=payload.depth,
            movetime_ms=payload.movetime_ms,
            stockfish_skill=payload.stockfish_skill if payload.engine == "stockfish" else None,
        )
        
        print("\n=== [BACKEND DEBUG] ВСЕ ЛОГИ ДВИЖКА ПЕРЕД ПАРСИНГОМ ===")
        for line in session.log:
            print(f" >  {line}")
        print("====================================================\n")
        
        score_type = "cp"
        score_value = 0

        # 1. Сначала парсим логи с конца, как у тебя и было
        for line in reversed(session.log):
            if "info" in line:
                match_cp = SCORE_CP_REGEX.search(line)
                if match_cp:
                    score_type = "cp"
                    score_value = int(match_cp.group(1))
                    break

                match_mate = SCORE_MATE_REGEX.search(line)
                if match_mate:
                    score_type = "mate"
                    score_value = int(match_mate.group(1))
                    break

        # === 2. А ВОТ СЮДА ВСТАВЛЯЕМ ФИКС ЗНАКА ===
        # Проверяем, чей сейчас ход. 
        # Если в запросе пришел FEN — смотрим на активного игрока в FEN.
        # Если FEN нет, ориентируемся по количеству сделанных ходов (нечетное количество = ход черных).
        if payload.fen:
            is_black_turn = " b " in payload.fen
        else:
            is_black_turn = len(payload.moves) % 2 != 0 if payload.moves else False

        # Если ход черных, переворачиваем знак, чтобы плюс ВСЕГДА был за белых
        if is_black_turn:
            score_value = -score_value
        # =========================================

        return BestMoveResponse(
            bestmove=move, 
            ok=move != "0000", 
            log=session.log,
            score_type=score_type,  
            score_value=score_value  
        )
    except Exception as exc:
        raise HTTPException(
            status_code=500,
            detail={"message": str(exc), "log": session.log},
        ) from exc
