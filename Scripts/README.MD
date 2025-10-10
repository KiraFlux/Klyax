<div align="center">

# Klyax / Scripts

</div>

## [Klyax](klyax) - универсальный исполняемый python модуль для автоматизации проекта.

- Режимы задаются первым позиционным аргументом доступны следующие:

### Режим cleanup

- Назначение: рекурсивно ищет файлы в каталогах проекта по glob‑маскам и
  опционально удаляет их.

- Аргументы:
    - `masks` - 1+ glob‑масок (nargs='*').
    - `-d`, `--delete` - реально удалить файлы (по умолчанию dry run).

Примеры:

- Dry run:
    - `python -m klyax cleanup "*.bak" "*.log"`
- Удалить:
    - `python -m klyax cleanup -d "*.bak" "*.log"`
- Shell wrapper [`cleanup.sh`](cleanup.sh):
    - `sh cleanup.sh -d "*.bak" "*.log"`

### Режим display

- Назначение: Отобразить значение идентификатора из реестра моделей

- Аргументы:
    - `identifier` - Unix-подобный путь к модели

Примеры:

- Деталь:
    - `python -m klyax display Purchased/Propeller` - Выведет список моделей сборки, изображения и т.п., что учитывает (`ModelRegistry`)

- Сборочная единица:
    - `python -m klyax display Klyax` - Выведет список моделей сборки, изображения и т.п., что учитывает (`ModelRegistry`)
  