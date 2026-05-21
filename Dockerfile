# ==============================================================================
# STAGE 1: Сборка приложения (Builder)
# ==============================================================================
FROM ubuntu:24.04 AS builder

# Исключаем интерактивные диалоги при установке пакетов
ENV DEBIAN_FRONTEND=noninteractive

# Устанавливаем системные зависимости для компиляции и vcpkg
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    tar \
    unzip \
    curl \
    git \
    zip \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Настраиваем и собираем vcpkg
WORKDIR /opt
RUN git clone https://github.com/Levvvz/HydraMetrics && \
    ./vcpkg/bootstrap-vcpkg.sh

ENV VCPKG_ROOT=/opt/vcpkg

# Копируем файлы описания проекта и манифест зависимостей
WORKDIR /app
COPY vcpkg.json ./
COPY CMakeLists.txt ./

# Конфигурируем зависимости vcpkg заранее для кэширования слоев Docker
RUN cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -G Ninja

# Копируем исходный код проекта
COPY src/ ./src
COPY tests/ ./tests
COPY benchmarks/ ./benchmarks

# Финальная сборка бинарного файла в Release режиме с LTO и -O3
RUN cmake --build build --config Release

# ==============================================================================
# STAGE 2: Минимальный образ для запуска (Runner)
# ==============================================================================
FROM ubuntu:24.04 AS runner

RUN apt-get update && apt-get install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Создаем безопасного не-root пользователя для запуска сервиса
RUN useradd -m appuser
USER appuser

WORKDIR /home/appuser/app

# Копируем только готовый скомпилированный исполняемый файл из первой стадии
COPY --from=builder /app/build/src/hydrametrics_server ./

# Сервер слушает входящие метрики на порту 8080
EXPOSE 8080

# Точка входа для запуска нашего C++ движка
CMD ["./hydrametrics_server"]
