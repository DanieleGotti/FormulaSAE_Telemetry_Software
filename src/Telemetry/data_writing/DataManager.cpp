#include "Telemetry/data_writing/DataManager.hpp"

void DataManager::addSubscriber(std::shared_ptr<IWritingSubscriber> subscriber) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    m_subscribers.push_back(subscriber); 
}

void DataManager::processData(const PacketParser& packet) {
    std::lock_guard<std::mutex> lock(m_subscriberMutex);
    
    for (auto it = m_subscribers.begin(); it != m_subscribers.end(); ) {
        // Se l'oggetto a cui puntava è stato distrutto, il risultato sarà un puntatore nullo.
        if (auto subscriber = it->lock()) {
            //L'oggetto esiste ancora. Possiamo chiamare il suo metodo.
            subscriber->onDataReceived(packet);
            ++it;
        } else {
            // Il subscriber non esiste più, puntatore rimosso
            it = m_subscribers.erase(it);
        }
    }
}