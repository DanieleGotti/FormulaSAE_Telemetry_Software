"""Genera AmkDiagnostics.hpp / .cpp dall'indice della documentazione AMK PDK_025786.

Uso:
    python scripts/gen_amk_diagnostics.py

Scarica il TOC della documentazione AMKmotion e ne estrae la lista completa
"numero di diagnostica -> descrizione". E' la stessa lista usata dal firmware
ECU (source/powertrain_control/inverter/INVERTER_Errors.h): se AMK pubblica una
versione nuova del documento, rilanciare entrambi gli script.
"""
import os
import re
import urllib.request

TOC_URL = ('https://doku.amk-motion.com/en/Subsystems/PDK_025786_Diagnose/'
           'Data/Tocs/PDK_025786_Diagnose_Chunk0.js')

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HPP_PATH = os.path.join(REPO_ROOT, 'include', 'Telemetry', 'data_acquisition', 'AmkDiagnostics.hpp')
CPP_PATH = os.path.join(REPO_ROOT, 'src', 'Telemetry', 'data_acquisition', 'AmkDiagnostics.cpp')

BS = chr(92)

GROUPS = [
    (257, 260, 'CONTROL_PANEL'),
    (512, 525, 'MONITOR'),
    (768, 799, 'DATABASE'),
    (1024, 1120, 'CENTRAL_COMPUTER'),
    (1280, 1460, 'PARAMETER'),
    (1536, 1559, 'EEPROM'),
    (1792, 1849, 'DRIVE_COMMANDING'),
    (2048, 2079, 'SBUS'),
    (2304, 2370, 'INVERTER_HW'),
    (2560, 2599, 'SERCOS'),
    (2600, 2619, 'PROFIBUS'),
    (2620, 2629, 'ARCNET'),
    (2640, 2649, 'LON'),
    (2660, 2669, 'INTERBUS'),
    (2680, 2699, 'CAN'),
    (2720, 2739, 'ETHERCAT'),
    (2740, 2749, 'MODBUS'),
    (2760, 2764, 'COE'),
    (2816, 2819, 'OPTION_PS'),
    (3071, 3079, 'OPTION_SERCOS'),
    (3328, 3328, 'OPTION_NC'),
    (3584, 3603, 'SPECIAL_FUNCTIONS'),
    (3605, 3610, 'FUNCTIONAL_SAFETY'),
    (3840, 3871, 'OPERATING_SYSTEM'),
]


def unescape(s):
    return re.sub((BS * 2) + '+u([0-9a-fA-F]{4})',
                  lambda m: chr(int(m.group(1), 16)), s)


def load_rows():
    with urllib.request.urlopen(TOC_URL) as response:
        text = response.read().decode('utf-8')
    pairs = re.findall(r"'(/Content/[^']+)':{i:\[\d+\],t:\['([^']*)'\]", text)
    rows = []
    for _path, title in pairs:
        title = unescape(title).replace('\xa0', ' ').strip().strip("'").strip()
        m = re.match(r'^(\d+)\s+(.*)$', title)
        if m:
            # la virgola romperebbe il CSV, il doppio apice la stringa C++
            name = m.group(2).strip().strip("'").strip()
            name = name.replace(',', ' -').replace('"', "'")
            rows.append((int(m.group(1)), ' '.join(name.split())))
    rows.sort()
    assert len({c for c, _ in rows}) == len(rows), 'codici duplicati'
    return rows


HEADER = '''// AmkDiagnostics.hpp
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
'''


def gen_cpp(rows):
    L = []
    a = L.append
    a('// AmkDiagnostics.cpp')
    a('//')
    a('// File generato da scripts/gen_amk_diagnostics.py: non modificare a mano.')
    a('')
    a('#include "Telemetry/data_acquisition/AmkDiagnostics.hpp"')
    a('')
    a('#include <algorithm>')
    a('#include <cstdlib>')
    a('#include <cstring>')
    a('')
    a('namespace {')
    a('')
    a('struct DiagEntry { uint16_t code; const char* name; };')
    a('')
    a('// Ordinata per codice: la ricerca e\' binaria')
    a('const DiagEntry kDiagnostics[] = {')
    for code, title in rows:
        a('    { %5dU, "%s" },' % (code, title))
    a('};')
    a('')
    a('struct DiagGroup { uint16_t lo; uint16_t hi; const char* name; };')
    a('')
    a('const DiagGroup kGroups[] = {')
    for lo, hi, tag in GROUPS:
        a('    { %5dU, %5dU, "%s" },' % (lo, hi, tag))
    a('};')
    a('')
    warnings = sorted({c for c, t in rows if 'warning' in t.lower()})
    a('// Diagnostiche marcate "warning" da AMK ("Message classes", PDK_025786)')
    a('const uint16_t kWarnings[] = {')
    a('    ' + ', '.join('%dU' % c for c in warnings))
    a('};')
    a('')
    a('} // namespace')
    a('')
    a('const char* amkDiagName(uint16_t code) {')
    a('    const auto* begin = std::begin(kDiagnostics);')
    a('    const auto* end   = std::end(kDiagnostics);')
    a('    const auto* it = std::lower_bound(begin, end, code,')
    a('        [](const DiagEntry& entry, uint16_t value) { return entry.code < value; });')
    a('    return (it != end && it->code == code) ? it->name : nullptr;')
    a('}')
    a('')
    a('const char* amkDiagGroupName(uint16_t code) {')
    a('    if (code == 0U) return "NONE";')
    a('    for (const auto& group : kGroups) {')
    a('        if (code >= group.lo && code <= group.hi) return group.name;')
    a('    }')
    a('    return "UNKNOWN";')
    a('}')
    a('')
    a('bool amkDiagIsWarning(uint16_t code) {')
    a('    for (const uint16_t warning : kWarnings) {')
    a('        if (warning == code) return true;')
    a('    }')
    a('    return false;')
    a('}')
    a('')
    a('std::string amkDiagToText(uint16_t code) {')
    a('    if (code == 0U) return "NO_DIAG";')
    a('')
    a('    std::string text = std::to_string(code);')
    a('    text += \' \';')
    a('    text += amkDiagGroupName(code);')
    a('    text += \' \';')
    a('')
    a('    const char* name = amkDiagName(code);')
    a('    text += (name != nullptr) ? name : "see PDK_025786";')
    a('    return text;')
    a('}')
    a('')
    a('uint16_t amkDiagFromText(const char* text) {')
    a('    // Il testo inizia sempre con il numero, quindi rileggerlo basta')
    a('    if (text == nullptr || *text == \'\\0\') return 0U;')
    a('    return static_cast<uint16_t>(std::atoi(text));')
    a('}')
    return '\n'.join(L) + '\n'


def main():
    rows = load_rows()
    with open(HPP_PATH, 'w', encoding='utf-8', newline='\n') as f:
        f.write(HEADER)
    with open(CPP_PATH, 'w', encoding='utf-8', newline='\n') as f:
        f.write(gen_cpp(rows))
    print('codici: %d' % len(rows))


if __name__ == '__main__':
    main()
