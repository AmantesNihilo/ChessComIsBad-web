#!/bin/bash
# Linux start-dev.sh script for TRPO project
cd "$(dirname "$0")"

# При нажатии Ctrl+C или закрытии окна — чисто убиваем и бэк, и фронт
trap 'kill $(jobs -p) 2>/dev/null' EXIT

echo "====== Сборка и запуск TRPO Chess ======"

# 1. Запуск бэкенда
if [ -d "backend" ]; then
    echo "[1/2] Проверка бэкенда..."
    cd backend
    
    if command -v python3 &>/dev/null; then
        # Если venv еще нет — создаем
        if [ ! -d "venv" ]; then
            echo "Создаю виртуальное окружение..."
            python3 -m venv venv
        fi
        
        # Активируем локальный venv
        source venv/bin/activate
        
        # Ставим зависимости, только если они не стоят (или если обновился requirements.txt)
        if [ -f "requirements.txt" ]; then
            echo "Проверяю зависимости Python..."
            pip install -r requirements.txt --quiet
        fi
        
        echo "Запускаю FastAPI бэкенд на порту 8000..."
        # !!! ВНИМАНИЕ: Если твой файл лежит прямо в backend/main.py, запускаем uvicorn вот так:
        uvicorn app.main:app --port 8000 &
        
        # Если uvicorn не хочешь юзать, а запускаешь через python3 напрямую, раскомментируй строку ниже (а uvicorn закомментируй):
        # python3 main.py &
    else
        echo "Ошибка: Python3 не найден в системе."
        exit 1
    fi
    cd ..
fi

# Небольшая пауза, чтобы бэк успел занять порт до старта фронта
sleep 1.2

# 2. Запуск фронтенда
# ПОПРАВЬ ПУТЬ: Если папки "frontend" нет, а package.json лежит прямо в корне, измени на [ -f "package.json" ]
if [ -d "frontend" ]; then
    echo "[2/2] Запуск фронтенда..."
    cd frontend
    if command -v npm &>/dev/null; then
        # Установка ноды только если нет папки node_modules
        if [ ! -d "node_modules" ]; then
            echo "Папка node_modules не найдена, запускаю npm install..."
            npm install
        fi
        # Фронт запускаем НЕ в фоне, чтобы он держал терминал открытым и выводил логи
        npm run dev
    else
        echo "Ошибка: npm не найден. Установи Node.js."
        exit 1
    fi
    cd ..
else
    # Если фронт лежит прямо в корневом каталоге проекта
    if [ -f "package.json" ]; then
        echo "[2/2] Запуск фронтенда из корня..."
        if [ ! -d "node_modules" ]; then
            npm install
        fi
        npm run dev
    fi
fi

# Ждем завершения фоновых процессов, если фронт вдруг упадет
wait