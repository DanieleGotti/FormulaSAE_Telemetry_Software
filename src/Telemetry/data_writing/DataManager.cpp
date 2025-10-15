#include <algorithm>
#include "Telemetry/data_writing/DataManager.hpp"

void DataManager::addSubscriber(std::shared_ptr<IWritingSubscriber> subscriber) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    m_subscribers.push_back(subscriber); 
}

void DataManager::removeSubscriber(std::shared_ptr<IWritingSubscriber> subscriberToRemove) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);

    // Sposta tutti gli elementi da rimuovere alla fine del vettore e restituisce un iteratore al primo elemento da rimuovere
    auto new_end = std::remove_if(m_subscribers.begin(), m_subscribers.end(),
        [&](const std::weak_ptr<IWritingSubscriber>& weak_sub) {
            // La condizione è vera se l'elemento va rimosso (puntatore debole scaduto o puntatore corrispondente)
            if (weak_sub.expired()) {
                return true;
            }
            return weak_sub.lock() == subscriberToRemove;
        });

    // Rimuove gli elementi dalla fine del vettore
    m_subscribers.erase(new_end, m_subscribers.end());
}

void DataManager::processData(const PacketParser& packet) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    
    for (auto it = m_subscribers.begin(); it != m_subscribers.end(); ) {
        // Se l'oggetto a cui puntava è stato distrutto, il risultato sarà un puntatore nullo
        if (auto subscriber = it->lock()) {
            // L'oggetto esiste ancora, possiamo chiamare il suo metodo
            subscriber->onDataReceived(packet);
            ++it;
        } else {
            // Il subscriber non esiste più, puntatore rimosso
            it = m_subscribers.erase(it);
        }
    }
}