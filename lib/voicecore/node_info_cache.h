// Le presentazioni dei nodi (node_info.h) che il reflector ricorda anche dopo un riavvio: senza, un reflector appena avviato non conosce la posizione fissa dei
// nodi fino alla loro prossima presentazione (fino a 10 minuti), e per tutto quel tempo non puo' dire la distanza fra la stazione e il nodo. Il file e'
// nodi.cache accanto al file dei client: una riga per nodo, "id;lat;lon;frequenza kHz;modo;ssid;testo" (lat e lon in decimilionesimi di grado).
// Solo logica, provata sul PC (tests/test_node_info_cache.cpp).
#pragma once

#include <cstdint>
#include <map>
#include <string>

#include "node_info.h"

namespace lcr {

std::string formatNodeInfoCache(const std::map<uint32_t, NodeInfo>& m);
// Righe vuote e che cominciano con # si ignorano. Una riga non valida si salta (e si conta in 'skipped'): un file rovinato non deve fermare il reflector.
void parseNodeInfoCache(const std::string& text, std::map<uint32_t, NodeInfo>& out, size_t& skipped);
// Le due presentazioni dicono la stessa cosa (posizione, frequenza, modo, SSID e testo).
bool sameNodeInfo(const NodeInfo& a, const NodeInfo& b);

}  // namespace lcr
