#include "listeners.h"

namespace lcr {

std::string ListenerRegistry::token() {
  static const char* hex = "0123456789abcdef";
  std::string t;
  for (int w = 0; w < 4; w++) {
    const uint32_t v = rng_();
    for (int i = 28; i >= 0; i -= 4) t += hex[(v >> i) & 15];
  }
  return t;
}

void ListenerRegistry::setConfig(const Config& c) {
  std::lock_guard<std::mutex> l(m_);
  cfg_ = c;
}

ListenerRegistry::Config ListenerRegistry::config() const {
  std::lock_guard<std::mutex> l(m_);
  return cfg_;
}

ListenerRegistry::Add ListenerRegistry::addPending(const std::string& callsign, const std::string& email, const std::string& source, uint32_t nowMs, std::string& code) {
  std::lock_guard<std::mutex> l(m_);
  // il limite: per lo stesso nominativo e per la stessa origine, una richiesta ogni cooldownMs
  for (const std::string& k : {"C:" + callsign, "S:" + source}) {
    auto it = lastAsk_.find(k);
    if (it != lastAsk_.end() && static_cast<uint32_t>(nowMs - it->second) < cfg_.cooldownMs) {
      stats_.tooSoon++;
      return Add::TooSoon;
    }
  }
  // quelle scadute non contano
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (lastExpired(nowMs, it->second.atMs, cfg_.pendingTtlMs)) it = pending_.erase(it);
    else ++it;
  }
  if (pending_.size() >= cfg_.maxPending) {
    stats_.busy++;
    return Add::Busy;
  }
  // una richiesta nuova dello stesso nominativo sostituisce la vecchia (il codice vecchio non vale piu')
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (it->second.callsign == callsign) it = pending_.erase(it);
    else ++it;
  }
  if (lastAsk_.size() > 4000) lastAsk_.clear();
  lastAsk_["C:" + callsign] = nowMs;
  lastAsk_["S:" + source] = nowMs;
  code = token();
  pending_[code] = Pending{callsign, email, source, nowMs};
  stats_.requested++;
  return Add::Ok;
}

bool ListenerRegistry::confirm(const std::string& code, uint32_t nowMs, std::string& ticket, std::string& callsign) {
  std::lock_guard<std::mutex> l(m_);
  auto it = pending_.find(code);
  if (it == pending_.end()) return false;
  const Pending p = it->second;
  pending_.erase(it);
  if (lastExpired(nowMs, p.atMs, cfg_.pendingTtlMs)) return false;
  for (auto s = sessions_.begin(); s != sessions_.end();) {  // le scadute
    if (static_cast<int32_t>(nowMs - s->second.untilMs) > 0) s = sessions_.erase(s);
    else ++s;
  }
  if (sessions_.size() >= cfg_.maxSessions) return false;
  ticket = token();
  sessions_[ticket] = Session{p.callsign, nowMs + cfg_.sessionTtlMs};
  callsign = p.callsign;
  stats_.confirmed++;
  return true;
}

bool ListenerRegistry::ticketValid(const std::string& ticket, uint32_t nowMs, std::string* callsign) const {
  std::lock_guard<std::mutex> l(m_);
  auto it = sessions_.find(ticket);
  if (it == sessions_.end() || static_cast<int32_t>(nowMs - it->second.untilMs) > 0) return false;
  if (callsign) *callsign = it->second.callsign;
  return true;
}

uint32_t ListenerRegistry::startListening(const std::string& ticket, uint32_t nowMs, std::string& callsign) {
  std::lock_guard<std::mutex> l(m_);
  auto it = sessions_.find(ticket);
  if (it == sessions_.end() || static_cast<int32_t>(nowMs - it->second.untilMs) > 0) {
    stats_.refused++;
    return 0;
  }
  callsign = it->second.callsign;
  size_t live = 0, mine = 0;
  for (const auto& o : open_) {
    if (o.second.kicked) continue;
    live++;
    if (o.second.callsign == callsign) mine++;
  }
  // chi ha gia' troppi flussi aperti: il piu' vecchio dello stesso biglietto lascia il posto (si riapre la pagina e il vecchio si chiude)
  if (mine >= cfg_.maxPerCallsign) {
    for (auto& o : open_) {
      if (!o.second.kicked && o.second.ticket == ticket) {
        o.second.kicked = true;
        live--;
        mine--;
        break;
      }
    }
  }
  if (live >= cfg_.maxListening || mine >= cfg_.maxPerCallsign) {
    stats_.refused++;
    return 0;
  }
  const uint32_t id = nextId_++;
  open_[id] = Open{callsign, ticket, nowMs, false};
  stats_.started++;
  return id;
}

void ListenerRegistry::stopListening(uint32_t id) {
  std::lock_guard<std::mutex> l(m_);
  open_.erase(id);
}

std::vector<ListenerRegistry::Listening> ListenerRegistry::listening() const {
  std::lock_guard<std::mutex> l(m_);
  std::vector<Listening> v;
  for (const auto& o : open_) {
    if (o.second.kicked) continue;
    Listening x;
    x.id = o.first;
    x.callsign = o.second.callsign;
    x.sinceMs = o.second.sinceMs;
    v.push_back(x);
  }
  return v;
}

size_t ListenerRegistry::kick(const std::string& callsign) {
  std::lock_guard<std::mutex> l(m_);
  size_t n = 0;
  for (auto it = sessions_.begin(); it != sessions_.end();) {
    if (it->second.callsign == callsign) {
      it = sessions_.erase(it);
      n++;
    } else ++it;
  }
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (it->second.callsign == callsign) it = pending_.erase(it);
    else ++it;
  }
  for (auto& o : open_) {
    if (o.second.callsign == callsign && !o.second.kicked) {
      o.second.kicked = true;
      n++;
    }
  }
  return n;
}

bool ListenerRegistry::isKicked(uint32_t id) const {
  std::lock_guard<std::mutex> l(m_);
  auto it = open_.find(id);
  return it == open_.end() || it->second.kicked || sessions_.find(it->second.ticket) == sessions_.end();
}

void ListenerRegistry::expire(uint32_t nowMs) {
  std::lock_guard<std::mutex> l(m_);
  for (auto it = pending_.begin(); it != pending_.end();) {
    if (lastExpired(nowMs, it->second.atMs, cfg_.pendingTtlMs)) it = pending_.erase(it);
    else ++it;
  }
  for (auto it = sessions_.begin(); it != sessions_.end();) {
    if (static_cast<int32_t>(nowMs - it->second.untilMs) > 0) it = sessions_.erase(it);
    else ++it;
  }
}

size_t ListenerRegistry::pending() const {
  std::lock_guard<std::mutex> l(m_);
  return pending_.size();
}

size_t ListenerRegistry::sessions() const {
  std::lock_guard<std::mutex> l(m_);
  return sessions_.size();
}

ListenerRegistry::Stats ListenerRegistry::stats() const {
  std::lock_guard<std::mutex> l(m_);
  return stats_;
}

}  // namespace lcr
