# ♟️ TRPO Chess

Шахматное веб-приложение с React frontend, FastAPI backend и собственным C-движком.

## Возможности

- Игра в шахматы в браузере
- Режим игры против движка
- Режим для двух игроков
- Поддержка собственного C-движка
- Поддержка Stockfish
- Chess960
- Таймер партии
- История ходов
- Темы доски и фигур
- Анализ позиции и лучший ход

## Стек

- React
- TypeScript
- Vite
- Tailwind CSS
- FastAPI
- Python
- C

## Запуск через Docker

### Требования

Для запуска нужен только Docker и Docker Compose.

### Запуск

```bash
docker compose up --build
```

После запуска приложение будет доступно по адресу:

```text
http://localhost:5173
```

Backend API:

```text
http://localhost:8000
```

### Остановка

```bash
docker compose down
```

## Локальный запуск без Docker

### Требования

- Node.js
- Python 3.10+
- GCC
- Make

### Backend

```bash
pip install -r backend/requirements.txt
python -m uvicorn backend.app.main:app --host 127.0.0.1 --port 8000
```

### Frontend

```bash
cd frontend
npm install
npm run dev
```

### Сборка движка

```bash
cd engine
make
```

## Stockfish

Для использования Stockfish положите бинарный файл в папку:

```text
tools/stockfish/
```

Например:

```text
tools/stockfish/stockfish.exe
```

## Структура проекта

```text
ChessComIsBad-web/
├── assets/
├── backend/
├── docs/
├── engine/
├── frontend/
├── docker-compose.yml
├── Dockerfile.backend
└── Dockerfile.frontend
```

## Лицензия

MIT License
