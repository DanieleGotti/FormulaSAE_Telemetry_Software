// AmkDiagnostics.hpp
//
// Numeri di diagnostica AMK (AMK_ActualErrorInfo) trasmessi dall'ECU nei campi
// inv_L_errorInfo / inv_R_errorInfo del pacchetto 0xCC. Sono gli stessi numeri
// mostrati sul display dell'inverter, documentati da AMKmotion in PDK_025786
// "Diagnosis messages": https://doku.amk-motion.com/en/Subsystems/PDK_025786_Diagnose/
//
// File generato da scripts/gen_amk_diagnostics.py: non modificare a mano.

#pragma once

#include <cstdint>
#include <string>

// Descrizione del codice, oppure nullptr se non e' in elenco.
const char* amkDiagName(uint16_t code);

// Gruppo funzionale del codice (INVERTER_HW, CAN, FUNCTIONAL_SAFETY, ...).
const char* amkDiagGroupName(uint16_t code);

// true per le diagnostiche che AMK classifica "warning": non fermano il drive.
bool amkDiagIsWarning(uint16_t code);

// Testo mostrato in UI / CSV, es. "2347 INVERTER_HW Motor temperature error".
// Il codice 0 diventa "NO_DIAG".
std::string amkDiagToText(uint16_t code);

// Inverso esatto di amkDiagToText, per rileggere un CSV salvato.
uint16_t amkDiagFromText(const char* text);
