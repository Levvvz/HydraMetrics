# ==============================================================================
# STAGE 1: Сборка приложения (Builder)
# ==============================================================================
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

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

# Скачиваем оригинальный vcpkg от Microsoft в папку /opt/vcpkg
WORKDIR /opt
RUN git clone https://github.com && \
    ./vcpkg/bootstrap-vcpkg.sh

ENV VCPKG_ROOT=/opt/vcpkg

# Копируем манифесты
WORKDIR /app
COPY vcpkg.json ./
COPY CMakeLists.txt ./

# Скачиваем и компилируем зависимости (Boost, GTest, Redis) в кэш Docker
RUN cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
    -G Ninja

# Копируем локальный исходный код проекта (Docker сам возьмет его из контекста сборки)
COPY src/ ./src
COPY tests/ ./tests
COPY benchmarks/ ./benchmarks

# Финальная компиляция нашего исполняемого файла
RUN cmake --build build --config Release

# ==============================================================================
# STAGE 2: Минимальный образ для запуска (Runner)
# ==============================================================================
FROM ubuntu:24.04 AS runner

RUN apt-get update && apt-get install -y \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m appuser
USER appuser

WORKDIR /home/appuser/app

# Копируем бинарник. Если таргет в CMake называется иначе, поправим имя позже.
COPY --from=builder /app/build/src/hydrametrics_server ./

EXPOSE 8080

CMD ["./hydrametrics_server"]