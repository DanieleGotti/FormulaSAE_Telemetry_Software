#pragma once
#include <thread>
#include <iostream>
#include <vector>
#include <string>

typedef enum {
    ACQUISITION_METHOD_SERIAL = 0,
    ACQUISITION_METHOD_NETWORK = 1
} AcquisitionMethod;

class ServiceManager {
public:
    // Istanzia tutti i servizi necessari senza startare i thread
    static void initialize();
    // Restituisce tutti i dispositivi di connessione disponibili (es. porte seriali, dispositivi di rete, ecc.)
    static std::vector<std::string> getAllConnectionOptions();
    // Imposta il path del database (da chiamare prima di startare i servizi)
    static void setDBPath(const std::string& path);
    // Imposta il metodo di acquisizione (da chiamare prima di startare i servizi)
    static void setAcquisitionMethod(int method);
    // Avvia i thread dei servizi
    static void startServices();
    // Ferma i thread dei servizi
    static void stopTasks();

private:
    static std::string dbPath;
};