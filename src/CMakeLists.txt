# Компонент ядра (Парсер, Агрегатор)
add_library(hydra_core
    core/metric_aggregator.cpp
    core/protocol_parser.cpp
)
target_include_directories(hydra_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(hydra_storage
    storage/flush_worker.cpp
    storage/redis_storage.cpp
)
target_include_directories(hydra_storage PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(hydra_storage PUBLIC
    hydra_core
)
if(TARGET redis++::redis++)
    target_link_libraries(hydra_storage PRIVATE redis++::redis++)
elseif(TARGET redis++::redis++_static)
    target_link_libraries(hydra_storage PRIVATE redis++::redis++_static)
elseif(TARGET unofficial::redis++::redis++)
    target_link_libraries(hydra_storage PRIVATE unofficial::redis++::redis++)
elseif(TARGET Redis++::Redis++)
    target_link_libraries(hydra_storage PRIVATE Redis++::Redis++)
else()
    message(FATAL_ERROR "Redis++ CMake target was not found")
endif()

# Новое: Компонент сети (TCP-сервер)
add_library(hydra_network INTERFACE)
target_include_directories(hydra_network INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
