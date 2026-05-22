#pragma once

#include <string>
#include <unordered_map>

#include "types.hpp"

namespace hydra::core {

/**
 * @brief Структура атомарного снимка данных для персистентного хранения.
 *        Фоновый поток сброса преобразует этот снимок в batch-запрос к Redis.
 */
struct MetricSnapshot {
    std::unordered_map<std::string, double> counters;
    std::unordered_map<std::string, double> gauges;
};

class IMetricAggregator {
public:
    virtual ~IMetricAggregator() = default;

    /**
     * @brief Обновление метрики в памяти.
     *        Метод вызывается асинхронно множеством сетевых воркеров Boost.Asio.
     *        Должен иметь Thread-Safe реализацию без использования тяжелых Mutex.
     *
     * @param packet Распарсенный бинарный пакет с метрикой.
     */
    virtual void update_metric(const MetricPacket& packet) noexcept = 0;

    /**
     * @brief Экстракция текущего снимка данных.
     *        Вызывается фоновым потоком Flush Worker. Метод должен атомарно
     *        извлечь текущие саккумулированные данные и очистить счетчики (Counters).
     *
     * @return MetricSnapshot Снимок агрегированных метрик на момент вызова.
     */
    virtual MetricSnapshot extract_snapshot() = 0;
};

}  // namespace hydra::core
