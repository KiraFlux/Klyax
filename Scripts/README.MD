<div align="center">

# Klyax / Scripts

</div>

## [Klyax](klyax.py) - универсальный python‑скрипт для автоматизации проекта.

- Режимы задаются первым позиционным аргументом доступны следующие:

### Режим cleanup

- Назначение: рекурсивно ищет файлы в каталоге [Models](./../Models) по glob‑маскам и
  опционально удаляет их.

- Аргументы:
    - `masks` - 1+ glob‑масок (nargs='*').
    - `-d`, `--delete` - реально удалить файлы (по умолчанию dry run).

Примеры:

- Dry run:
    - `python klyax.py cleanup "*.bak" "*.log"`
- Удалить:
    - `python klyax.py cleanup -d "*.bak" "*.log"`
- Shell wrapper [`cleanup.sh`](cleanup.sh):
    - `sh cleanup.sh -d "*.bak" "*.log"`

Если каталога [Models](./../Models) не существует,
то скрипт выбрасывает `FileNotFoundError` и завершает работу с кодом 1
