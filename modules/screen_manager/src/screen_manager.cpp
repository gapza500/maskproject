#include "../include/screen_manager.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <pgmspace.h>

namespace screen_manager {
namespace {

#ifdef SCREEN_BACKEND_U8G2
using ColorType = uint8_t;
#else
using ColorType = uint16_t;
#endif

inline ColorType fgColor(bool inverted) {
#ifdef SCREEN_BACKEND_U8G2
  return inverted ? 0 : 1;
#else
  return inverted ? 0 : 1;
#endif
}

inline ColorType bgColor(bool inverted) {
#ifdef SCREEN_BACKEND_U8G2
  return inverted ? 1 : 0;
#else
  return inverted ? 1 : 0;
#endif
}

inline void copyEllipsized(char* dest, size_t cap, const char* src) {
  if (cap == 0) {
    return;
  }
  if (!src) {
    dest[0] = '\0';
    return;
  }
  size_t len = strlen(src);
  if (len < cap) {
    memcpy(dest, src, len);
    dest[len] = '\0';
    return;
  }
  if (cap < 4) {
    size_t copy = cap - 1;
    memcpy(dest, src, copy);
    dest[copy] = '\0';
    return;
  }
  size_t copy = cap - 1;
  memcpy(dest, src, copy - 3);
  dest[copy - 3] = '.';
  dest[copy - 2] = '.';
  dest[copy - 1] = '.';
  dest[copy] = '\0';
}

inline uint16_t alignDownTo(uint16_t value, uint8_t step) {
  return static_cast<uint16_t>((value / step) * step);
}

inline uint16_t alignUpTo(uint16_t value, uint8_t step) {
  if ((value % step) == 0) {
    return value;
  }
  return static_cast<uint16_t>(((value / step) + 1) * step);
}

}  // namespace

static_assert(sizeof(ScreenManager) <= 6000, "ScreenManager footprint exceeds 6KB budget");

ScreenManager::ScreenManager() = default;

bool ScreenManager::begin(screen_config::BackendType& backend) {
  backend_ = &backend;
  screen_config::backendBegin(*backend_);
#ifdef SCREEN_BACKEND_U8G2
  backend_->clearBuffer();
  backend_->sendBuffer();
  backend_->setFontMode(1);
  backend_->setFontDirection(0);
#else
  backend_->setTextWrap(false);
#endif
  reset();
  applyTheme();
  lastActivityMs_ = millis();
  return true;
}

void ScreenManager::reset() {
  for (auto& slot : statusSlots_) {
    slot.icon = screen_config::IconId::None;
    slot.value = 0;
    slot.label[0] = '\0';
    slot.valueText[0] = '\0';
    slot.hasLabel = false;
    slot.dirty = true;
    slot.showValue = false;
    slot.useTextValue = false;
    slot.pendingUpdate = false;
  }
  for (auto& cell : dataCells_) {
    cell.text[0] = '\0';
    cell.dirty = true;
    cell.align = TextAlign::Left;
    cell.lastUpdateMs = lastTickMs_;
    cell.format.units = nullptr;
    cell.format.showUnitsInline = true;
  }
  alert_ = ActiveAlert{};
  resetDirtyRegions();
  forceFullFrame_ = true;
  hasDirty_ = true;
  dimDirty_ = true;
  themeDirty_ = true;
  alertsShownThisLoop_ = 0;
}

void ScreenManager::snapshotStatus(StatusSnapshot* out, size_t maxSlots) const {
  size_t count = std::min(maxSlots, statusSlots_.size());
  for (size_t i = 0; i < count; ++i) {
    out[i].icon = statusSlots_[i].icon;
    out[i].value = statusSlots_[i].value;
    strncpy(out[i].label, statusSlots_[i].label, sizeof(out[i].label) - 1);
    out[i].label[sizeof(out[i].label) - 1] = '\0';
    strncpy(out[i].valueText, statusSlots_[i].valueText, sizeof(out[i].valueText) - 1);
    out[i].valueText[sizeof(out[i].valueText) - 1] = '\0';
  }
}

bool ScreenManager::validStatus(uint8_t slot) const {
  return slot < statusSlots_.size();
}

bool ScreenManager::validData(uint8_t row, uint8_t col) const {
  return row < screen_config::kDataRows && col < screen_config::kDataCols;
}

uint8_t ScreenManager::dataIndex(uint8_t row, uint8_t col) const {
  return static_cast<uint8_t>(row * screen_config::kDataCols + col);
}

void ScreenManager::setLabel(StatusSlot& slot, const char* label) {
  if (label && label[0]) {
    copyEllipsized(slot.label, sizeof(slot.label), label);
    slot.hasLabel = true;
  } else {
    slot.label[0] = '\0';
    slot.hasLabel = false;
  }
}

bool ScreenManager::setStatusBox(uint8_t slot,
                                 screen_config::IconId icon,
                                 int16_t value,
                                 const char* label) {
  if (!validStatus(slot)) {
    return false;
  }
  StatusSlot& dest = statusSlots_[slot];
  dest.icon = icon;
  if (value > static_cast<int16_t>(screen_config::kStatusValueMax)) {
    value = static_cast<int16_t>(screen_config::kStatusValueMax);
  }
  if (value < static_cast<int16_t>(screen_config::kStatusValueMin)) {
    value = static_cast<int16_t>(screen_config::kStatusValueMin);
  }
  dest.value = value;
  dest.showValue = true;
  dest.useTextValue = false;
  dest.valueText[0] = '\0';
  setLabel(dest, label);
  markStatusDirty(slot);
  touch();
  return true;
}

bool ScreenManager::setStatusValue(uint8_t slot, int16_t value) {
  if (!validStatus(slot)) {
    return false;
  }
  if (value > static_cast<int16_t>(screen_config::kStatusValueMax)) {
    value = static_cast<int16_t>(screen_config::kStatusValueMax);
  }
  if (value < static_cast<int16_t>(screen_config::kStatusValueMin)) {
    value = static_cast<int16_t>(screen_config::kStatusValueMin);
  }
  StatusSlot& dest = statusSlots_[slot];
  if (dest.value == value) {
    return true;
  }
  dest.value = value;
  dest.showValue = true;
  dest.useTextValue = false;
  dest.valueText[0] = '\0';
  dest.dirty = true;
  markStatusDirty(slot);
  touch();
  return true;
}

bool ScreenManager::setStatusValueText(uint8_t slot, const char* text) {
  if (!validStatus(slot)) {
    return false;
  }
  StatusSlot& dest = statusSlots_[slot];
  copyEllipsized(dest.valueText, sizeof(dest.valueText), text ? text : "");
  dest.useTextValue = true;
  dest.showValue = true;
  dest.dirty = true;
  markStatusDirty(slot);
  touch();
  return true;
}

bool ScreenManager::notifyStatusValue(uint8_t slot, int16_t value) {
  if (!validStatus(slot)) {
    return false;
  }
  statusSlots_[slot].pendingValue = value;
  statusSlots_[slot].pendingUpdate = true;
  statusSlots_[slot].showValue = true;
  statusSlots_[slot].useTextValue = false;
  return true;
}

bool ScreenManager::setStatusLabel(uint8_t slot, const char* label) {
  if (!validStatus(slot)) {
    return false;
  }
  setLabel(statusSlots_[slot], label);
  markStatusDirty(slot);
  touch();
  return true;
}

bool ScreenManager::clearStatusLabel(uint8_t slot) {
  if (!validStatus(slot)) {
    return false;
  }
  StatusSlot& s = statusSlots_[slot];
  s.label[0] = '\0';
  s.hasLabel = false;
  markStatusDirty(slot);
  touch();
  return true;
}

bool ScreenManager::setStatusIcon(uint8_t slot, screen_config::IconId icon) {
  if (!validStatus(slot)) {
    return false;
  }
  statusSlots_[slot].icon = icon;
  markStatusDirty(slot);
  touch();
  return true;
}

bool ScreenManager::setStatusValueVisible(uint8_t slot, bool visible) {
  if (!validStatus(slot)) {
    return false;
  }
  StatusSlot& dest = statusSlots_[slot];
  if (dest.showValue == visible) {
    return true;
  }
  dest.showValue = visible;
  markStatusDirty(slot);
  touch();
  return true;
}

bool ScreenManager::setData(uint8_t row, uint8_t col, const String& text) {
  if (!validData(row, col)) {
    return false;
  }
  return setDataRaw(dataIndex(row, col), text.c_str());
}

bool ScreenManager::setDataRaw(uint8_t index, const char* text) {
  if (index >= dataCells_.size()) {
    return false;
  }
  DataCell& cell = dataCells_[index];
  char buffer[screen_config::kDataCellMaxChars + 1];
  copyEllipsized(buffer, sizeof(buffer), text ? text : "");
  if (strcmp(cell.text, buffer) == 0) {
    cell.lastUpdateMs = lastTickMs_;
    return true;
  }
  strncpy(cell.text, buffer, sizeof(cell.text) - 1);
  cell.text[sizeof(cell.text) - 1] = '\0';
  cell.lastUpdateMs = lastTickMs_;
  markDataDirty(index);
  touch();
  return true;
}

bool ScreenManager::setDataAlignment(uint8_t row, uint8_t col, TextAlign align) {
  if (!validData(row, col)) {
    return false;
  }
  DataCell& cell = dataCells_[dataIndex(row, col)];
  if (cell.align == align) {
    return true;
  }
  cell.align = align;
  markDataDirty(dataIndex(row, col));
  return true;
}

bool ScreenManager::setDataFormat(uint8_t row, uint8_t col, const DataFormat& fmt) {
  if (!validData(row, col)) {
    return false;
  }
  DataCell& cell = dataCells_[dataIndex(row, col)];
  cell.format = fmt;
  markDataDirty(dataIndex(row, col));
  return true;
}

void ScreenManager::clearData() {
  for (size_t i = 0; i < dataCells_.size(); ++i) {
    if (dataCells_[i].text[0] != '\0') {
      dataCells_[i].text[0] = '\0';
      markDataDirty(static_cast<uint8_t>(i));
    }
  }
}

void ScreenManager::setDataAreaEnabled(bool enabled) {
  if (dataAreaEnabled_ == enabled) {
    return;
  }
  dataAreaEnabled_ = enabled;
  if (customRenderer_ && !enabled) {
    markRectDirty(screen_config::kDataAreaRegion);
  }
  if (enabled) {
    for (uint8_t i = 0; i < dataCells_.size(); ++i) {
      dataCells_[i].dirty = true;
      markDataDirty(i);
    }
  } else {
    for (auto& cell : dataCells_) {
      cell.dirty = false;
    }
  }
  touch();
}

void ScreenManager::setCustomRenderer(CustomRenderer renderer, void* userData) {
  customRenderer_ = renderer;
  customRendererUser_ = userData;
  if (customRenderer_) {
    touch();
  }
}

void ScreenManager::markRectDirty(const screen_config::Rect& rect) {
  trackDirtyRect(rect);
  hasDirty_ = true;
}

void ScreenManager::markAllDirty() {
  for (uint8_t i = 0; i < statusSlots_.size(); ++i) {
    statusSlots_[i].dirty = true;
    markStatusDirty(i);
  }
  for (uint8_t i = 0; i < dataCells_.size(); ++i) {
    dataCells_[i].dirty = true;
    markDataDirty(i);
  }
  if (alert_.active) {
    alert_.dirty = true;
    trackDirtyRect(screen_config::kAlertRegion);
  }
  forceFullFrame_ = true;
}

void ScreenManager::markStatusDirty(uint8_t slot) {
  if (!validStatus(slot)) {
    return;
  }
  statusSlots_[slot].dirty = true;
  const auto& layout = screen_config::kStatusBoxes[slot];
  screen_config::Rect rect{layout.x, layout.y, layout.w, layout.h};
  trackDirtyRect(rect);
}

void ScreenManager::markDataDirty(uint8_t index) {
  if (index >= dataCells_.size()) {
    return;
  }
  if (!dataAreaEnabled_) {
    return;
  }
  dataCells_[index].dirty = true;
  const auto& layout = screen_config::kGridCells[index];
  screen_config::Rect rect{layout.x, layout.y, layout.w, layout.h};
  trackDirtyRect(rect);
}

void ScreenManager::trackDirtyRect(const screen_config::Rect& rect) {
  screen_config::Rect clipped = rect;
  if (clipped.x < 0) clipped.x = 0;
  if (clipped.y < 0) clipped.y = 0;
  if (clipped.x + clipped.w > screen_config::kScreenWidth) {
    clipped.w = static_cast<int16_t>(screen_config::kScreenWidth - clipped.x);
  }
  if (clipped.y + clipped.h > screen_config::kScreenHeight) {
    clipped.h = static_cast<int16_t>(screen_config::kScreenHeight - clipped.y);
  }

  if (screen_config::kBackendSupportsPartialFlush) {
    uint16_t xAligned = alignDownTo(static_cast<uint16_t>(clipped.x), screen_config::kTilePixelSpan);
    uint16_t wAligned = alignUpTo(static_cast<uint16_t>(clipped.w + (clipped.x - xAligned)), screen_config::kTilePixelSpan);
    clipped.x = static_cast<int16_t>(xAligned);
    clipped.w = static_cast<int16_t>(wAligned);
    clipped.y = static_cast<int16_t>(alignDownTo(static_cast<uint16_t>(clipped.y), screen_config::kTilePixelSpan));
    clipped.h = static_cast<int16_t>(alignUpTo(static_cast<uint16_t>(clipped.h), screen_config::kTilePixelSpan));
  }

  for (auto& region : dirtyRegions_) {
    if (!region.active) {
      region.rect = clipped;
      region.active = true;
      hasDirty_ = true;
      return;
    }
  }

  // If we reach here we ran out of slots; merge into first region.
  dirtyRegions_[0].rect.x = std::min(dirtyRegions_[0].rect.x, clipped.x);
  dirtyRegions_[0].rect.y = std::min(dirtyRegions_[0].rect.y, clipped.y);
  int16_t x2 = std::max<int16_t>(dirtyRegions_[0].rect.x + dirtyRegions_[0].rect.w, clipped.x + clipped.w);
  int16_t y2 = std::max<int16_t>(dirtyRegions_[0].rect.y + dirtyRegions_[0].rect.h, clipped.y + clipped.h);
  dirtyRegions_[0].rect.w = x2 - dirtyRegions_[0].rect.x;
  dirtyRegions_[0].rect.h = y2 - dirtyRegions_[0].rect.y;
  dirtyRegions_[0].active = true;
#if defined(SCREEN_MANAGER_METRICS)
  metrics_.dirtyMerges++;
#endif
  hasDirty_ = true;
}

void ScreenManager::mergeDirtyRegions() {
  for (size_t i = 0; i < dirtyRegions_.size(); ++i) {
    if (!dirtyRegions_[i].active) continue;
    for (size_t j = i + 1; j < dirtyRegions_.size(); ++j) {
      if (!dirtyRegions_[j].active) continue;
      const auto& a = dirtyRegions_[i].rect;
      const auto& b = dirtyRegions_[j].rect;
      bool overlap = !(b.x >= a.x + a.w || b.x + b.w <= a.x ||
                       b.y >= a.y + a.h || b.y + b.h <= a.y);
      if (overlap) {
        int16_t left = std::min(a.x, b.x);
        int16_t top = std::min(a.y, b.y);
        int16_t right = std::max<int16_t>(a.x + a.w, b.x + b.w);
        int16_t bottom = std::max<int16_t>(a.y + a.h, b.y + b.h);
        dirtyRegions_[i].rect.x = left;
        dirtyRegions_[i].rect.y = top;
        dirtyRegions_[i].rect.w = right - left;
        dirtyRegions_[i].rect.h = bottom - top;
        dirtyRegions_[j].active = false;
#if defined(SCREEN_MANAGER_METRICS)
        metrics_.dirtyMerges++;
#endif
      }
    }
  }
}

void ScreenManager::resetDirtyRegions() {
  for (auto& region : dirtyRegions_) {
    region.active = false;
    region.rect = screen_config::Rect{0, 0, 0, 0};
  }
}

bool ScreenManager::drawStatusBar() {
  if (!backend_) {
    return false;
  }
  bool drawn = false;
  for (uint8_t i = 0; i < statusSlots_.size(); ++i) {
    if (forceFullFrame_ || statusSlots_[i].dirty) {
      drawStatusSlot(i);
      statusSlots_[i].dirty = false;
      drawn = true;
    }
  }
  return drawn;
}

bool ScreenManager::drawDataArea() {
  if (!backend_) {
    return false;
  }
  if (!dataAreaEnabled_) {
    return false;
  }
  bool drawn = false;
  for (uint8_t i = 0; i < dataCells_.size(); ++i) {
    if (forceFullFrame_ || dataCells_[i].dirty) {
      drawDataCell(i);
      dataCells_[i].dirty = false;
      drawn = true;
    }
  }
  return drawn;
}

void ScreenManager::clearRegion(const screen_config::Rect& rect) {
  if (!backend_) return;
#ifdef SCREEN_BACKEND_U8G2
  backend_->setDrawColor(bgColor(themeInverted_));
  backend_->drawBox(rect.x, rect.y, rect.w, rect.h);
  backend_->setDrawColor(fgColor(themeInverted_));
#else
  backend_->fillRect(rect.x, rect.y, rect.w, rect.h, bgColor(themeInverted_));
#endif
}

void ScreenManager::drawStatusSlot(uint8_t slot) {
  const auto& layout = screen_config::kStatusBoxes[slot];
  screen_config::Rect rect{layout.x, layout.y, layout.w, layout.h};
  clearRegion(rect);

#ifdef SCREEN_BACKEND_U8G2
  if (showStatusFrames_) {
    backend_->drawFrame(layout.x, layout.y, layout.w, layout.h);
  }
#else
  if (showStatusFrames_) {
    backend_->drawRect(layout.x, layout.y, layout.w, layout.h, fgColor(themeInverted_));
  }
#endif

  const StatusSlot& data = statusSlots_[slot];
  int16_t cursorX = layout.x + screen_config::kStatusBoxPaddingH;
  const screen_config::IconInfo* iconInfo = screen_config::iconInfo(data.icon);
  uint8_t iconW = iconInfo ? iconInfo->height : 0;  // rotated width
  uint8_t iconH = iconInfo ? iconInfo->width : 0;   // rotated height
  bool hasIcon = data.icon != screen_config::IconId::None;
  bool hasValue = data.showValue;
  bool useText = data.useTextValue;
  bool hasLabel = data.hasLabel;

  if (hasIcon && !hasValue && !hasLabel) {
    int16_t iconX = layout.x + layout.w - iconW - screen_config::kStatusBoxPaddingH;
    if (iconX < layout.x + screen_config::kStatusBoxPaddingH) {
      iconX = layout.x + screen_config::kStatusBoxPaddingH;
    }
    int16_t iconY = layout.y + (layout.h - iconH) / 2;
    drawIcon(data.icon, iconX, iconY);
    return;
  }

  if (hasIcon) {
    int16_t iconY = layout.y + (layout.h - iconH) / 2;
    drawIcon(data.icon, cursorX, iconY);
    cursorX += iconW + screen_config::kStatusIconValueGap;
  }

  if (hasValue) {
    const char* text = nullptr;
    char valueBuf[screen_config::kStatusValueBufferLen];
    if (useText) {
      text = data.valueText;
    } else {
      snprintf(valueBuf, sizeof(valueBuf), "%d", static_cast<int>(data.value));
      text = valueBuf;
    }
    int16_t valueBaseline = layout.y + screen_config::kStatusValueBaselineOffset;
    setFrontendFont(screen_config::kFonts.statusFont);
#ifdef SCREEN_BACKEND_ADAFRUIT
    backend_->setCursor(cursorX, valueBaseline);
    backend_->print(text);
#else
    backend_->drawUTF8(cursorX, valueBaseline, text);
#endif
    cursorX += static_cast<int16_t>(screen_config::backendTextWidth(*backend_, text)) + screen_config::kStatusLabelValueGap;
  }

  if (hasLabel) {
    setFrontendFont(screen_config::kFonts.statusLabelFont);
    int16_t labelBaseline = layout.y + screen_config::kStatusLabelBaselineOffset;
#ifdef SCREEN_BACKEND_ADAFRUIT
    backend_->setCursor(cursorX, labelBaseline);
    backend_->print(data.label);
#else
    backend_->drawUTF8(cursorX, labelBaseline, data.label);
#endif
  }
}

void ScreenManager::drawDataCell(uint8_t index) {
  const auto& layout = screen_config::kGridCells[index];
  screen_config::Rect rect{layout.x, layout.y, layout.w, layout.h};
  clearRegion(rect);
#ifdef SCREEN_BACKEND_U8G2
  if (showDataFrames_) {
    backend_->drawFrame(layout.x, layout.y, layout.w, layout.h);
  }
#else
  if (showDataFrames_) {
    backend_->drawRect(layout.x, layout.y, layout.w, layout.h, fgColor(themeInverted_));
  }
#endif

  const DataCell& cell = dataCells_[index];
  if (cell.text[0] == '\0' && (!cell.format.units || !cell.format.units[0])) {
    return;
  }

  char buffer[screen_config::kDataCellMaxChars + screen_config::kDataScratchPadding];
  strncpy(buffer, cell.text, sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = '\0';

  if (cell.format.units && cell.format.showUnitsInline) {
    size_t len = strlen(buffer);
    size_t unitsLen = strlen(cell.format.units);
    if (len + 1 + unitsLen < sizeof(buffer)) {
      if (len > 0) {
        buffer[len] = ' ';
        buffer[len + 1] = '\0';
        strncat(buffer, cell.format.units, sizeof(buffer) - len - 2);
      } else {
        strncat(buffer, cell.format.units, sizeof(buffer) - 1);
      }
    }
  }

  setFrontendFont(screen_config::kFonts.dataFont);
  int16_t baseline = layout.y + layout.h - screen_config::kDataCellPaddingV;
  uint16_t width = screen_config::backendTextWidth(*backend_, buffer);
  int16_t textX = layout.x + screen_config::kDataCellPaddingH;
  if (cell.align == TextAlign::Center) {
    textX = layout.x + (layout.w - width) / 2;
  } else if (cell.align == TextAlign::Right) {
    textX = layout.x + layout.w - screen_config::kDataCellPaddingH - width;
  }
  if (textX < layout.x + screen_config::kDataCellPaddingH) {
    textX = layout.x + screen_config::kDataCellPaddingH;
  }

#ifdef SCREEN_BACKEND_ADAFRUIT
  backend_->setCursor(textX, baseline);
  backend_->print(buffer);
#else
  backend_->drawUTF8(textX, baseline, buffer);
#endif

  if (cell.format.units && !cell.format.showUnitsInline) {
    setFrontendFont(screen_config::kFonts.statusLabelFont);
    int16_t unitBaseline = layout.y + screen_config::kDataCellPaddingV + screen_config::kStatusLabelBaselineOffset;
    int16_t unitX = layout.x + layout.w - screen_config::kStatusBoxPaddingH - screen_config::backendTextWidth(*backend_, cell.format.units);
#ifdef SCREEN_BACKEND_ADAFRUIT
    backend_->setCursor(unitX, unitBaseline);
    backend_->print(cell.format.units);
#else
    backend_->drawUTF8(unitX, unitBaseline, cell.format.units);
#endif
  }

  uint32_t age = lastTickMs_ >= cell.lastUpdateMs ? lastTickMs_ - cell.lastUpdateMs : 0;
  if (age > screen_config::kDataStaleMs) {
    const auto* dotInfo = screen_config::iconInfo(screen_config::IconId::Dot);
    if (dotInfo) {
      uint8_t dotW = dotInfo->height;
      uint8_t dotH = dotInfo->width;
      int16_t dotX = layout.x + layout.w - screen_config::kDataCellPaddingH - dotW;
      int16_t dotY = layout.y + (layout.h - dotH) / 2;
      drawIcon(screen_config::IconId::Dot, dotX, dotY);
    }
  }
}

void ScreenManager::drawIcon(screen_config::IconId icon, int16_t x, int16_t y) {
  if (!backend_) return;
  const auto* info = screen_config::iconInfo(icon);
  if (!info || !info->bitmap) {
    return;
  }
  uint8_t srcW = info->width;
  uint8_t srcH = info->height;
  uint8_t byteWidth = static_cast<uint8_t>((srcW + 7) / 8);

  for (uint8_t sy = 0; sy < srcH; ++sy) {
    for (uint8_t sx = 0; sx < srcW; ++sx) {
      uint8_t byte = pgm_read_byte(info->bitmap + sy * byteWidth + (sx >> 3));
      bool set = byte & (0x80 >> (sx & 0x07));
      if (!set) {
        continue;
      }
      int16_t dx = x + sy;
      int16_t dy = y + (srcW - 1 - sx);
#ifdef SCREEN_BACKEND_U8G2
      backend_->drawPixel(dx, dy);
#else
      backend_->drawPixel(dx, dy, fgColor(themeInverted_));
#endif
    }
  }
}

bool ScreenManager::showAlert(const AlertCfg& cfg) {
  if (alertsShownThisLoop_ >= alertRateLimit_) {
    return false;
  }
  alert_.cfg = cfg;
  alert_.active = true;
  alert_.dirty = true;
  alert_.fadeOut = false;
  alert_.fadeStep = 0;
  alert_.startMs = lastTickMs_;
  alert_.deadlineMs = lastTickMs_ + cfg.durationMs;
  alertsShownThisLoop_++;
  trackDirtyRect(screen_config::kAlertRegion);
  touch();
#if defined(SCREEN_MANAGER_METRICS)
  metrics_.alertsShown++;
#endif
  return true;
}

void ScreenManager::clearAlert() {
  if (!alert_.active) {
    return;
  }
  alert_.active = false;
  alert_.dirty = true;
  trackDirtyRect(screen_config::kAlertRegion);
}

void ScreenManager::drawAlertOverlay() {
  const auto& region = screen_config::kAlertRegion;
  clearRegion(region);

#ifdef SCREEN_BACKEND_U8G2
  backend_->drawFrame(region.x, region.y, region.w, region.h);
#else
  backend_->drawRect(region.x, region.y, region.w, region.h, fgColor(themeInverted_));
#endif

  screen_config::IconId icon = alert_.cfg.iconOverride != screen_config::IconId::None
                                   ? alert_.cfg.iconOverride
                                   : screen_config::kAlertIcons[static_cast<uint8_t>(alert_.cfg.level)];
  int16_t iconX = region.x + screen_config::kAlertPaddingX;
  int16_t iconY = region.y + screen_config::kAlertPaddingY;
  drawIcon(icon, iconX, iconY);

  setFrontendFont(screen_config::kFonts.alertTitleFont);
  int16_t titleBaseline = region.y + screen_config::kAlertTitleBaselineOffset;
  if (alert_.cfg.title) {
#ifdef SCREEN_BACKEND_ADAFRUIT
    backend_->setCursor(region.x + screen_config::kAlertPaddingX + screen_config::kAlertIconSize + screen_config::kAlertIconTextGap, titleBaseline);
    backend_->print(alert_.cfg.title);
#else
    backend_->drawUTF8(region.x + screen_config::kAlertPaddingX + screen_config::kAlertIconSize + screen_config::kAlertIconTextGap,
                       titleBaseline,
                       alert_.cfg.title);
#endif
  }

  if (alert_.cfg.detail) {
    setFrontendFont(screen_config::kFonts.alertDetailFont);
    int16_t detailBaseline = region.y + screen_config::kAlertDetailBaselineOffset;
#ifdef SCREEN_BACKEND_ADAFRUIT
    backend_->setCursor(region.x + screen_config::kAlertPaddingX, detailBaseline);
    backend_->print(alert_.cfg.detail);
#else
    backend_->drawUTF8(region.x + screen_config::kAlertPaddingX,
                       detailBaseline,
                       alert_.cfg.detail);
#endif
  }
}

void ScreenManager::setAlertRateLimit(uint16_t maxPerLoop) {
  alertRateLimit_ = maxPerLoop;
}

void ScreenManager::setDim(uint8_t level) {
  updateDimLevel(level);
  autoDimEnabled_ = false;
}

void ScreenManager::sleepDisplay(bool sleep) {
  if (sleep == displaySleeping_) {
    return;
  }
  displaySleeping_ = sleep;
  if (backend_) {
    screen_config::backendSleep(*backend_, sleep);
  }
}

void ScreenManager::setThemeInverted(bool inverted) {
  if (themeInverted_ == inverted) {
    return;
  }
  themeInverted_ = inverted;
  themeDirty_ = true;
  markAllDirty();
}

void ScreenManager::toggleTheme() {
  setThemeInverted(!themeInverted_);
}

void ScreenManager::setAutoDimEnabled(bool enabled) {
  autoDimEnabled_ = enabled;
}

void ScreenManager::setAutoSleepEnabled(bool enabled) {
  autoSleepEnabled_ = enabled;
}

void ScreenManager::suspendRefresh(bool suspend) {
  refreshSuspended_ = suspend;
}

void ScreenManager::setMaxFps(uint8_t fps) {
  if (fps == 0) {
    return;
  }
  maxFps_ = fps;
  minFrameIntervalMs_ = static_cast<uint16_t>(1000 / std::max<uint8_t>(1, maxFps_));
}

void ScreenManager::setMinFrameInterval(uint16_t ms) {
  if (ms == 0) {
    return;
  }
  minFrameIntervalMs_ = ms;
}

void ScreenManager::setFrameIdleInterval(uint16_t ms) {
  if (ms == 0) {
    return;
  }
  idleFrameIntervalMs_ = ms;
}

void ScreenManager::touch() {
  uint32_t now = lastTickMs_;
  if (now == 0) {
    now = millis();
  }
  lastActivityMs_ = now;
  if (displaySleeping_ && autoSleepEnabled_) {
    sleepDisplay(false);
    dimDirty_ = true;
  }
}

void ScreenManager::applyPendingStatus() {
  for (uint8_t i = 0; i < statusSlots_.size(); ++i) {
    StatusSlot& slot = statusSlots_[i];
    if (slot.pendingUpdate) {
      int16_t value = slot.pendingValue;
      slot.pendingUpdate = false;
      if (value > static_cast<int16_t>(screen_config::kStatusValueMax)) {
        value = static_cast<int16_t>(screen_config::kStatusValueMax);
      }
      if (value < static_cast<int16_t>(screen_config::kStatusValueMin)) {
        value = static_cast<int16_t>(screen_config::kStatusValueMin);
      }
      if (slot.value != value) {
        slot.value = value;
        slot.dirty = true;
        markStatusDirty(i);
      }
      slot.useTextValue = false;
      slot.valueText[0] = '\0';
      slot.showValue = true;
    }
  }
}

void ScreenManager::updatePowerState(uint32_t nowMs) {
  if (autoSleepEnabled_) {
    if (!displaySleeping_ && nowMs - lastActivityMs_ > screen_config::kInactivitySleepTimeoutMs) {
      sleepDisplay(true);
    } else if (displaySleeping_ && nowMs - lastActivityMs_ <= screen_config::kInactivitySleepTimeoutMs) {
      sleepDisplay(false);
    }
  }

  if (autoDimEnabled_) {
    if (nowMs - lastActivityMs_ > screen_config::kInactivityDimTimeoutMs) {
      updateDimLevel(screen_config::kDimIdleLevel);
    } else {
      updateDimLevel(screen_config::kDimActiveLevel);
    }
  }
}

void ScreenManager::updateDimLevel(uint8_t level) {
  if (currentDimLevel_ == level) {
    return;
  }
  currentDimLevel_ = level;
  if (backend_) {
    screen_config::backendSetContrast(*backend_, level);
    dimDirty_ = false;
  } else {
    dimDirty_ = true;
  }
}

void ScreenManager::updateAlertState(uint32_t nowMs) {
  if (!alert_.active) {
    return;
  }
  if (!alert_.cfg.sticky && nowMs >= alert_.deadlineMs) {
    alert_.active = false;
    alert_.dirty = true;
    trackDirtyRect(screen_config::kAlertRegion);
    return;
  }
  if (alert_.cfg.sticky && alert_.fadeOut && alert_.fadeStep >= screen_config::kAlertFadeSteps) {
    alert_.active = false;
    alert_.dirty = true;
    trackDirtyRect(screen_config::kAlertRegion);
  }
}

bool ScreenManager::tick(uint32_t nowMs) {
  lastTickMs_ = nowMs;
  alertsShownThisLoop_ = 0;
  applyPendingStatus();
  updatePowerState(nowMs);
  updateAlertState(nowMs);
  return hasDirty_ || forceFullFrame_ || alert_.dirty;
}

void ScreenManager::setFrontendFont(screen_config::FontHandle font) {
  if (!backend_) return;
  screen_config::backendSetFont(*backend_, font);
}

void ScreenManager::applyTheme() {
  if (!backend_) {
    return;
  }
#ifdef SCREEN_BACKEND_ADAFRUIT
  backend_->setTextColor(fgColor(themeInverted_), bgColor(themeInverted_));
#else
  backend_->setDrawColor(fgColor(themeInverted_));
#endif
  themeDirty_ = false;
}

void ScreenManager::setStatusFramesEnabled(bool enabled) {
  if (showStatusFrames_ == enabled) {
    return;
  }
  showStatusFrames_ = enabled;
  for (uint8_t i = 0; i < statusSlots_.size(); ++i) {
    markStatusDirty(i);
  }
  touch();
}

void ScreenManager::setDataFramesEnabled(bool enabled) {
  if (showDataFrames_ == enabled) {
    return;
  }
  showDataFrames_ = enabled;
  for (uint8_t i = 0; i < dataCells_.size(); ++i) {
    markDataDirty(i);
  }
  touch();
}

bool ScreenManager::render(uint32_t nowMs) {
  if (!backend_ || refreshSuspended_) {
    return false;
  }

  uint32_t elapsed = nowMs - lastRenderMs_;
  uint16_t interval = hasDirty_ ? minFrameIntervalMs_ : idleFrameIntervalMs_;
  if (!hasDirty_ && elapsed < interval) {
#if defined(SCREEN_MANAGER_METRICS)
    metrics_.droppedFrames++;
#endif
    return false;
  }

  if (themeDirty_) {
    applyTheme();
  }
  if (dimDirty_) {
    screen_config::backendSetContrast(*backend_, currentDimLevel_);
    dimDirty_ = false;
  }

  forceFullFrame_ = true;
  bool drew = false;
#ifdef SCREEN_MANAGER_METRICS
  uint32_t startUs = micros();
#endif

  if (!alert_.active) {
    drew |= drawStatusBar();
    drew |= drawDataArea();
    if (customRenderer_) {
      customRenderer_(*backend_, customRendererUser_);
      drew = true;
    }
  }
  if (alert_.dirty || alert_.active) {
    drawAlertOverlay();
    alert_.dirty = false;
    drew = true;
  }

  if (!drew) {
    forceFullFrame_ = false;
    return false;
  }

#ifdef SCREEN_BACKEND_U8G2
  backend_->sendBuffer();
#else
  screen_config::backendFinishFrame(*backend_);
#endif

  resetDirtyRegions();
  hasDirty_ = false;
  forceFullFrame_ = false;
  lastRenderMs_ = nowMs;
#ifdef SCREEN_MANAGER_METRICS
  uint32_t duration = micros() - startUs;
  metrics_.framesDrawn++;
  metrics_.maxRenderDurationUs = std::max(metrics_.maxRenderDurationUs, duration);
#endif
  return true;
}

}  // namespace screen_manager
