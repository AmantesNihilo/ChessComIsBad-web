export type GameMode = "1v1" | "1vAI" | "690" | "690vAI";

export type TimerPreset = "none" | "3" | "5" | "10" | "15" | "custom";

export type AppSettings = {
  pieceTheme: string;
  boardTexture: string;
  highlightMoves: boolean;
  chess960Index: number;
  soundless: boolean;
};

export type MoveRecord = {
  san: string;
  uci: string;
  color: "w" | "b";
  chess960Castle?: "king" | "queen";
};
