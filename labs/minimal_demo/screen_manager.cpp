#include "screen_manager.h"
#include <pgmspace.h>

static inline uint16_t pitchAuto(uint16_t w){ return (w+7)>>3; }

void ScreenManagerU8g2::begin(U8G2* driver){
  u8g2_ = driver;
  W_ = u8g2_->getDisplayWidth();
  H_ = u8g2_->getDisplayHeight();
  autoClassFromSize();
  statusSlots_.assign(statusCfg_.sectors, {});
  u8g2_->begin();
  u8g2_->clearBuffer();
  u8g2_->sendBuffer();
}

void ScreenManagerU8g2::autoClassFromSize(){
  if (sclass_ != ScreenClass::Auto) return;
  int area = W_*H_;
  if (area <= 128*64) sclass_ = ScreenClass::Small;
  else if (area <= 240*128) sclass_ = ScreenClass::Mid;
  else sclass_ = ScreenClass::Large;

  switch (sclass_){
    case ScreenClass::Small: statusCfg_.heightMin=12; dataLayout_.fontScale=1; break;
    case ScreenClass::Mid:   statusCfg_.heightMin=13; dataLayout_.fontScale=1; break;
    case ScreenClass::Large: statusCfg_.heightMin=14; dataLayout_.fontScale=2; break;
    default: break;
  }
}

void ScreenManagerU8g2::setStabilityMode(bool on){ stability_=on; }
void ScreenManagerU8g2::setMaxFps(uint8_t fps){ maxFps_ = fps; }
void ScreenManagerU8g2::setContrast(uint8_t c){ u8g2_->setContrast(c); }
void ScreenManagerU8g2::setBrightnessSoft(float f){ softBright_ = constrain(f,0.2f,1.0f); }
void ScreenManagerU8g2::setScreenClass(ScreenClass c){ sclass_=c; autoClassFromSize(); }
void ScreenManagerU8g2::setLayout(const ScreenLayoutConfig& lay){ layout_=lay; statusCfg_=lay.status; dataLayout_=lay.data; }
void ScreenManagerU8g2::setFx(const FxConfig& fx){ fx_=fx; }
void ScreenManagerU8g2::setAlertRule(const AlertRule& r){ alertRule_=r; }

void ScreenManagerU8g2::setStatusItem(uint8_t idx, const StatusItem& item){
  if (idx>=statusSlots_.size()) return;
  statusSlots_[idx]=item;
}
void ScreenManagerU8g2::setDataArray(const std::vector<String>& arr){ dataItems_=arr; }

bool ScreenManagerU8g2::pushAlert(const AlertItem& a){
  uint32_t now=millis();
  if (now - alert_.roundStart > alertRule_.roundMs){
    alert_.roundStart=now;
    memset(alert_.usedPerType,0,sizeof(alert_.usedPerType));
  }
  uint8_t t=(uint8_t)a.type;
  if (alert_.usedPerType[t]>=alertRule_.maxPerTypePerRound) return false;
  if (alert_.q.size()>=alertRule_.maxQueue) alert_.q.erase(alert_.q.begin());
  alert_.usedPerType[t]++;
  alert_.q.push_back(a);
  return true;
}
void ScreenManagerU8g2::clearAlerts(){ alert_.q.clear(); }

void ScreenManagerU8g2::drawIcon(int16_t x,int16_t y,const Icon* ic,uint8_t scale,bool invert){
  if (!ic) return;
  const uint16_t pitch = ic->pitchBytes ? ic->pitchBytes : pitchAuto(ic->w);
  for (uint16_t row=0; row<ic->h; ++row){
    for (uint16_t col=0; col<ic->w; ++col){
      uint8_t b=pgm_read_byte(ic->data + row*pitch + (col>>3));
      bool on=(b & (0x80>>(col&7)));
      if (invert) on=!on;
      if (!on) continue;
      u8g2_->drawPixel(x+col*scale,y+row*scale);
    }
  }
}
void ScreenManagerU8g2::printText(int16_t x,int16_t y,const String& t,uint8_t s){
  u8g2_->setFont(u8g2_font_6x10_tf);
  u8g2_->setFontRefHeightExtendedText();
  u8g2_->setDrawColor(1);
  u8g2_->setFontPosTop();
  u8g2_->drawStr(x,y,t.c_str());
}

void ScreenManagerU8g2::drawStatus(){
  int16_t h=statusCfg_.heightMin;
  if(statusCfg_.showGrid) u8g2_->drawFrame(0,0,W_,h);
  int16_t sw = statusCfg_.autoSlot? W_/statusCfg_.sectors : 8;
  for(uint8_t i=0;i<statusSlots_.size();++i){
    int16_t x=i*sw+2;
    if(!statusSlots_[i].visible) continue;
    if(statusSlots_[i].icon)
      drawIcon(x,1,statusSlots_[i].icon,1,false);
    else if(statusSlots_[i].label.length())
      printText(x,1,statusSlots_[i].label,1);
  }
}

void ScreenManagerU8g2::drawData(){
  const int16_t top=statusCfg_.heightMin;
  const int16_t availH=H_-top;
  const int16_t lineH=(dataLayout_.fontScale*8)+dataLayout_.vGap;
  uint8_t rows=max<uint8_t>(1,availH/max<int16_t>(lineH,8));
  uint8_t cols=(W_>=192)?3:2;
  int16_t cellW=W_/cols;
  int16_t cellH=availH/rows;

  for(uint8_t r=0;r<rows;r++){
    for(uint8_t c=0;c<cols;c++){
      uint16_t idx=r*cols+c;
      if(idx>=dataItems_.size()) continue;
      String text=dataItems_[idx];
      int16_t x=c*cellW+2;
      int16_t y=top+r*cellH+1;
      printText(x,y,text,dataLayout_.fontScale);
    }
  }
}

void ScreenManagerU8g2::drawAlert(uint32_t now){
  if(alert_.showing && now<alert_.until){
    const int16_t top=statusCfg_.heightMin;
    u8g2_->drawBox(0,top,W_,H_-top);
    if(alert_.cur.icon)
      drawIcon(4,top+2,alert_.cur.icon,alert_.cur.iconScale,false);
    printText(24,top+2,alert_.cur.title,2);
    printText(24,top+20,alert_.cur.subtitle,1);
    return;
  }
  if(!alert_.q.empty()){
    alert_.cur=alert_.q.front(); alert_.q.erase(alert_.q.begin());
    alert_.showing=true; alert_.until=now+alert_.cur.durationMs;
  } else alert_.showing=false;
}

void ScreenManagerU8g2::loop(uint32_t now){
  uint32_t minDelta=1000UL/max<uint8_t>(maxFps_,1);
  if(now-lastFrame_<minDelta) return;
  lastFrame_=now;

  u8g2_->clearBuffer();
  drawStatus();
  drawAlert(now);
  if(!alert_.showing) drawData();
  u8g2_->sendBuffer();
}
