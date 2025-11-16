#pragma once
#include <vector>
#include <string>
#include <optional>
#include <mutex>
#include "Telemetry/data_writing/DataAggregator.hpp" 

class PlaybackManager {
public:
    PlaybackManager() = default;

    // Metodo per caricare i dati
    void setData(std::vector<DbRow>&& data);
    void clearData();

    // Metodi per la navigazione
    void setCurrentIndex(size_t index);
    size_t getCurrentIndex() const;
    size_t getTotalRows() const;

    // Ottiene la riga di dati alla posizione corrente
    std::optional<DbRow> getCurrentRow() const;
    std::optional<DbRow> getRowAtIndex(size_t index) const;

    // Ottiene una "finestra" di dati per i grafici
    std::vector<DbRow> getDataWindow(size_t centerIndex, size_t halfWindowSize) const;

private:
    std::vector<DbRow> m_aggregatedData;
    size_t m_currentIndex = 0;
    mutable std::mutex m_dataMutex;
};