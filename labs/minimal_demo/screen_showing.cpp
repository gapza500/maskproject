#include "screen_showing.h"

void ScreenShowingU8g2::begin(ScreenManagerU8g2* sm){ sm_ = sm; }
void ScreenShowingU8g2::setProviders(const Providers& p){ prov_ = p; }
void ScreenShowingU8g2::setStatus(const StatusMap& smap){
  status_ = smap;
  sm_->setLayout({.status=smap.cfg});
}

void ScreenShowingU8g2::setPages(const std::vector<PageSpec>& ps, PageId initial){
  pages_ = ps;
  show(initial);
}

void ScreenShowingU8g2::setAlertPolicy(const AlertPolicy& ap){
  alert_ = ap;
  sm_->setAlertRule(ap.rule);
}

void ScreenShowingU8g2::setDefaultFx(const FxConfig& fx){ fxDefault_ = fx; }
void ScreenShowingU8g2::setDefaultRefresh(uint32_t ms){ defaultRefreshMs_ = ms; }

void ScreenShowingU8g2::_renderStatus(){
  for (auto& it : status_.items){
    StatusItem si;
    si.icon = it.icon;
    si.visible = true;

    // ใช้ข้อมูลจาก Provider
    if (prov_.getText) si.label = prov_.getText(it.key);
    if (prov_.getLevel) si.value = prov_.getLevel(it.key);
    sm_->setStatusItem(it.sector, si);
  }
}

void ScreenShowingU8g2::_applyPage(PageId id){
  int idx = -1;
  for (int i = 0; i < (int)pages_.size(); ++i)
    if (pages_[i].id == id) { idx = i; break; }

  if (idx < 0) return;
  const auto& pg = pages_[idx];
  sm_->setLayout({ .data = pg.dataLayout });
  FxConfig fx = fxDefault_;
  fx.alertEnter = pg.enterFx;
  fx.alertExit  = pg.exitFx;
  sm_->setFx(fx);
  curIndex_ = idx;
  lastRefresh_ = 0;
}

void ScreenShowingU8g2::_refreshDataIfNeeded(uint32_t nowMs){
  if (curIndex_ < 0) return;
  const auto& pg = pages_[curIndex_];
  uint32_t every = pg.refreshMs ? pg.refreshMs : defaultRefreshMs_;
  if (nowMs - lastRefresh_ < every) return;
  lastRefresh_ = nowMs;

  std::vector<String> items;
  items.reserve(pg.dataKeys.size());
  for (size_t i = 0; i < pg.dataKeys.size(); ++i){
    String raw = prov_.getText ? prov_.getText(pg.dataKeys[i]) : "-";
    if (i < pg.formatters.size() && pg.formatters[i]) 
      raw = pg.formatters[i](raw);
    items.push_back(raw);
  }
  sm_->setDataArray(items);
}

void ScreenShowingU8g2::show(PageId id){ _applyPage(id); }
PageId ScreenShowingU8g2::current() const{
  if (curIndex_ < 0) return PageId::Home;
  return pages_[curIndex_].id;
}

void ScreenShowingU8g2::triggerAlert(const String& key, const String& rawTitle, const String& rawSubtitle){
  for (auto& m : alert_.maps){
    if (m.key == key){
      sm_->pushAlert(AlertItem{ m.type, m.icon, rawTitle, rawSubtitle, m.iconScale, m.durationMs });
      return;
    }
  }
}

void ScreenShowingU8g2::requestDisplayOff(uint32_t ms){ /* optional: ใส่ถ้ามีฟังก์ชันใน manager */ }
void ScreenShowingU8g2::wakeDisplay(){ /* optional: ใส่ถ้ามีฟังก์ชันใน manager */ }

void ScreenShowingU8g2::loop(uint32_t nowMs){
  _renderStatus();
  _refreshDataIfNeeded(nowMs);
  sm_->loop(nowMs);
}
