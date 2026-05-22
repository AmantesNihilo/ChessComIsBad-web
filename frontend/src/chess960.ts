export function chess960BackRank(index: number): string {
  if (index < 0 || index > 959) throw new Error("Chess960 index must be 0..959");

  const pieces: Array<string | null> = Array(8).fill(null);
  const dark = [0, 2, 4, 6];
  const light = [1, 3, 5, 7];

  pieces[dark[index % 4]] = "B";
  index = Math.floor(index / 4);
  pieces[light[index % 4]] = "B";
  index = Math.floor(index / 4);

  let free = pieces.map((piece, square) => ({ piece, square })).filter((item) => item.piece === null);
  pieces[free[index % 6].square] = "Q";
  index = Math.floor(index / 6);

  free = pieces.map((piece, square) => ({ piece, square })).filter((item) => item.piece === null);
  const knightPairs = [
    [0, 1],
    [0, 2],
    [0, 3],
    [0, 4],
    [1, 2],
    [1, 3],
    [1, 4],
    [2, 3],
    [2, 4],
    [3, 4]
  ];
  const [a, b] = knightPairs[index];
  pieces[free[a].square] = "N";
  pieces[free[b].square] = "N";

  free = pieces.map((piece, square) => ({ piece, square })).filter((item) => item.piece === null);
  pieces[free[0].square] = "R";
  pieces[free[1].square] = "K";
  pieces[free[2].square] = "R";

  return pieces.join("");
}

export type Chess960Setup = {
  index: number;
  rank: string;
  kingFile: number;
  queenSideRookFile: number;
  kingSideRookFile: number;
};

export function chess960Setup(index: number): Chess960Setup {
  const rank = chess960BackRank(index);
  const kingFile = rank.indexOf("K");
  const rookFiles = rank
    .split("")
    .map((piece, file) => ({ piece, file }))
    .filter((item) => item.piece === "R")
    .map((item) => item.file);

  return {
    index,
    rank,
    kingFile,
    queenSideRookFile: rookFiles[0],
    kingSideRookFile: rookFiles[1]
  };
}

export function chess960Fen(index: number): string {
  const rank = chess960BackRank(index);
  return `${rank.toLowerCase()}/pppppppp/8/8/8/8/PPPPPPPP/${rank} w - - 0 1`;
}

export function chess690Fen(): string {
  return chess960Fen(690);
}
