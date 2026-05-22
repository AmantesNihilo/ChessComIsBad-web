import { Chess, Move, Square } from "chess.js";
import { AnimatePresence, motion } from "framer-motion";
import {
  ArrowLeft,
  Bot,
  BrainCircuit,
  CheckCircle2,
  Clock3,
  Crown,
  History,
  MonitorCog,
  RotateCcw,
  Settings,
  ShieldAlert,
  Shuffle,
  Sparkles,
  Swords,
  Undo2
} from "lucide-react";
import { useEffect, useMemo, useState } from "react";
import { Chessboard } from "react-chessboard";
import { EngineChoice, getHealth, getThemes, HealthResponse, requestBestMove } from "./api";
import { Chess960Setup, chess960Fen, chess960Setup } from "./chess960";
import { Badge } from "./components/ui/badge";
import { Button } from "./components/ui/button";
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from "./components/ui/card";
import { ScrollArea } from "./components/ui/scroll-area";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "./components/ui/select";
import { Switch } from "./components/ui/switch";
import { cn } from "./lib/utils";
import { AppSettings, GameMode, MoveRecord, TimerPreset } from "./types";

const defaultSettings: AppSettings = {
  pieceTheme: "cburnett",
  boardTexture: "maple2.jpg",
  highlightMoves: true,
  chess960Index: 690,
  soundless: true
};

const pieceNames = ["wK", "wQ", "wR", "wB", "wN", "wP", "bK", "bQ", "bR", "bB", "bN", "bP"];
const AI_PREVIEW_PLIES = 3;
const timerOptions: Array<{ value: TimerPreset; label: string }> = [
  { value: "none", label: "Без таймера" },
  { value: "3", label: "3 минуты" },
  { value: "5", label: "5 минут" },
  { value: "10", label: "10 минут" },
  { value: "15", label: "15 минут" },
  { value: "custom", label: "Свое время" }
];
const materialValues: Record<string, number> = {
  p: 1,
  n: 3,
  b: 3,
  r: 5,
  q: 9,
  k: 0
};

const screenMotion = {
  initial: { opacity: 0, y: 12 },
  animate: { opacity: 1, y: 0 },
  exit: { opacity: 0, y: -8 },
  transition: { duration: 0.2, ease: "easeOut" }
};

function getInitialFen(mode: GameMode, chess960Index: number): string {
  return mode === "690" ? chess960Fen(chess960Index) : new Chess().fen();
}

function createGame(mode: GameMode, chess960Index: number): Chess {
  return new Chess(getInitialFen(mode, chess960Index));
}

function uciFromMove(move: Move): string {
  return `${move.from}${move.to}${move.promotion ?? ""}`;
}

function statusText(game: Chess, mode: GameMode, aiThinking: boolean): string {
  if (aiThinking) return "AI думает";
  if (game.isCheckmate()) return `Мат. Победили ${game.turn() === "w" ? "черные" : "белые"}`;
  if (game.isStalemate()) return "Пат";
  if (game.isDraw()) return "Ничья";
  if (game.isGameOver()) return "Партия завершена";
  const side = game.turn() === "w" ? "Белые" : "Черные";
  return `${side}${game.isCheck() ? ", шах" : ""}${mode === "690" ? " - Chess960" : ""}`;
}

function modeTitle(mode: GameMode): string {
  if (mode === "1vAI") return "Игрок против C-движка";
  if (mode === "690") return "Chess960";
  return "Два игрока";
}

function moveSide(color: "w" | "b"): string {
  return color === "w" ? "Белые" : "Черные";
}

function sideText(color: "w" | "b"): string {
  return color === "w" ? "белые" : "черные";
}

type TimerConfig = {
  timerPreset: TimerPreset;
  customMinutes: number;
  incrementSeconds: number;
  aiDelayMs: number;
  engine: EngineChoice;
  stockfishSkill: number;
};

function timerSeconds(config: TimerConfig): number | null {
  if (config.timerPreset === "none") return null;
  const minutes = config.timerPreset === "custom" ? config.customMinutes : Number(config.timerPreset);
  return Math.max(1, Math.min(180, minutes)) * 60;
}

function formatClock(seconds: number): string {
  const safe = Math.max(0, seconds);
  const minutes = Math.floor(safe / 60);
  const rest = safe % 60;
  return `${minutes}:${String(rest).padStart(2, "0")}`;
}

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => window.setTimeout(resolve, ms));
}

function applyUciMove(game: Chess, uci: string): Move | null {
  try {
    return game.move({
      from: uci.slice(0, 2) as Square,
      to: uci.slice(2, 4) as Square,
      promotion: uci[4] ?? "q"
    });
  } catch {
    return null;
  }
}

type CastleSide = "king" | "queen";

function squareName(file: number, rank: 1 | 8): Square {
  return `${String.fromCharCode(97 + file)}${rank}` as Square;
}

function opposite(color: "w" | "b"): "w" | "b" {
  return color === "w" ? "b" : "w";
}

function castleMeta(setup: Chess960Setup, color: "w" | "b", side: CastleSide) {
  const rank = color === "w" ? 1 : 8;
  const kingToFile = side === "king" ? 6 : 2;
  const rookToFile = side === "king" ? 5 : 3;
  const rookFromFile = side === "king" ? setup.kingSideRookFile : setup.queenSideRookFile;

  return {
    rank: rank as 1 | 8,
    kingFrom: squareName(setup.kingFile, rank as 1 | 8),
    kingTo: squareName(kingToFile, rank as 1 | 8),
    rookFrom: squareName(rookFromFile, rank as 1 | 8),
    rookTo: squareName(rookToFile, rank as 1 | 8),
    kingToFile,
    rookToFile,
    rookFromFile
  };
}

function hasChess960CastleRight(records: MoveRecord[], setup: Chess960Setup, color: "w" | "b", side: CastleSide): boolean {
  const meta = castleMeta(setup, color, side);
  return !records.some((record) => {
    if (record.color !== color) return false;
    return record.uci.startsWith(meta.kingFrom) || record.uci.startsWith(meta.rookFrom);
  });
}

function canChess960Castle(game: Chess, setup: Chess960Setup, records: MoveRecord[], side: CastleSide): boolean {
  const color = game.turn() as "w" | "b";
  const meta = castleMeta(setup, color, side);
  const king = game.get(meta.kingFrom);
  const rook = game.get(meta.rookFrom);

  if (game.isCheck()) return false;
  if (!hasChess960CastleRight(records, setup, color, side)) return false;
  if (!king || king.type !== "k" || king.color !== color) return false;
  if (!rook || rook.type !== "r" || rook.color !== color) return false;

  const betweenStart = Math.min(setup.kingFile, meta.rookFromFile) + 1;
  const betweenEnd = Math.max(setup.kingFile, meta.rookFromFile) - 1;
  for (let file = betweenStart; file <= betweenEnd; file += 1) {
    const square = squareName(file, meta.rank);
    if (square !== meta.kingFrom && square !== meta.rookFrom && game.get(square)) return false;
  }

  for (const file of [meta.kingToFile, meta.rookToFile]) {
    const square = squareName(file, meta.rank);
    if (square !== meta.kingFrom && square !== meta.rookFrom && game.get(square)) return false;
  }

  const step = meta.kingToFile > setup.kingFile ? 1 : -1;
  for (let file = setup.kingFile; file !== meta.kingToFile + step; file += step) {
    if (game.isAttacked(squareName(file, meta.rank), opposite(color))) return false;
  }

  return true;
}

function applyChess960Castle(game: Chess, setup: Chess960Setup, records: MoveRecord[], side: CastleSide): MoveRecord | null {
  if (!canChess960Castle(game, setup, records, side)) return null;

  const color = game.turn() as "w" | "b";
  const meta = castleMeta(setup, color, side);
  game.remove(meta.kingFrom);
  game.remove(meta.rookFrom);
  game.put({ type: "k", color }, meta.kingTo);
  game.put({ type: "r", color }, meta.rookTo);
  game.setTurn(opposite(color));

  return {
    san: side === "king" ? "O-O" : "O-O-O",
    uci: `${meta.kingFrom}${meta.rookFrom}`,
    color,
    chess960Castle: side
  };
}

function hasAnyLegalMove(game: Chess, mode: GameMode, setup: Chess960Setup, records: MoveRecord[]): boolean {
  if (game.moves().length > 0) return true;
  if (mode !== "690") return false;
  return canChess960Castle(game, setup, records, "king") || canChess960Castle(game, setup, records, "queen");
}

function materialBalance(game: Chess) {
  let white = 0;
  let black = 0;

  for (const row of game.board()) {
    for (const piece of row) {
      if (!piece) continue;
      const value = materialValues[piece.type] ?? 0;
      if (piece.color === "w") {
        white += value;
      } else {
        black += value;
      }
    }
  }

  const diff = white - black;
  const markerPercent = Math.max(8, Math.min(92, 50 - (diff / 28) * 50));
  const fillTop = Math.min(50, markerPercent);
  const fillHeight = Math.abs(markerPercent - 50);
  const label = diff === 0 ? "0" : `${diff > 0 ? "+" : ""}${diff}`;
  return { white, black, diff, markerPercent, fillTop, fillHeight, label };
}

function MaterialBalanceBar({ balance }: { balance: ReturnType<typeof materialBalance> }) {
  const advantage = balance.diff > 0 ? "Белые" : balance.diff < 0 ? "Черные" : "Равно";
  const accent = balance.diff > 0 ? "bg-slate-100" : balance.diff < 0 ? "bg-slate-950" : "bg-emerald-400";
  const badgeClass =
    balance.diff > 0
      ? "border-slate-200/70 bg-slate-100 text-slate-950"
      : balance.diff < 0
        ? "border-slate-700 bg-slate-950 text-slate-100"
        : "border-emerald-400/60 bg-emerald-500/15 text-emerald-200";

  return (
    <div className="flex w-14 shrink-0 flex-col items-center gap-2">
      <span className="text-[10px] font-semibold tracking-wide text-muted-foreground">W</span>
      <div className="relative min-h-0 w-7 flex-1 rounded-full border bg-gradient-to-b from-slate-100/12 via-muted/20 to-slate-950/55 shadow-inner">
        <div className="absolute left-1/2 top-3 bottom-3 w-px -translate-x-1/2 bg-border/80" />
        <div className="absolute inset-x-1 top-1/2 h-px -translate-y-1/2 bg-emerald-400/70" />
        <div
          className={cn("absolute left-1/2 w-3 -translate-x-1/2 rounded-full opacity-80 transition-all duration-700 ease-out", accent)}
          style={{ top: `${balance.fillTop}%`, height: `${Math.max(2, balance.fillHeight)}%` }}
        />
        <div
          className={cn("absolute left-1/2 h-5 w-5 -translate-x-1/2 -translate-y-1/2 rounded-full border-2 border-background shadow-lg transition-all duration-700 ease-out", accent)}
          style={{ top: `${balance.markerPercent}%` }}
        />
      </div>
      <div className={cn("w-full rounded-md border px-1.5 py-1 text-center shadow-sm transition-colors duration-500", badgeClass)}>
        <p className="font-mono text-xs font-semibold">{balance.label}</p>
        <p className="truncate text-[9px] opacity-75">{advantage}</p>
      </div>
      <span className="text-[10px] font-semibold tracking-wide text-muted-foreground">B</span>
    </div>
  );
}

export default function App() {
  const [mode, setMode] = useState<GameMode>("1v1");
  const [screen, setScreen] = useState<"menu" | "game" | "settings">("menu");
  const [settingsReturnTo, setSettingsReturnTo] = useState<"menu" | "game">("menu");
  const [pendingMode, setPendingMode] = useState<GameMode | null>(null);
  const [timerConfig, setTimerConfig] = useState<TimerConfig>({
    timerPreset: "none",
    customMinutes: 10,
    incrementSeconds: 0,
    aiDelayMs: 600,
    engine: "alpha-beta",
    stockfishSkill: 10
  });
  const [settings, setSettings] = useState<AppSettings>(() => {
    const saved = localStorage.getItem("trpo-chess-settings");
    return saved ? { ...defaultSettings, ...JSON.parse(saved) } : defaultSettings;
  });
  const [activeChess960Setup, setActiveChess960Setup] = useState<Chess960Setup>(() => chess960Setup(settings.chess960Index));
  const [initialFen, setInitialFen] = useState(() => getInitialFen("1v1", settings.chess960Index));
  const [game, setGame] = useState(() => createGame("1v1", settings.chess960Index));
  const [fen, setFen] = useState(game.fen());
  const [selected, setSelected] = useState<Square | null>(null);
  const [moves, setMoves] = useState<MoveRecord[]>([]);
  const [legalSquares, setLegalSquares] = useState<Record<string, React.CSSProperties>>({});
  const [aiThinking, setAiThinking] = useState(false);
  const [evaluatingLine, setEvaluatingLine] = useState(false);
  const [bestMove, setBestMove] = useState("-");
  const [evalScore, setEvalScore] = useState<{ type: "cp" | "mate"; value: number }>({ type: "cp", value: 0 });
  const [isEvalLoading, setIsEvalLoading] = useState(false);
  const [predictionLine, setPredictionLine] = useState<string[]>([]);
  const [notice, setNotice] = useState<string | null>(null);
  const [health, setHealth] = useState<HealthResponse | null>(null);
  const [pieceThemes, setPieceThemes] = useState<string[]>(["cburnett"]);
  const [boards, setBoards] = useState<string[]>(["maple2.jpg", "blue2.jpg"]);
  const [timeLeft, setTimeLeft] = useState<{ w: number; b: number }>(() => {
    const seconds = timerSeconds(timerConfig) ?? 0;
    return { w: seconds, b: seconds };
  });
  const [timeWinner, setTimeWinner] = useState<"w" | "b" | null>(null);

  const updateEvaluation = async (currentFen: string) => {
    if (mode === "690" && timerConfig.engine !== "stockfish") {
      setEvalScore({ type: "cp", value: 0 });
      return;
    }
    setIsEvalLoading(true);
    try {
      const response = await requestBestMove([], currentFen, 3, timerConfig.engine, mode === "690", timerConfig.stockfishSkill);
      if (response && response.score_type) {
        setEvalScore({ 
          type: response.score_type as "cp" | "mate", 
          value: Number(response.score_value) 
        });
      }
    } catch (error) {
      console.error("Ошибка при получении оценки:", error);
    } finally {
      setIsEvalLoading(false);
    }
  };
    
  useEffect(() => {
    localStorage.setItem("trpo-chess-settings", JSON.stringify(settings));
  }, [settings]);

  useEffect(() => {
    getHealth().then(setHealth).catch((error) => setNotice(`Backend: ${error.message}`));
    getThemes()
      .then((data) => {
        if (data.piece_themes.length) setPieceThemes(data.piece_themes);
        if (data.boards.length) setBoards(data.boards);
      })
      .catch((error) => setNotice(`Темы: ${error.message}`));
  }, []);

  const customPieces = useMemo(() => {
    return Object.fromEntries(
      pieceNames.map((piece) => [
        piece,
        ({ squareWidth }: { squareWidth: number }) => (
          <img
            alt={piece}
            draggable={false}
            src={`/assets/themes/${settings.pieceTheme}/${piece}.svg`}
            style={{ width: squareWidth, height: squareWidth, padding: squareWidth * 0.08 }}
          />
        )
      ])
    );
  }, [settings.pieceTheme]);

  const boardStyle = useMemo<Record<string, string | number>>(
    () => ({
      borderRadius: 8,
      boxShadow: "0 28px 80px rgba(0,0,0,.34)",
      backgroundImage: `url(/assets/boards/${settings.boardTexture})`,
      backgroundSize: "cover"
    }),
    [settings.boardTexture]
  );

  const resetClocks = (nextTimerConfig = timerConfig) => {
    const seconds = timerSeconds(nextTimerConfig) ?? 0;
    setTimeLeft({ w: seconds, b: seconds });
    setTimeWinner(null);
  };

  const rebuildGame = (records: MoveRecord[], baseFen = initialFen): Chess => {
    const rebuilt = new Chess(baseFen);
    for (let index = 0; index < records.length; index += 1) {
      const record = records[index];
      if (mode === "690" && record.chess960Castle) {
        applyChess960Castle(rebuilt, activeChess960Setup, records.slice(0, index), record.chess960Castle);
      } else {
        applyUciMove(rebuilt, record.uci);
      }
    }
    return rebuilt;
  };

  const commitRecord = (nextGame: Chess, newMoveRecord: MoveRecord, baseMoves = moves): MoveRecord[] => {
      setGame(nextGame);
      setFen(nextGame.fen());
      setSelected(null);
      setLegalSquares({});
      
      const updatedMoves = [...baseMoves, newMoveRecord];
      setMoves(updatedMoves);
      if (timerSeconds(timerConfig) !== null && timerConfig.incrementSeconds > 0) {
        setTimeLeft((current) => ({
          ...current,
          [newMoveRecord.color]: current[newMoveRecord.color] + timerConfig.incrementSeconds
        }));
      }

      if ((mode !== "690" || timerConfig.engine === "stockfish") && !nextGame.isGameOver()) {
        void updateEvaluation(nextGame.fen());
      } else {
        setEvalScore({ type: "cp", value: 0 });
      }
      return updatedMoves;
    };

  const commitMove = (nextGame: Chess, move: Move, baseMoves = moves): MoveRecord[] => {
      return commitRecord(nextGame, { san: move.san, uci: uciFromMove(move), color: move.color }, baseMoves);
    };
  const evaluatePositionLine = async (currentFen: string, plies = AI_PREVIEW_PLIES) => {
    if (mode !== "1vAI" || plies < 1) {
      setPredictionLine([]);
      return;
    }

    setEvaluatingLine(true);
    try {
      const previewGame = new Chess(currentFen);
      const line: string[] = [];

      for (let index = 0; index < plies && !previewGame.isGameOver(); index += 1) {
      const response = await requestBestMove([], previewGame.fen(), 1, timerConfig.engine, false, timerConfig.stockfishSkill);
        if (response.bestmove === "0000") break;

        const move = applyUciMove(previewGame, response.bestmove);
        if (!move) break;
        line.push(`${moveSide(move.color)} ${move.san} (${response.bestmove})`);
      }

      setPredictionLine(line);
    } catch (error) {
      setPredictionLine([]);
      setNotice(`Прогноз: ${(error as Error).message}`);
    } finally {
      setEvaluatingLine(false);
    }
  };

  const requestAiMove = async (currentFen: string, baseMoves: MoveRecord[]) => {
    setAiThinking(true);
    setPredictionLine([]);
    try {
      if (timerConfig.aiDelayMs > 0) {
        await sleep(timerConfig.aiDelayMs);
      }
      const response = await requestBestMove([], currentFen, 1, timerConfig.engine, false, timerConfig.stockfishSkill);

      if (response.bestmove === "0000") {
        const snapshot = new Chess(currentFen);
        setBestMove(snapshot.isGameOver() ? "нет ходов" : "нет хода");
        return;
      }

      setBestMove(response.bestmove);
      const next = new Chess(currentFen);
      const move = applyUciMove(next, response.bestmove);

      if (move) {
        commitMove(next, move, baseMoves);
        if (!next.isGameOver()) {
          void evaluatePositionLine(next.fen()); 
        }
      } else {
        setNotice(`AI вернул нелегальный ход: ${response.bestmove}`);
      }
    } catch (error) {
      setNotice(`AI: ${(error as Error).message}`);
    } finally {
      setAiThinking(false);
    }
};

    const onDrop = (sourceSquare: Square, targetSquare: Square) => {
    // Блокируем ход, если комп думает, игра окончена или сейчас ход чёрного AI
    if (aiThinking || timeWinner || !hasAnyLegalMove(game, mode, activeChess960Setup, moves) || (mode === "1vAI" && game.turn() === "b")) return false;

    const kingCastle = castleMeta(activeChess960Setup, game.turn() as "w" | "b", "king");
    const queenCastle = castleMeta(activeChess960Setup, game.turn() as "w" | "b", "queen");
    const castleSide =
      mode === "690" && sourceSquare === kingCastle.kingFrom
        ? targetSquare === kingCastle.kingTo || targetSquare === kingCastle.rookFrom
          ? "king"
          : targetSquare === queenCastle.kingTo || targetSquare === queenCastle.rookFrom
            ? "queen"
            : null
        : null;

    if (castleSide) {
      const next = new Chess(game.fen());
      const record = applyChess960Castle(next, activeChess960Setup, moves, castleSide);
      if (!record) return false;
      commitRecord(next, record);
      return true;
    }

    const next = new Chess(game.fen());
    let move: Move | null = null;
    try {
      move = next.move({ from: sourceSquare, to: targetSquare, promotion: "q" });
    } catch {
      return false;
    }
    if (!move) return false;

    // commitMove сам обновит доску, историю и ТРИГГЕРНЕТ левую шкалу EVAL
    const updatedMoves = commitMove(next, move);

    // А вот эта часть отвечает за то, должен ли движок делать ответный ход на доске
    if (mode === "1vAI") {
      if (next.isGameOver()) {
        setBestMove("партия завершена");
        setPredictionLine([]);
      } else {
        // Формируем историю для логики ответного хода бота
        void requestAiMove(next.fen(), updatedMoves);
      }
    }
    return true;
  };

  const onSquareClick = (square: Square) => {
    if (aiThinking || timeWinner || !hasAnyLegalMove(game, mode, activeChess960Setup, moves) || (mode === "1vAI" && game.turn() === "b")) return;
    if (selected && onDrop(selected, square)) return;

    const piece = game.get(square);
    if (!piece || piece.color !== game.turn()) {
      setSelected(null);
      setLegalSquares({});
      return;
    }

    const styles: Record<string, React.CSSProperties> = {
      [square]: { background: "rgba(255,255,255,0.22)" }
    };
    for (const move of game.moves({ square, verbose: true })) {
      styles[move.to] = {
        background:
          game.get(move.to) !== null
            ? "radial-gradient(circle, transparent 58%, rgba(16,185,129,.85) 60%, rgba(16,185,129,.85) 68%, transparent 70%)"
            : "radial-gradient(circle, rgba(16,185,129,.9) 18%, transparent 20%)"
      };
    }
    if (mode === "690" && square === castleMeta(activeChess960Setup, game.turn() as "w" | "b", "king").kingFrom) {
      for (const side of ["king", "queen"] as const) {
        if (canChess960Castle(game, activeChess960Setup, moves, side)) {
          const meta = castleMeta(activeChess960Setup, game.turn() as "w" | "b", side);
          styles[meta.kingTo] = {
            background: "radial-gradient(circle, rgba(245,158,11,.9) 18%, transparent 20%)"
          };
          styles[meta.rookFrom] = {
            background: "radial-gradient(circle, rgba(245,158,11,.9) 18%, transparent 20%)"
          };
        }
      }
    }
    setSelected(square);
    setLegalSquares(styles);
  };

  const startGame = (nextMode: GameMode) => {
    const nextInitialFen = getInitialFen(nextMode, settings.chess960Index);
    const next = new Chess(nextInitialFen);
    setMode(nextMode);
    setActiveChess960Setup(chess960Setup(settings.chess960Index));
    setInitialFen(nextInitialFen);
    setGame(next);
    setFen(next.fen());
    
    // ОЧИЩАЕМ ВСЁ, чтобы старые партии не ломали логику ходов:
    setMoves([]); 
    setEvalScore({ type: "cp", value: 0 }); // Сбрасываем шкалу на 0.0
    setBestMove("-");
    setPredictionLine([]);
    setLegalSquares({});
    setSelected(null);
    setAiThinking(false);
    resetClocks();
    
    setScreen("game");
    setPendingMode(null);
  };

  const openTimerSetup = (nextMode: GameMode) => {
    setTimerConfig((current) => ({
      ...current,
      engine: nextMode === "690" ? "stockfish" : current.engine
    }));
    setPendingMode(nextMode);
  };

  const openSettings = (returnTo: "menu" | "game") => {
    setSettingsReturnTo(returnTo);
    setScreen("settings");
  };

  const updateTimerPreset = (value: TimerPreset) => {
    setTimerConfig({ ...timerConfig, timerPreset: value });
  };

  const updateCustomMinutes = (value: string) => {
    const customMinutes = Math.max(1, Math.min(180, Number(value) || 1));
    setTimerConfig({ ...timerConfig, customMinutes });
  };

  const updateIncrementSeconds = (value: string) => {
    const incrementSeconds = Math.max(0, Math.min(120, Number(value) || 0));
    setTimerConfig({ ...timerConfig, incrementSeconds });
  };

  const updateAiDelayMs = (value: string) => {
    const aiDelayMs = Math.max(0, Math.min(10_000, Math.round((Number(value) || 0) * 1000)));
    setTimerConfig({ ...timerConfig, aiDelayMs });
  };

  const updateEngineChoice = (engine: EngineChoice) => {
    if (pendingMode === "690" && engine !== "stockfish") return;
    setTimerConfig({ ...timerConfig, engine });
  };

  const updateStockfishSkill = (value: string) => {
    const stockfishSkill = Math.max(0, Math.min(20, Number(value) || 0));
    setTimerConfig({ ...timerConfig, stockfishSkill });
  };

  const updateChess960Index = (value: string) => {
    const chess960Index = Math.max(0, Math.min(959, Number(value) || 0));
    setSettings({ ...settings, chess960Index });
  };

  const randomizeChess960Index = () => {
    setSettings({ ...settings, chess960Index: Math.floor(Math.random() * 960) });
  };

  const undoMove = () => {
    if (aiThinking || moves.length === 0) return;
    const rollbackCount = mode === "1vAI" && moves.length >= 2 && game.turn() === "w" ? 2 : 1;
    const nextMoves = moves.slice(0, Math.max(0, moves.length - rollbackCount));
    const next = rebuildGame(nextMoves);
    setGame(next);
    setFen(next.fen());
    setMoves(nextMoves);
    setSelected(null);
    setLegalSquares({});
    setPredictionLine([]);
    setNotice(null);
    setBestMove("-");
    setTimeWinner(null);
    if ((mode !== "690" || timerConfig.engine === "stockfish") && !next.isGameOver()) {
      void updateEvaluation(next.fen());
    }
  };

  useEffect(() => {
    const seconds = timerSeconds(timerConfig);
    if (seconds === null || screen !== "game" || game.isGameOver() || timeWinner) return;

    const interval = window.setInterval(() => {
      const turn = game.turn();
      setTimeLeft((current) => {
        if (current[turn] <= 0) return current;
        const next = { ...current, [turn]: current[turn] - 1 };
        if (next[turn] <= 0) {
          const winner = turn === "w" ? "b" : "w";
          setTimeWinner(winner);
          setNotice(`Время вышло: победили ${sideText(winner)}.`);
        }
        return next;
      });
    }, 1000);

    return () => window.clearInterval(interval);
  }, [game, screen, timerConfig, timeWinner]);

  const engineReady = health?.engine_exists === true;
  const clockEnabled = timerSeconds(timerConfig) !== null;
  const currentTurn = game.turn();
  const noLegalMoves = !hasAnyLegalMove(game, mode, activeChess960Setup, moves);
  const displayStatus = timeWinner
    ? `Время вышло. Победили ${sideText(timeWinner)}.`
    : noLegalMoves
      ? game.isCheck()
        ? `Мат. Победили ${sideText(opposite(currentTurn))}.`
        : "Пат. Нет доступных ходов."
      : statusText(game, mode, aiThinking);
  const status = timeWinner ? `Время вышло. Победили ${sideText(timeWinner)}.` : statusText(game, mode, aiThinking);
  const passiveBestMove = bestMove === "-" || bestMove.startsWith("нет") || bestMove.startsWith("партия");

  return (
    <main className="min-h-screen overflow-x-hidden bg-background text-foreground subtle-grid">
      <AnimatePresence mode="wait">
        {screen === "menu" && (
          <motion.section key="menu" {...screenMotion} className="page-shell grid min-h-screen items-center gap-8 py-8 lg:grid-cols-[1fr_360px]">
            <div className="max-w-3xl">
              <Badge variant="secondary" className="gap-2">
                <Sparkles className="h-3.5 w-3.5" />
                TRPO Chess
              </Badge>
              <h1 className="mt-5 max-w-4xl text-5xl font-black leading-[1.04] tracking-tight md:text-7xl">
                TRPO Chess (C, React, Python)
              </h1>
              <p className="mt-5 max-w-2xl text-base leading-7 text-muted-foreground md:text-lg">
                Шахматное приложение с React-интерфейсом, FastAPI backend и C-движком через UCI.
              </p>
              <div className="mt-6 flex flex-wrap gap-2">
                <Badge variant={engineReady ? "default" : "destructive"} className="gap-1.5">
                  {engineReady ? <CheckCircle2 className="h-3.5 w-3.5" /> : <ShieldAlert className="h-3.5 w-3.5" />}
                  {health ? (engineReady ? "движок найден" : "движок не найден") : "проверка backend"}
                </Badge>
                <Badge variant="outline">React</Badge>
                <Badge variant="outline">FastAPI</Badge>
                <Badge variant="outline">C engine</Badge>
              </div>
            </div>

            <Card className="border-border/80 bg-card/95 shadow-panel backdrop-blur">
              <CardHeader className="pb-4">
                <CardTitle className="text-2xl">Новая партия</CardTitle>
                <CardDescription>Выбери режим и сразу играй.</CardDescription>
              </CardHeader>
              <CardContent className="grid gap-3">
                <Button size="lg" className="h-12 justify-start" onClick={() => openTimerSetup("1v1")}>
                  <Swords className="h-4 w-4" />
                  1v1 на одном компьютере
                </Button>
                <Button size="lg" variant="secondary" className="h-12 justify-start" onClick={() => openTimerSetup("1vAI")}>
                  <Bot className="h-4 w-4" />
                  Игрок против движка
                </Button>
                <Button size="lg" variant="secondary" className="h-12 justify-start" onClick={() => openTimerSetup("690")}>
                  <Crown className="h-4 w-4" />
                  Chess960 #{settings.chess960Index}
                </Button>
                <Button size="lg" variant="outline" className="h-12 justify-start" onClick={() => openSettings("menu")}>
                  <Settings className="h-4 w-4" />
                  Настройки
                </Button>
                {false && pendingMode && (
                  <div className="mt-2 grid gap-3 rounded-md border bg-muted/25 p-3">
                    <div className="flex items-center justify-between gap-3">
                      <p className="flex items-center gap-2 text-sm font-semibold">
                        <Clock3 className="h-4 w-4" />
                        Таймер перед игрой
                      </p>
                      <Badge variant="outline">{pendingMode === "690" ? `Chess960 #${settings.chess960Index}` : modeTitle(pendingMode as GameMode)}</Badge>
                    </div>
                    <Select value={timerConfig.timerPreset} onValueChange={(value) => updateTimerPreset(value as TimerPreset)}>
                      <SelectTrigger className="h-10">
                        <SelectValue />
                      </SelectTrigger>
                      <SelectContent>
                        {timerOptions.map((option) => (
                          <SelectItem key={option.value} value={option.value}>
                            {option.label}
                          </SelectItem>
                        ))}
                      </SelectContent>
                    </Select>
                    {timerConfig.timerPreset === "custom" && (
                      <input
                        className="h-10 w-full rounded-md border border-input bg-transparent px-3 text-sm shadow-sm outline-none focus:ring-1 focus:ring-ring"
                        min={1}
                        max={180}
                        type="number"
                        value={timerConfig.customMinutes}
                        onChange={(event) => updateCustomMinutes(event.target.value)}
                      />
                    )}
                    <div className="grid grid-cols-2 gap-2">
                      <Button variant="outline" onClick={() => setPendingMode(null)}>
                        Отмена
                      </Button>
                      <Button onClick={() => startGame(pendingMode as GameMode)}>
                        Старт
                      </Button>
                    </div>
                  </div>
                )}
              </CardContent>
            </Card>
          </motion.section>
        )}

        {screen === "settings" && (
          <motion.section key="settings" {...screenMotion} className="page-shell py-6 md:py-8">
            <header className="mb-6 flex items-center justify-between gap-4">
              <div>
                <Badge variant="secondary" className="mb-3 gap-2">
                  <MonitorCog className="h-3.5 w-3.5" />
                  Настройки
                </Badge>
                <h2 className="text-3xl font-bold tracking-tight md:text-4xl">Внешний вид и анализ</h2>
              </div>
              <Button variant="outline" onClick={() => setScreen(settingsReturnTo)}>
                <ArrowLeft className="h-4 w-4" />
                Назад
              </Button>
            </header>

            <div className="grid gap-5 lg:grid-cols-[minmax(360px,560px)_1fr]">
              <Card className="h-fit bg-card/95 shadow-panel">
                <CardHeader className="pb-4">
                  <CardTitle>Параметры</CardTitle>
                  <CardDescription>Фигуры, доска и глубина прогноза применяются без перезапуска партии.</CardDescription>
                </CardHeader>
                <CardContent className="space-y-5">
                  <div className="space-y-2">
                    <label className="text-sm font-medium" htmlFor="piece-theme">
                      Набор фигур
                    </label>
                    <Select value={settings.pieceTheme} onValueChange={(value) => setSettings({ ...settings, pieceTheme: value })}>
                      <SelectTrigger id="piece-theme" className="h-10">
                        <SelectValue placeholder="Набор фигур" />
                      </SelectTrigger>
                      <SelectContent>
                        {pieceThemes.map((theme) => (
                          <SelectItem key={theme} value={theme}>
                            {theme}
                          </SelectItem>
                        ))}
                      </SelectContent>
                    </Select>
                  </div>

                  <div className="space-y-2">
                    <label className="text-sm font-medium" htmlFor="board-theme">
                      Текстура доски
                    </label>
                    <Select value={settings.boardTexture} onValueChange={(value) => setSettings({ ...settings, boardTexture: value })}>
                      <SelectTrigger id="board-theme" className="h-10">
                        <SelectValue placeholder="Текстура доски" />
                      </SelectTrigger>
                      <SelectContent>
                        {boards.map((board) => (
                          <SelectItem key={board} value={board}>
                            {board}
                          </SelectItem>
                        ))}
                      </SelectContent>
                    </Select>
                  </div>

                  <div className="grid gap-3 sm:grid-cols-[1fr_auto]">
                    <div className="space-y-2">
                      <label className="text-sm font-medium" htmlFor="chess960-index">Chess960 позиция</label>
                      <input
                        id="chess960-index"
                        className="h-10 w-full rounded-md border border-input bg-transparent px-3 text-sm shadow-sm outline-none focus:ring-1 focus:ring-ring"
                        min={0}
                        max={959}
                        type="number"
                        value={settings.chess960Index}
                        onChange={(event) => updateChess960Index(event.target.value)}
                      />
                    </div>
                    <Button type="button" variant="outline" className="mt-7" onClick={randomizeChess960Index}>
                      <Shuffle className="h-4 w-4" />
                      Случайно
                    </Button>
                  </div>

                  <div className="flex items-center justify-between gap-4 rounded-md border bg-muted/25 p-4">
                    <div className="min-w-0">
                      <p className="text-sm font-medium">Подсветка ходов</p>
                      <p className="mt-1 text-sm leading-5 text-muted-foreground">Показывать легальные клетки при выборе фигуры.</p>
                    </div>
                    <Switch checked={settings.highlightMoves} onCheckedChange={(value) => setSettings({ ...settings, highlightMoves: value })} />
                  </div>
                </CardContent>
              </Card>

              <div className="board-shell flex min-h-[420px] items-center justify-center rounded-lg border p-4 shadow-board">
                <div className="w-full max-w-[480px]">
                  <Chessboard
                    position={fen}
                    customPieces={customPieces}
                    customBoardStyle={boardStyle}
                    customDarkSquareStyle={{ backgroundColor: "transparent" }}
                    customLightSquareStyle={{ backgroundColor: "transparent" }}
                    arePiecesDraggable={false}
                    animationDuration={120}
                  />
                </div>
              </div>
            </div>
          </motion.section>
        )}

        {screen === "game" && (
          <motion.section key="game" {...screenMotion} className="page-shell grid min-h-screen gap-4 py-4 xl:grid-cols-[minmax(520px,1fr)_380px]">
            <div className="board-shell flex min-h-[calc(100vh-32px)] items-center justify-center rounded-lg border p-3 shadow-board">
              <div className="flex w-full max-w-[min(calc(100vh-72px),840px)] items-stretch gap-3">
                
                {/* ЛЕВАЯ ШКАЛА ОЦЕНКИ ДВИЖКА */}
                {mode === "690" && timerConfig.engine !== "stockfish" ? <MaterialBalanceBar balance={materialBalance(game)} /> : <EngineEvaluationBar evalScore={evalScore} isSearching={isEvalLoading} />}

                <div className="min-w-0 flex-1">
                  <Chessboard
                    position={fen}
                    onPieceDrop={onDrop}
                    onSquareClick={onSquareClick}
                    customPieces={customPieces}
                    customSquareStyles={settings.highlightMoves ? legalSquares : {}}
                    customBoardStyle={boardStyle}
                    customDarkSquareStyle={{ backgroundColor: "transparent" }}
                    customLightSquareStyle={{ backgroundColor: "transparent" }}
                    arePiecesDraggable={!aiThinking && !timeWinner && hasAnyLegalMove(game, mode, activeChess960Setup, moves) && !(mode === "1vAI" && game.turn() === "b")}
                    animationDuration={180}
                  />
                </div>
              </div>
            </div>

            <Card className="flex max-h-none flex-col bg-card/95 shadow-panel xl:h-[calc(100vh-32px)] xl:overflow-hidden">
              <CardHeader className="space-y-4 pb-4">
                <div className="flex items-start justify-between gap-3">
                  <div className="min-w-0">
                    <Badge variant="secondary" className="mb-3">
                      {mode === "690" ? `Chess960 #${settings.chess960Index}` : modeTitle(mode)}
                    </Badge>
                    <CardTitle className="text-2xl leading-tight">{displayStatus}</CardTitle>
                  </div>
                  <Button variant="outline" size="icon" onClick={() => setScreen("menu")} title="Меню">
                    <ArrowLeft className="h-4 w-4" />
                  </Button>
                </div>

                <div className="grid grid-cols-2 gap-3">
                  <div className={cn("rounded-md border p-3 transition-colors", currentTurn === "w" ? "border-slate-100 bg-slate-100 text-slate-950" : "bg-muted/20")}>
                    <p className="text-xs font-medium opacity-70">Белые</p>
                    <p className="mt-1 flex items-center gap-2 text-lg font-semibold">
                      {currentTurn === "w" && <span className="h-2 w-2 rounded-full bg-emerald-400" />}
                      {clockEnabled ? formatClock(timeLeft.w) : "ходят"}
                    </p>
                  </div>
                  <div className={cn("rounded-md border p-3 transition-colors", currentTurn === "b" ? "border-slate-700 bg-slate-950 text-slate-100" : "bg-muted/20")}>
                    <p className="text-xs font-medium opacity-70">Черные</p>
                    <p className="mt-1 flex items-center gap-2 text-lg font-semibold">
                      {currentTurn === "b" && <span className="h-2 w-2 rounded-full bg-emerald-400" />}
                      {clockEnabled ? formatClock(timeLeft.b) : "ходят"}
                    </p>
                  </div>
                </div>

                <div className="grid grid-cols-2 gap-3">
                  <div className="rounded-md border bg-muted/25 p-4">
                    <p className="flex items-center gap-2 text-sm text-muted-foreground">
                      <Bot className="h-4 w-4" />
                      Ход AI
                    </p>
                    <p className={cn("mt-2 truncate font-mono text-lg", passiveBestMove ? "text-muted-foreground" : "text-amber-300")}>
                      {bestMove}
                    </p>
                  </div>
                  <div className="rounded-md border bg-muted/25 p-4">
                    <p className="flex items-center gap-2 text-sm text-muted-foreground">
                      <CheckCircle2 className="h-4 w-4" />
                      Backend
                    </p>
                    <p className="mt-2 text-sm font-medium">{engineReady ? "готов" : "нет связи"}</p>
                  </div>
                </div>

                <div className="grid grid-cols-3 gap-2">
                  <Button variant="secondary" onClick={() => { setScreen("menu"); openTimerSetup(mode); }}>
                    <RotateCcw className="h-4 w-4" />
                    Сначала
                  </Button>
                  <Button variant="outline" onClick={undoMove} disabled={aiThinking || moves.length === 0}>
                    <Undo2 className="h-4 w-4" />
                    Назад
                  </Button>
                  <Button variant="outline" onClick={() => openSettings("game")}>
                    <Settings className="h-4 w-4" />
                    Вид
                  </Button>
                </div>

                {mode === "1vAI" && (
                  <div className="rounded-md border bg-muted/20 p-3">
                    <div className="min-w-0">
                      <p className="flex items-center gap-2 text-sm font-medium">
                        <BrainCircuit className="h-4 w-4" />
                        Прогноз позиции
                      </p>
                      <p className="mt-1 text-xs text-muted-foreground">Серия предполагаемых ходов от текущей позиции.</p>
                    </div>
                  </div>
                )}
              </CardHeader>

              <CardContent className="flex min-h-0 flex-1 flex-col gap-4 overflow-visible xl:overflow-hidden">
                {mode === "1vAI" && (
                  <section className="shrink-0">
                    <h3 className="mb-2 flex items-center gap-2 text-sm font-semibold">
                      <BrainCircuit className="h-4 w-4" />
                      Предполагаемая линия
                    </h3>
                    <ScrollArea className="h-[120px] rounded-md border bg-muted/20 md:h-[148px] xl:h-[132px]">
                      <div className="space-y-1 p-3 font-mono text-sm leading-6">
                        {evaluatingLine && <p className="text-muted-foreground">Считаю прогноз...</p>}
                        {!evaluatingLine && predictionLine.length === 0 && <p className="text-muted-foreground">Появится после ответа AI.</p>}
                        {predictionLine.map((item, index) => (
                          <div key={`${item}-${index}`} className="rounded-md px-2 py-1.5 hover:bg-muted/40">
                            <span className="text-muted-foreground">{index + 1}.</span> {item}
                          </div>
                        ))}
                      </div>
                    </ScrollArea>
                  </section>
                )}

                <section className="shrink-0">
                  <h3 className="mb-2 flex items-center gap-2 text-sm font-semibold">
                    <History className="h-4 w-4" />
                    История
                  </h3>
                  <ScrollArea className="h-[236px] rounded-md border bg-muted/20">
                    <div className="space-y-1 p-3 font-mono text-sm">
                      {moves.length === 0 && <p className="text-muted-foreground">Пока без ходов</p>}
                      {moves.map((move, index) => (
                        <div key={`${move.uci}-${index}`} className="flex items-center justify-between gap-3 rounded-md px-2 py-1.5 hover:bg-muted/40">
                          <span className="min-w-0 truncate">
                            {index + 1}. {moveSide(move.color)} {move.san}
                          </span>
                          <span className="shrink-0 text-muted-foreground">{move.uci}</span>
                        </div>
                      ))}
                    </div>
                  </ScrollArea>
                </section>

                {noLegalMoves && (
                  <p className="rounded-md border border-amber-500/35 bg-amber-500/10 px-3 py-2 text-xs text-amber-100">
                    Нет доступных ходов. Можно нажать "Назад" и откатить ход.
                  </p>
                )}

                {notice && (
                  <p className="rounded-md border border-destructive/35 bg-destructive/10 px-3 py-2 text-xs text-destructive-foreground">
                    {notice}
                  </p>
                )}
              </CardContent>
            </Card>
          </motion.section>
        )}
      </AnimatePresence>

      <AnimatePresence>
        {pendingMode && (
          <motion.div
            key="timer-modal"
            className="fixed inset-0 z-50 flex items-center justify-center bg-background/75 p-4 backdrop-blur-sm"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            onMouseDown={() => setPendingMode(null)}
          >
            <motion.div
              className="grid w-full max-w-[420px] gap-4 rounded-lg border bg-card p-4 shadow-panel"
              initial={{ opacity: 0, scale: 0.96, y: 12 }}
              animate={{ opacity: 1, scale: 1, y: 0 }}
              exit={{ opacity: 0, scale: 0.96, y: 12 }}
              transition={{ duration: 0.18, ease: "easeOut" }}
              onMouseDown={(event) => event.stopPropagation()}
            >
              <div className="flex items-center justify-between gap-3">
                <p className="flex items-center gap-2 text-lg font-semibold">
                  <Clock3 className="h-5 w-5" />
                  Таймер перед игрой
                </p>
                <Badge variant="outline">{pendingMode === "690" ? `Chess960 #${settings.chess960Index}` : modeTitle(pendingMode)}</Badge>
              </div>
              <Select value={timerConfig.timerPreset} onValueChange={(value) => updateTimerPreset(value as TimerPreset)}>
                <SelectTrigger className="h-12">
                  <SelectValue />
                </SelectTrigger>
                <SelectContent>
                  {timerOptions.map((option) => (
                    <SelectItem key={option.value} value={option.value}>
                      {option.label}
                    </SelectItem>
                  ))}
                </SelectContent>
              </Select>
              {timerConfig.timerPreset === "custom" && (
                <input
                  className="h-12 w-full rounded-md border border-input bg-transparent px-3 text-sm shadow-sm outline-none focus:ring-1 focus:ring-ring"
                  min={1}
                  max={180}
                  type="number"
                  value={timerConfig.customMinutes}
                  onChange={(event) => updateCustomMinutes(event.target.value)}
                />
              )}
              {(pendingMode === "1vAI" || pendingMode === "690") && (
                <div className="space-y-2">
                  <p className="text-sm font-medium">Движок</p>
                  <div className="grid gap-2 sm:grid-cols-2">
                    <Button
                      type="button"
                      variant={timerConfig.engine === "alpha-beta" ? "default" : "outline"}
                      className="h-auto min-h-12 flex-col items-start gap-1 whitespace-normal px-3 py-2"
                      disabled={pendingMode === "690"}
                      onClick={() => updateEngineChoice("alpha-beta")}
                    >
                      <span>Наш альфа-бета</span>
                      {pendingMode === "690" && <span className="text-xs opacity-70">Недоступно для Фишера</span>}
                    </Button>
                    <Button
                      type="button"
                      variant={timerConfig.engine === "stockfish" ? "default" : "outline"}
                      className="h-auto min-h-12 flex-col items-start gap-1 whitespace-normal px-3 py-2"
                      onClick={() => updateEngineChoice("stockfish")}
                    >
                      <span>Stockfish</span>
                      <span className="text-xs opacity-70">official-stockfish/stockfish</span>
                      {health && !health.stockfish_exists && <span className="text-xs text-amber-300">Бинарник не найден</span>}
                    </Button>
                  </div>
                </div>
              )}
              {timerConfig.engine === "stockfish" && (pendingMode === "1vAI" || pendingMode === "690") && (
                <div className="space-y-2">
                  <label className="text-sm font-medium" htmlFor="stockfish-skill">Сложность Stockfish</label>
                  <div className="grid grid-cols-[1fr_52px] items-center gap-3">
                    <input
                      id="stockfish-skill"
                      className="h-12 w-full"
                      min={0}
                      max={20}
                      type="range"
                      value={timerConfig.stockfishSkill}
                      onChange={(event) => updateStockfishSkill(event.target.value)}
                    />
                    <div className="rounded-md border bg-muted/25 px-2 py-2 text-center font-mono text-sm">
                      {timerConfig.stockfishSkill}
                    </div>
                  </div>
                </div>
              )}
              <div className="grid gap-3 sm:grid-cols-2">
                <div className="space-y-2">
                  <label className="text-sm font-medium" htmlFor="increment-seconds">Добавка, сек</label>
                  <input
                    id="increment-seconds"
                    className="h-12 w-full rounded-md border border-input bg-transparent px-3 text-sm shadow-sm outline-none focus:ring-1 focus:ring-ring"
                    min={0}
                    max={120}
                    type="number"
                    value={timerConfig.incrementSeconds}
                    onChange={(event) => updateIncrementSeconds(event.target.value)}
                  />
                </div>
                {pendingMode === "1vAI" && (
                  <div className="space-y-2">
                    <label className="text-sm font-medium" htmlFor="ai-delay">Задержка AI, сек</label>
                    <input
                      id="ai-delay"
                      className="h-12 w-full rounded-md border border-input bg-transparent px-3 text-sm shadow-sm outline-none focus:ring-1 focus:ring-ring"
                      min={0}
                      max={10}
                      step={0.1}
                      type="number"
                      value={timerConfig.aiDelayMs / 1000}
                      onChange={(event) => updateAiDelayMs(event.target.value)}
                    />
                  </div>
                )}
              </div>
              <div className="grid grid-cols-2 gap-2">
                <Button variant="outline" className="h-12" onClick={() => setPendingMode(null)}>
                  Отмена
                </Button>
                <Button className="h-12" onClick={() => startGame(pendingMode)}>
                  Старт
                </Button>
              </div>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>
    </main>
  );
}

// КОМПОНЕНТ ЛЕВОЙ ШКАЛЫ ОЦЕНКИ
function EngineEvaluationBar({ 
  evalScore, 
  isSearching 
}: { 
  evalScore: { type: "cp" | "mate"; value: number }; 
  isSearching: boolean; 
}) {

  console.log("Рендер шкалы. Текущий evalScore:", evalScore);
  
  const getEvalData = () => {
    if (evalScore.type === "mate") {
      const isWhiteWinning = evalScore.value > 0;
      return {
        markerPercent: isWhiteWinning ? 8 : 92,
        label: `M${Math.abs(evalScore.value)}`,
        accent: isWhiteWinning ? "bg-slate-950" : "bg-slate-100",
        ring: isWhiteWinning ? "border-slate-100" : "border-slate-950"
      };
    }

    const pawns = evalScore.value / 100;
    const clamped = Math.max(-5, Math.min(5, pawns));
    const markerPercent = 50 - (clamped / 5) * 42;
    const label = pawns === 0 ? "0.0" : `${pawns > 0 ? "+" : ""}${pawns.toFixed(1)}`;
    const accent = Math.abs(pawns) < 0.05 ? "bg-emerald-400" : markerPercent < 50 ? "bg-slate-950" : "bg-slate-100";
    const ring = markerPercent < 50 ? "border-slate-100" : markerPercent > 50 ? "border-slate-950" : "border-emerald-950";

    return { markerPercent, label, accent, ring };
  };

  const data = getEvalData();

  return (
    <div className="flex w-12 shrink-0 flex-col items-center gap-2">
      <span className="text-[10px] font-bold tracking-wide text-muted-foreground/80">EVAL</span>
      <div className="relative min-h-0 w-5 flex-1 rounded-md border border-border bg-gradient-to-b from-slate-100 via-slate-400 to-slate-950 shadow-inner overflow-hidden">
        <div className="absolute top-0 bottom-1/2 left-0 right-0 bg-slate-100" />
        <div className="absolute top-1/2 bottom-0 left-0 right-0 bg-slate-950" />
        <div className="absolute inset-x-0 top-1/2 h-px -translate-y-1/2 bg-emerald-400/80 z-10" />
        <div
          className={cn(
            "absolute left-1/2 h-4 w-4 -translate-x-1/2 -translate-y-1/2 rounded-full border-2 shadow-[0_0_0_1px_rgba(15,23,42,.35),0_4px_12px_rgba(0,0,0,.38)] transition-all duration-500 ease-out z-20 flex items-center justify-center",
            data.accent,
            data.ring,
            isSearching && "animate-pulse"
          )}
          style={{ top: `${data.markerPercent}%` }}
        />
      </div>
      <div className={cn(
        "w-full rounded border px-1 py-0.5 text-center shadow-sm transition-all duration-300",
        evalScore.value > 0 ? "bg-slate-100 text-slate-950 border-slate-200" : evalScore.value < 0 ? "bg-slate-950 text-slate-100 border-slate-800" : "bg-emerald-500/20 text-emerald-300 border-emerald-500/40"
      )}>
        <p className="font-mono text-[10px] font-bold leading-tight">{data.label}</p>
      </div>
    </div>
  );
}
