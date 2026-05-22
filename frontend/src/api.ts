export type HealthResponse = {
  ok: boolean;
  engine_exists: boolean;
  engine_path: string;
  stockfish_exists: boolean;
  stockfish_path: string;
};

export type ThemeResponse = {
  piece_themes: string[];
  boards: string[];
};

export type BestMoveResponse = {
  bestmove: string;
  ok: boolean;
  log: string[];
  score_type: "cp" | "mate";
  score_value: number;
};

export async function getHealth(): Promise<HealthResponse> {
  const response = await fetch("/api/health");
  if (!response.ok) throw new Error("Не удалось проверить backend");
  return response.json();
}

export async function getThemes(): Promise<ThemeResponse> {
  const response = await fetch("/api/themes");
  if (!response.ok) throw new Error("Не удалось загрузить темы");
  return response.json();
}

export type EngineChoice = "alpha-beta" | "stockfish";

export async function requestBestMove(
  moves: string[],
  fen?: string,
  depth = 1,
  engine: EngineChoice = "alpha-beta",
  chess960 = false,
  stockfishSkill = 10
): Promise<BestMoveResponse> {
  const response = await fetch("/api/engine/bestmove", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ moves, fen, depth, engine, chess960, stockfish_skill: stockfishSkill })
  });
  if (!response.ok) {
    const detail = await response.json().catch(() => null);
    throw new Error(detail?.detail?.message ?? "Движок не ответил");
  }
  return response.json();
}
