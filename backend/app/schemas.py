from __future__ import annotations

from pydantic import BaseModel, Field


class HealthResponse(BaseModel):
    ok: bool
    engine_exists: bool
    engine_path: str
    stockfish_exists: bool = False
    stockfish_path: str = ""


class BestMoveRequest(BaseModel):
    moves: list[str] = Field(default_factory=list)
    fen: str | None = None
    depth: int = Field(default=1, ge=1, le=8)
    movetime_ms: int | None = Field(default=None, ge=50, le=10_000)
    engine: str = Field(default="alpha-beta", pattern="^(alpha-beta|stockfish)$")
    chess960: bool = False
    stockfish_skill: int = Field(default=10, ge=0, le=20)


class BestMoveResponse(BaseModel):
    bestmove: str
    ok: bool
    log: list[str] = Field(default_factory=list)
    score_type: str = "cp"  # <-- Обязательно со знаком "="
    score_value: int = 0    # <-- Обязательно со знаком "="

class ThemeResponse(BaseModel):
    piece_themes: list[str]
    boards: list[str]
