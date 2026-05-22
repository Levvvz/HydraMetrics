# Компонент ядра (Парсер, Агрегатор)
add_library(hydra_core INTERFACE)
target_include_directories(hydra_core INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Новое: Компонент сети (TCP-сервер)
add_library(hydra_network INTERFACE)
target_include_directories(hydra_network INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)
