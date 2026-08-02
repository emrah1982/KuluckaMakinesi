#include "DisplayManager.h"

// ==================== RENK PALETI (Web arayuz eslesimi) ====================
static const uint16_t COL_BG     = 0x0000;  // Siyah arkaplan
static const uint16_t COL_CARD   = 0x1947;  // #1E293B - Kart
static const uint16_t COL_TEXT   = 0xFFFF;  // Beyaz
static const uint16_t COL_DIM    = 0x9517;  // #94A3B8 - Soluk
static const uint16_t COL_RED    = 0xD924;  // #DC2626
static const uint16_t COL_BLUE   = 0x231D;  // #2563EB
static const uint16_t COL_GREEN  = 0x1509;  // #16A34A
static const uint16_t COL_ORANGE = 0xDBA0;  // #D97706
static const uint16_t COL_CYAN   = 0x05BA;  // #06B6D4
static const uint16_t COL_PURPLE = 0x79DD;  // #7C3AED
static const uint16_t COL_BAR_BG = 0x2104;  // #202020
static const uint16_t COL_YELLOW = 0xFE60;  // #FFD700

// Durum isimleri ve renkleri (index = SystemState enum)
static const char* ST_BADGE[]  = {"BASLA", "TUNE", "CALISIYOR", "DURAKLAT", "TAMAM", "ACIL!"};
static const char* ST_SHORT[]  = {"Basla", "Tune", "Calisiyor", "Duraklat", "Tamam", "ACIL!"};
static const uint16_t ST_CLR[] = {COL_DIM, COL_PURPLE, COL_GREEN, COL_ORANGE, COL_BLUE, COL_RED};

// Sekme isimleri
static const char* TAB_LABEL[] = {"Durum", "Kontrol", "Profil", "Ayar"};

// Layout sabitleri (240x320 portrait, tab bar altta)
static const int SCR_W = 240;
static const int SCR_H = 320;
static const int TAB_H = 30;           // Alt tab bar yuksekligi
static const int PAGE_H = SCR_H - TAB_H; // Icerik alani: 0-289

// Dashboard layout
static const int HDR_Y = 0,   HDR_H = 28;
static const int GAU_Y = 30,  GAU_H = 56;
static const int INF_Y = 88,  INF_H = 46;
static const int OUT_Y = 136, OUT_H = 62;
static const int SNS_Y = 200, SNS_H = 24;
static const int ALM_Y = 226, ALM_H = 38;

// Kontrol sayfasi buton konumlari
static const int BTN_W  = 112;
static const int BTN_H  = 44;
static const int BTN_Y1 = 38;
static const int BTN_Y2 = 88;

// ==================== CONSTRUCTOR & BEGIN ====================

DisplayManager::DisplayManager()
    : _tft()
    , _lastUpdate(0)
    , _lastTouch(0)
    , _tab(TAB_DASH)
    , _forceRedraw(true)
    , _bgDraw(true)
    , _scrollOffset(0)
    , _profileListOpen(false)
    , _pendingProfileIdx(0)
    , _editYear(2026)
    , _editMonth(1)
    , _editDay(1)
    , _editDateInit(false)
{
}

void DisplayManager::getEditedStartDate(uint16_t &year, uint8_t &month, uint8_t &day) const {
    year = _editYear;
    month = _editMonth;
    day = _editDay;
}

void DisplayManager::begin() {
#if (TFT_BL >= 0)
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
#endif

    _tft.init();
    _tft.setRotation(0);  // Portrait: 240x320
    _tft.fillScreen(COL_BG);

    // Dokunmatik kalibrasyon
    // 5. parametre: bit0=swap_XY, bit1=invert_X, bit2=invert_Y
    // ESP32-3248S032 icin 7 (swap+invertX+invertY) genelde dogru
    uint16_t calData[5] = {
        TOUCH_CAL_X_MIN, TOUCH_CAL_X_MAX,
        TOUCH_CAL_Y_MIN, TOUCH_CAL_Y_MAX, 7
    };
    _tft.setTouch(calData);

    _lastUpdate = 0;
    _lastTouch  = 0;
    _tab = TAB_DASH;
    _forceRedraw = true;

    Serial.println("[TOUCH] Dokunmatik baslatildi.");

    // Ekrani temizle ve normal cizime gecis
    _tft.fillScreen(COL_BG);
}

// ==================== DOKUNMATIK ====================

bool DisplayManager::getTouchXY(uint16_t &x, uint16_t &y) {
    // Ham Z basinci oku
    uint16_t z = _tft.getTouchRawZ();
    if (z < 300) return false;

    // Ham koordinatlari oku
    uint16_t rawX, rawY;
    _tft.getTouchRaw(&rawX, &rawY);

    // ESP32-3248S032 rotation=0: swap YOK, invertX + invertY
    int sx = map((long)rawX, TOUCH_CAL_X_MIN, TOUCH_CAL_X_MAX, SCR_W - 1, 0);
    int sy = map((long)rawY, TOUCH_CAL_Y_MIN, TOUCH_CAL_Y_MAX, SCR_H - 1, 0);
    x = constrain(sx, 0, SCR_W - 1);
    y = constrain(sy, 0, SCR_H - 1);

    // Debug
    static unsigned long lastDbg = 0;
    if (millis() - lastDbg > 300) {
        lastDbg = millis();
        Serial.printf("[TOUCH] Z=%d rawX=%d rawY=%d -> x=%d y=%d\n",
                      z, rawX, rawY, x, y);
    }
    return true;
}

bool DisplayManager::touchInRect(uint16_t tx, uint16_t ty,
                                  int rx, int ry, int rw, int rh) {
    return (tx >= rx && tx < rx + rw && ty >= ry && ty < ry + rh);
}

TouchAction DisplayManager::handleTouch(const DisplayData &data) {
    uint16_t tx, ty;
    if (!getTouchXY(tx, ty)) return TOUCH_NONE;

    unsigned long now = millis();
    if (now - _lastTouch < TOUCH_DEBOUNCE_MS) return TOUCH_NONE;
    _lastTouch = now;

    // --- Profil dropdown aciksa TUM ekrani ele al (tab bar dahil) ---
    if (_tab == TAB_CTRL && _profileListOpen) {
        const int itemH  = 36;       // Daha buyuk dokunma alani
        const int hdrH   = 40;       // Ust baslik + yukari ok
        const int ftrH   = 40;       // Alt asagi ok
        const int listY  = hdrH;     // Liste baslangici
        int maxVisible = (SCR_H - hdrH - ftrH) / itemH;  // (320-40-40)/36 = 6
        if (maxVisible > PROFILE_COUNT) maxVisible = PROFILE_COUNT;
        int maxScroll = PROFILE_COUNT - maxVisible;
        if (maxScroll < 0) maxScroll = 0;
        if (_scrollOffset > maxScroll) _scrollOffset = maxScroll;

        int listEndY = listY + maxVisible * itemH;

        // Yukari ok butonuna dokunma (baslik alani, y < hdrH)
        if (ty < hdrH) {
            if (_scrollOffset > 0) {
                _scrollOffset--;
                _forceRedraw = true;
                Serial.printf("[TOUCH] Profil scroll yukari: %d\n", _scrollOffset);
            }
            return TOUCH_NONE;
        }

        // Asagi ok butonuna dokunma (alt alan, y >= listEndY)
        if (ty >= listEndY) {
            if (_scrollOffset < maxScroll) {
                _scrollOffset++;
                _forceRedraw = true;
                Serial.printf("[TOUCH] Profil scroll asagi: %d\n", _scrollOffset);
            }
            return TOUCH_NONE;
        }

        // Liste icerisindeki profillere dokunma
        int idx = (ty - listY) / itemH;
        idx = constrain(idx, 0, maxVisible - 1);
        int realIdx = idx + _scrollOffset;
        if (realIdx >= 0 && realIdx < PROFILE_COUNT) {
            _pendingProfileIdx = (uint8_t)realIdx;
            _profileListOpen = false;
            _scrollOffset = 0;
            _forceRedraw = true;
            Serial.printf("[TOUCH] Profil secildi: idx=%d real=%d ty=%d - %s\n",
                          idx, realIdx, ty, ALL_PROFILES[realIdx]->name);
            return TOUCH_PROFILE_SELECT;
        }

        // Baska bir yere dokunma -> kapat
        _profileListOpen = false;
        _scrollOffset = 0;
        _forceRedraw = true;
        return TOUCH_NONE;
    }

    // --- Tab bar dokunma (Y >= PAGE_H) ---
    if (ty >= PAGE_H) {
        int tabW = SCR_W / TAB_COUNT;
        int tabIdx = tx / tabW;
        if (tabIdx >= 0 && tabIdx < TAB_COUNT) {
            DisplayTab newTab = (DisplayTab)tabIdx;
            if (newTab != _tab) {
                Serial.printf("[TOUCH] Tab degisti: %d -> %d\n", (int)_tab, (int)newTab);
                _tab = newTab;
                _scrollOffset = 0;  // Tab degisince scroll sifirla
                _profileListOpen = false;
                _forceRedraw = true;
            }
        }
        return TOUCH_NONE;
    }

    // --- Kontrol sayfasi butonlari ---
    if (_tab == TAB_CTRL) {

        int lx = 4;
        int rx = SCR_W / 2 + 2;

        // Baslat (sol ust)
        if (touchInRect(tx, ty, lx, BTN_Y1, BTN_W, BTN_H)) {
            Serial.println("[TOUCH] BASLAT butonuna dokunuldu");
            return TOUCH_START;
        }
        // Duraklat (sag ust)
        if (touchInRect(tx, ty, rx, BTN_Y1, BTN_W, BTN_H)) {
            Serial.println("[TOUCH] DURAKLAT butonuna dokunuldu");
            return TOUCH_PAUSE;
        }
        // Devam (sol alt)
        if (touchInRect(tx, ty, lx, BTN_Y2, BTN_W, BTN_H)) {
            Serial.println("[TOUCH] DEVAM butonuna dokunuldu");
            return TOUCH_RESUME;
        }
        // Durdur (sag alt)
        if (touchInRect(tx, ty, rx, BTN_Y2, BTN_W, BTN_H)) {
            Serial.println("[TOUCH] DURDUR butonuna dokunuldu");
            return TOUCH_STOP;
        }

        // "Profil Sec" butonuna dokunma (sag alt, y=256)
        if (touchInRect(tx, ty, SCR_W / 2 + 2, 256, SCR_W / 2 - 4, 28)) {
            _profileListOpen = true;
            _scrollOffset = 0;
            _forceRedraw = true;
            return TOUCH_NONE;
        }
    }

    // --- Profil sayfasi: scroll ---
    if (_tab == TAB_PROF) {
        // Yukari scroll (ust alan, y < 80)
        if (ty < 80 && ty >= 38) {
            if (_scrollOffset > 0) {
                _scrollOffset--;
                _forceRedraw = true;
            }
            return TOUCH_NONE;
        }
        // Asagi scroll (alt alan, y > 220)
        if (ty > 220) {
            _scrollOffset++;
            _forceRedraw = true;
            return TOUCH_NONE;
        }
    }

    // --- Ayar sayfasi scroll ---
    if (_tab == TAB_SET) {
        if (ty < 60) {
            if (_scrollOffset > 0) {
                _scrollOffset--;
                _forceRedraw = true;
            }
            return TOUCH_NONE;
        }
        if (ty > 220) {
            _scrollOffset++;
            _forceRedraw = true;
            return TOUCH_NONE;
        }

        const int edX = 4;
        const int edY = 210;
        const int edW = SCR_W - 8;
        const int edH = 52;
        if (touchInRect(tx, ty, edX, edY, edW, edH)) {
            const int btnW = 28;
            const int btnH = 18;
            const int row1Y = edY + 18;
            const int row2Y = edY + 34;
            const int col1X = edX + 8;
            const int col2X = edX + 78;
            const int col3X = edX + 148;
            const int saveX = edX + edW - 56;
            const int saveY = row2Y;
            const int saveW = 48;
            const int saveH = 18;

            if (touchInRect(tx, ty, col1X, row1Y, btnW, btnH)) {
                if (_editDay > 1) _editDay--;
                _forceRedraw = true;
                return TOUCH_NONE;
            }
            if (touchInRect(tx, ty, col1X + btnW + 2, row1Y, btnW, btnH)) {
                if (_editDay < 31) _editDay++;
                _forceRedraw = true;
                return TOUCH_NONE;
            }

            if (touchInRect(tx, ty, col2X, row1Y, btnW, btnH)) {
                if (_editMonth > 1) _editMonth--;
                _forceRedraw = true;
                return TOUCH_NONE;
            }
            if (touchInRect(tx, ty, col2X + btnW + 2, row1Y, btnW, btnH)) {
                if (_editMonth < 12) _editMonth++;
                _forceRedraw = true;
                return TOUCH_NONE;
            }

            if (touchInRect(tx, ty, col3X, row1Y, btnW, btnH)) {
                if (_editYear > 2000) _editYear--;
                _forceRedraw = true;
                return TOUCH_NONE;
            }
            if (touchInRect(tx, ty, col3X + btnW + 2, row1Y, btnW, btnH)) {
                if (_editYear < 2099) _editYear++;
                _forceRedraw = true;
                return TOUCH_NONE;
            }

            if (touchInRect(tx, ty, saveX, saveY, saveW, saveH)) {
                _forceRedraw = true;
                return TOUCH_SET_START_DATE;
            }
        }
    }

    // --- Dashboard alarm sifirla (alarm kartina dokunma) ---
    if (_tab == TAB_DASH) {
        if (touchInRect(tx, ty, 2, ALM_Y, SCR_W - 4, ALM_H) && data.alarmActive) {
            return TOUCH_SAFETY_RESET;
        }
    }

    return TOUCH_NONE;
}

// ==================== UPDATE ====================

TouchAction DisplayManager::update(const DisplayData &data) {
    // Dokunma her zaman kontrol et (ekran guncelleme beklemeden)
    TouchAction action = handleTouch(data);

    unsigned long now = millis();
    if (!_forceRedraw && _lastUpdate != 0 && (now - _lastUpdate) < DISPLAY_UPDATE_MS) {
        return action;
    }
    _lastUpdate = now;

    _bgDraw = _forceRedraw;
    if (_forceRedraw) {
        _tft.fillRect(0, 0, SCR_W, PAGE_H, COL_BG);
        _forceRedraw = false;
    }

    draw(data);
    return action;
}

void DisplayManager::draw(const DisplayData &data) {
    drawTabBar();

    switch (_tab) {
        case TAB_DASH: drawDashboard(data); break;
        case TAB_CTRL: drawControl(data);   break;
        case TAB_PROF: drawProfile(data);   break;
        case TAB_SET:  drawSettings(data);  break;
        default:       drawDashboard(data); break;
    }
}

// ==================== TAB BAR (ALTTA) ====================

void DisplayManager::drawTabBar() {
    int tabW = SCR_W / TAB_COUNT;
    int by = PAGE_H;

    if (_bgDraw) {
        _tft.fillRect(0, by, SCR_W, TAB_H, COL_CARD);
        _tft.drawFastHLine(0, by, SCR_W, COL_DIM);
    }

    for (int i = 0; i < TAB_COUNT; i++) {
        int bx = i * tabW;
        bool active = (i == (int)_tab);

        _tft.setTextFont(2);
        _tft.setTextSize(1);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextPadding(0);

        if (active) {
            _tft.fillRect(bx + 1, by + 1, tabW - 2, TAB_H - 1, COL_BG);
            _tft.setTextColor(COL_BLUE, COL_BG);
            // Aktif gosterge cizgisi
            _tft.fillRect(bx + 4, by + 1, tabW - 8, 2, COL_BLUE);
        } else {
            _tft.setTextColor(COL_DIM, COL_CARD);
        }

        uint16_t bg = active ? COL_BG : COL_CARD;
        _tft.setTextColor(active ? COL_BLUE : COL_DIM, bg);
        _tft.drawString(TAB_LABEL[i], bx + tabW / 2, by + TAB_H / 2 + 4);
    }
}

// ==================== SAYFA: DASHBOARD ====================

void DisplayManager::drawDashboard(const DisplayData &data) {
    drawHeader(data);
    drawGauges(data);
    drawInfoRow(data);
    drawOutputs(data);
    drawSensors(data);
    drawAlarm(data);
}

void DisplayManager::drawHeader(const DisplayData &data) {
    _tft.fillRect(0, HDR_Y, SCR_W, HDR_H, COL_CARD);

    _tft.setTextFont(2);
    _tft.setTextSize(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(0);
    _tft.drawString(data.profileName ? data.profileName : "---", 5, HDR_Y + 1);

    char sub[40];
    int rd = data.remainingDays < 0 ? 0 : data.remainingDays;
    if (data.startDate.length() > 0 && data.hatchDate.length() > 0) {
        snprintf(sub, sizeof(sub), "Gun %d/%d | B:%s C:%s",
                 data.currentDay, data.totalDays, data.startDate.c_str(), data.hatchDate.c_str());
    } else {
        snprintf(sub, sizeof(sub), "Gun %d/%d | %s | %dg",
                 data.currentDay, data.totalDays,
                 data.phaseName ? data.phaseName : "---", rd);
    }
    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString(sub, 5, HDR_Y + 18);

    int si = constrain(data.systemState, 0, 5);
    const char* sn = ST_BADGE[si];
    uint16_t sc = ST_CLR[si];
    _tft.setTextFont(1);
    int tw = _tft.textWidth(sn);
    int bw = tw + 12;
    int bx = SCR_W - bw - 4;
    _tft.fillRoundRect(bx, HDR_Y + 4, bw, 16, 4, sc);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_TEXT, sc);
    _tft.drawString(sn, bx + bw / 2, HDR_Y + 12);
}

void DisplayManager::drawGauges(const DisplayData &data) {
    const int gw = 115;
    int lx = 2;
    int rx = SCR_W - gw - 2;

    // Sicaklik
    if (_bgDraw) {
        _tft.fillRoundRect(lx, GAU_Y, gw, GAU_H, 4, COL_CARD);
        _tft.fillRect(lx + 1, GAU_Y + 4, 3, GAU_H - 8, COL_RED);
    }

    _tft.setTextFont(4);
    _tft.setTextSize(1);
    _tft.setTextDatum(TC_DATUM);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(_tft.textWidth("88.8"));
    char ts[8];
    dtostrf(data.temperature, 4, 1, ts);
    _tft.drawString(ts, lx + gw / 2, GAU_Y + 4);

    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(0);
    _tft.drawString("C SICAKLIK", lx + gw / 2, GAU_Y + 34);
    char tt[16];
    snprintf(tt, sizeof(tt), "Hedef: %.1f", data.targetTemp);
    _tft.drawString(tt, lx + gw / 2, GAU_Y + 44);

    // Nem
    if (_bgDraw) {
        _tft.fillRoundRect(rx, GAU_Y, gw, GAU_H, 4, COL_CARD);
        _tft.fillRect(rx + 1, GAU_Y + 4, 3, GAU_H - 8, COL_BLUE);
    }

    _tft.setTextFont(4);
    _tft.setTextDatum(TC_DATUM);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(_tft.textWidth("88.8"));
    char hs[8];
    dtostrf(data.humidity, 4, 1, hs);
    _tft.drawString(hs, rx + gw / 2, GAU_Y + 4);

    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(0);
    _tft.drawString("% NEM", rx + gw / 2, GAU_Y + 34);
    char ht[16];
    snprintf(ht, sizeof(ht), "%%%d - %%%d", (int)data.targetHumLow, (int)data.targetHumHigh);
    _tft.drawString(ht, rx + gw / 2, GAU_Y + 44);
}

void DisplayManager::drawInfoRow(const DisplayData &data) {
    const int bw = 76;
    const int gap = (SCR_W - 3 * bw - 4) / 2;
    int x1 = 2, x2 = x1 + bw + gap, x3 = x2 + bw + gap;

    // Evre
    if (_bgDraw) _tft.fillRoundRect(x1, INF_Y, bw, INF_H, 4, COL_CARD);
    _tft.setTextFont(1);
    _tft.setTextSize(1);
    _tft.setTextDatum(TC_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(0);
    _tft.drawString("EVRE", x1 + bw / 2, INF_Y + 3);
    _tft.setTextFont(2);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(bw - 4);
    _tft.drawString(data.phaseName ? data.phaseName : "---", x1 + bw / 2, INF_Y + 14);
    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(bw - 8);
    char p1[16];
    snprintf(p1, sizeof(p1), "Bitis: %dg", data.phaseRemainingDays);
    _tft.drawString(p1, x1 + bw / 2, INF_Y + 35);
    _tft.setTextPadding(0);

    // Kalan
    if (_bgDraw) _tft.fillRoundRect(x2, INF_Y, bw, INF_H, 4, COL_CARD);
    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("KALAN", x2 + bw / 2, INF_Y + 3);
    _tft.setTextFont(2);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(bw - 4);
    int rem = data.remainingDays < 0 ? 0 : data.remainingDays;
    char r1[12];
    snprintf(r1, sizeof(r1), "%d gun", rem);
    _tft.drawString(r1, x2 + bw / 2, INF_Y + 14);
    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(bw - 8);
    char d1[12];
    snprintf(d1, sizeof(d1), "Gun %d/%d", data.currentDay, data.totalDays);
    _tft.drawString(d1, x2 + bw / 2, INF_Y + 35);
    _tft.setTextPadding(0);

    // Durum
    if (_bgDraw) _tft.fillRoundRect(x3, INF_Y, bw, INF_H, 4, COL_CARD);
    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("DURUM", x3 + bw / 2, INF_Y + 3);
    int si = constrain(data.systemState, 0, 5);
    _tft.setTextFont(2);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(bw - 4);
    _tft.drawString(ST_SHORT[si], x3 + bw / 2, INF_Y + 14);
    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(0);
    _tft.drawString(data.profileName ? data.profileName : "---", x3 + bw / 2, INF_Y + 35);
}

void DisplayManager::drawOutputs(const DisplayData &data) {
    if (_bgDraw) _tft.fillRoundRect(2, OUT_Y, SCR_W - 4, OUT_H, 4, COL_CARD);

    const int lblX = 8;
    const int barX = 55;
    const int barW = 148;
    const int barH = 12;

    // Isitici
    int iy = OUT_Y + 6;
    _tft.setTextFont(1);
    _tft.setTextSize(1);
    _tft.setTextDatum(ML_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(0);
    _tft.drawString("Isitici", lblX, iy + barH / 2);
    drawBar(barX, iy, barW, barH, data.heaterPWM, 255, COL_ORANGE, COL_BAR_BG);
    _tft.setTextDatum(MR_DATUM);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(24);
    char hv[6]; snprintf(hv, sizeof(hv), "%d", data.heaterPWM);
    _tft.drawString(hv, SCR_W - 8, iy + barH / 2);

    // Fan
    int fy = iy + barH + 6;
    _tft.setTextDatum(ML_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(0);
    _tft.drawString("Fan", lblX, fy + barH / 2);
    drawBar(barX, fy, barW, barH, data.fanPWM, 255, COL_CYAN, COL_BAR_BG);
    _tft.setTextDatum(MR_DATUM);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(24);
    char fv[6]; snprintf(fv, sizeof(fv), "%d", data.fanPWM);
    _tft.drawString(fv, SCR_W - 8, fy + barH / 2);

    // Nem
    int ny = fy + barH + 6;
    _tft.setTextDatum(ML_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(0);
    _tft.drawString("Nem", lblX, ny + barH / 2);
    uint16_t indBg = data.humidifierOn ? COL_GREEN : COL_BAR_BG;
    uint16_t indFg = data.humidifierOn ? COL_TEXT : COL_DIM;
    _tft.fillRoundRect(barX, ny, barW, barH, barH / 2, indBg);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(indFg, indBg);
    _tft.drawString(data.humidifierOn ? "ACIK" : "KAPALI", barX + barW / 2, ny + barH / 2);
}

void DisplayManager::drawSensors(const DisplayData &data) {
    if (_bgDraw) _tft.fillRoundRect(2, SNS_Y, SCR_W - 4, SNS_H, 4, COL_CARD);
    const int sw = (SCR_W - 4) / 3;
    _tft.setTextFont(1);
    _tft.setTextSize(1);
    _tft.setTextDatum(TC_DATUM);
    _tft.setTextPadding(0);

    int cx1 = 2 + sw / 2;
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("SHT20", cx1, SNS_Y + 2);
    _tft.setTextColor(data.sensor1OK ? COL_GREEN : COL_RED, COL_CARD);
    _tft.drawString(data.sensor1OK ? "OK" : "HATA", cx1, SNS_Y + 13);

    int cx2 = 2 + sw + sw / 2;
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("RS485", cx2, SNS_Y + 2);
    _tft.setTextColor(data.sensor2OK ? COL_GREEN : COL_RED, COL_CARD);
    _tft.drawString(data.sensor2OK ? "OK" : "HATA", cx2, SNS_Y + 13);

    int cx3 = 2 + 2 * sw + sw / 2;
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("UP", cx3, SNS_Y + 2);
    unsigned long s = data.uptimeSec;
    char ut[16];
    snprintf(ut, sizeof(ut), "%lus%lud%lus", s / 3600, (s % 3600) / 60, s % 60);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(sw);
    _tft.drawString(ut, cx3, SNS_Y + 13);
    _tft.setTextPadding(0);
}

void DisplayManager::drawAlarm(const DisplayData &data) {
    if (_bgDraw) _tft.fillRoundRect(2, ALM_Y, SCR_W - 4, ALM_H, 4, COL_CARD);
    if (data.alarmActive) {
        _tft.drawRoundRect(2, ALM_Y, SCR_W - 4, ALM_H, 4, COL_RED);
    }
    _tft.setTextFont(2);
    _tft.setTextSize(1);
    _tft.setTextDatum(ML_DATUM);
    _tft.setTextPadding(SCR_W - 16);
    if (data.alarmActive && data.alarmMsg.length() > 0) {
        _tft.setTextColor(COL_RED, COL_CARD);
        String txt = data.alarmMsg;
        if (txt.length() > 25) txt = txt.substring(0, 22) + "...";
        _tft.drawString(txt, 8, ALM_Y + ALM_H / 2);
    } else {
        _tft.setTextColor(COL_GREEN, COL_CARD);
        _tft.drawString("Alarm yok", 8, ALM_Y + ALM_H / 2);
    }
    _tft.setTextPadding(0);
}

// ==================== SAYFA: KONTROL ====================

void DisplayManager::drawControl(const DisplayData &data) {
    // Dropdown aciksa tam sayfa profil listesi goster (sadece ilk cizimde)
    if (_profileListOpen) {
        if (_bgDraw) drawProfileList();
        return;
    }

    // Baslik
    if (_bgDraw) _tft.fillRoundRect(2, 2, SCR_W - 4, 28, 4, COL_CARD);
    _tft.setTextFont(2);
    _tft.setTextSize(1);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(0);
    _tft.drawString("SISTEM KONTROL", SCR_W / 2, 16);

    // 4 Buton (2x2 grid) - her zaman ciz
    {
        int lx = 4;
        int rx = SCR_W / 2 + 2;

        // Durum bazli renk: aktif buton parlak, pasif soluk
        int st = data.systemState;
        // Baslat: SYS_INITIALIZING(0), SYS_PAUSED(3), SYS_EMERGENCY(5), SYS_COMPLETED(4)
        uint16_t cStart = (st == 0 || st == 3 || st == 4 || st == 5) ? COL_GREEN : COL_DIM;
        // Duraklat: SYS_RUNNING(2), SYS_AUTOTUNING(1)
        uint16_t cPause = (st == 1 || st == 2) ? COL_ORANGE : COL_DIM;
        // Devam: SYS_PAUSED(3)
        uint16_t cResume = (st == 3) ? COL_CYAN : COL_DIM;
        // Durdur: her zaman aktif (SYS_INITIALIZING haric)
        uint16_t cStop = (st != 0) ? COL_RED : COL_DIM;

        drawButton(lx, BTN_Y1, BTN_W, BTN_H, "> Baslat",   cStart,  COL_TEXT);
        drawButton(rx, BTN_Y1, BTN_W, BTN_H, "|| Duraklat", cPause,  COL_TEXT);
        drawButton(lx, BTN_Y2, BTN_W, BTN_H, "> Devam",     cResume, COL_TEXT);
        drawButton(rx, BTN_Y2, BTN_W, BTN_H, "# Durdur",    cStop,   COL_TEXT);
    }

    // PID Bilgi
    int py = 140;
    if (_bgDraw) _tft.fillRoundRect(2, py, SCR_W - 4, 52, 4, COL_CARD);
    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("PID PARAMETRELERI", 10, py + 4);

    _tft.setTextFont(2);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    char pid[40];
    snprintf(pid, sizeof(pid), "Kp=%.1f  Ki=%.2f  Kd=%.1f", data.kp, data.ki, data.kd);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextPadding(SCR_W - 20);
    _tft.drawString(pid, SCR_W / 2, py + 32);
    _tft.setTextPadding(0);

    // Nem esikleri
    int ny = 198;
    if (_bgDraw) _tft.fillRoundRect(2, ny, SCR_W - 4, 52, 4, COL_CARD);
    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("NEM ESIKLERI (OTOMATIK)", 10, ny + 4);

    _tft.setTextFont(2);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    char nem[30];
    snprintf(nem, sizeof(nem), "%%%d - %%%d", (int)data.targetHumLow, (int)data.targetHumHigh);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextPadding(SCR_W - 20);
    _tft.drawString(nem, SCR_W / 2, ny + 32);
    _tft.setTextPadding(0);

    // Cevirme durumu
    int cy = 256;
    if (_bgDraw) _tft.fillRoundRect(2, cy, (SCR_W - 8) / 2, 28, 4, COL_CARD);
    _tft.setTextFont(2);
    _tft.setTextSize(1);
    _tft.setTextDatum(ML_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(0);
    _tft.drawString("Cevirme:", 10, cy + 14);
    _tft.setTextDatum(MR_DATUM);
    _tft.setTextColor(data.turningEnabled ? COL_GREEN : COL_RED, COL_CARD);
    _tft.drawString(data.turningEnabled ? "AKTIF" : "PASIF", (SCR_W - 8) / 2 - 4, cy + 14);

    // Profil Sec butonu (sag)
    int px = SCR_W / 2 + 2;
    int pw = SCR_W / 2 - 4;
    if (_bgDraw) _tft.fillRoundRect(px, cy, pw, 28, 4, COL_BLUE);
    _tft.setTextFont(2);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_TEXT, COL_BLUE);
    _tft.setTextPadding(pw - 8);
    char profBtn[24];
    snprintf(profBtn, sizeof(profBtn), "%s v", data.profileName ? data.profileName : "Sec");
    _tft.drawString(profBtn, px + pw / 2, cy + 14);
    _tft.setTextPadding(0);
}

// ==================== SAYFA: PROFIL ====================

void DisplayManager::drawProfile(const DisplayData &data) {
    // --- Profil baslik karti ---
    if (_bgDraw) _tft.fillRoundRect(2, 2, SCR_W - 4, 34, 4, COL_CARD);

    // Profil adi (sol)
    _tft.setTextFont(2);
    _tft.setTextSize(1);
    _tft.setTextDatum(ML_DATUM);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(SCR_W - 40);
    char hdr[40];
    const char* pn = data.profileName ? data.profileName : "---";
    snprintf(hdr, sizeof(hdr), "%s - %dg", pn, data.totalDays);
    _tft.drawString(hdr, 8, 12);
    _tft.setTextPadding(0);

    // Alt baslik: Gun ve evre
    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(SCR_W - 40);
    char sub[32];
    snprintf(sub, sizeof(sub), "Gun %d/%d  |  Evre: %s",
             data.currentDay, data.totalDays,
             data.phaseName ? data.phaseName : "---");
    _tft.drawString(sub, 8, 26);
    _tft.setTextPadding(0);

    // Evre listesi
    const AnimalProfile* prof = data.profile;
    if (!prof) {
        _tft.setTextFont(2);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(COL_DIM, COL_BG);
        _tft.drawString("Profil bulunamadi", SCR_W / 2, 100);
        return;
    }

    const int phH = 58;   // Her evre karti yuksekligi
    const int gap = 4;
    int startY = 42;
    int maxVisible = (PAGE_H - startY) / (phH + gap);

    // Scroll siniri
    int totalCards = prof->phaseCount;
    int maxScroll = totalCards - maxVisible;
    if (maxScroll < 0) maxScroll = 0;
    if (_scrollOffset > maxScroll) _scrollOffset = maxScroll;

    for (uint8_t idx = 0; idx < maxVisible && (idx + _scrollOffset) < totalCards; idx++) {
        uint8_t i = idx + _scrollOffset;
        const IncubationPhase &ph = prof->phases[i];
        if (ph.startDay == 0 && ph.endDay == 0) continue;

        int cy = startY + idx * (phH + gap);
        if (cy + phH > PAGE_H) break;

        // Aktif evre vurgusu
        bool isActive = (i == data.currentPhaseIndex);
        uint16_t cardBg = isActive ? 0x0A2A : COL_CARD;
        uint16_t border = isActive ? COL_GREEN : COL_CARD;

        if (_bgDraw) {
            _tft.fillRoundRect(2, cy, SCR_W - 4, phH, 4, cardBg);
            if (isActive) {
                _tft.drawRoundRect(2, cy, SCR_W - 4, phH, 4, border);
            }
        }

        // Evre adi + gun araligi
        _tft.setTextFont(2);
        _tft.setTextDatum(TL_DATUM);
        _tft.setTextColor(isActive ? COL_GREEN : COL_PURPLE, cardBg);
        _tft.setTextPadding(SCR_W - 16);
        char label[32];
        snprintf(label, sizeof(label), "%s  (Gun %d-%d)", ph.phaseName, ph.startDay, ph.endDay);
        _tft.drawString(label, 8, cy + 4);
        _tft.setTextPadding(0);

        // Sicaklik + Nem
        _tft.setTextFont(1);
        _tft.setTextColor(COL_TEXT, cardBg);
        char line1[40];
        snprintf(line1, sizeof(line1), "Sicaklik: %.1f C", ph.temperature);
        _tft.drawString(line1, 12, cy + 22);

        char line2[40];
        snprintf(line2, sizeof(line2), "Nem: %%%d - %%%d", (int)ph.humidityLow, (int)ph.humidityHigh);
        _tft.drawString(line2, 12, cy + 34);

        // Cevirme durumu (sag)
        _tft.setTextDatum(TR_DATUM);
        _tft.setTextColor(ph.turningEnabled ? COL_GREEN : COL_RED, cardBg);
        _tft.drawString(ph.turningEnabled ? "Cevirme: ACIK" : "Cevirme: KAPALI",
                         SCR_W - 8, cy + 45);
        _tft.setTextDatum(TL_DATUM);
    }

    // Scroll gostergesi (birden fazla sayfa varsa)
    if (maxScroll > 0) {
        int indY = PAGE_H - 16;
        _tft.setTextFont(1);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(COL_DIM, COL_BG);
        _tft.setTextPadding(60);
        char sc[16];
        snprintf(sc, sizeof(sc), "%d/%d", _scrollOffset + 1, maxScroll + 1);
        _tft.drawString(sc, SCR_W / 2, indY);
        _tft.setTextPadding(0);
    }
}

// ==================== PROFIL DROPDOWN LISTESI ====================

void DisplayManager::drawProfileList() {
    const int itemH  = 36;       // Dokunma alani (touch handler ile ayni!)
    const int hdrH   = 40;       // Ust baslik yuksekligi
    const int ftrH   = 40;       // Alt buton yuksekligi
    const int listY  = hdrH;

    int maxVisible = (SCR_H - hdrH - ftrH) / itemH;  // (320-40-40)/36 = 6
    if (maxVisible > PROFILE_COUNT) maxVisible = PROFILE_COUNT;
    int maxScroll = PROFILE_COUNT - maxVisible;
    if (maxScroll < 0) maxScroll = 0;
    if (_scrollOffset > maxScroll) _scrollOffset = maxScroll;

    int listEndY = listY + maxVisible * itemH;

    // ---- Ust baslik + yukari ok ----
    _tft.fillRect(0, 0, SCR_W, hdrH, COL_CARD);
    _tft.setTextFont(2);
    _tft.setTextSize(1);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_BLUE, COL_CARD);
    _tft.setTextPadding(0);
    _tft.drawString("PROFIL SEC", SCR_W / 2, 10);

    if (_scrollOffset > 0) {
        _tft.setTextFont(2);
        _tft.setTextColor(COL_CYAN, COL_CARD);
        _tft.drawString("^  YUKARI  ^", SCR_W / 2, 28);
    } else {
        _tft.setTextFont(1);
        _tft.setTextColor(COL_DIM, COL_CARD);
        char info[20];
        snprintf(info, sizeof(info), "%d - %d / %d", _scrollOffset + 1,
                 _scrollOffset + maxVisible, PROFILE_COUNT);
        _tft.drawString(info, SCR_W / 2, 30);
    }
    _tft.drawFastHLine(0, hdrH - 2, SCR_W, COL_DIM);

    // ---- Profil listesi ----
    for (int idx = 0; idx < maxVisible; idx++) {
        int i = idx + _scrollOffset;
        if (i >= PROFILE_COUNT) break;

        int iy = listY + idx * itemH;
        const AnimalProfile* p = ALL_PROFILES[i];

        // Satir arkaplan (alternating)
        uint16_t rowBg = (idx % 2 == 0) ? COL_CARD : COL_BG;
        _tft.fillRect(0, iy, SCR_W, itemH, rowBg);

        // Sol kenar renk gostergesi
        uint16_t accent = (idx % 3 == 0) ? COL_GREEN : (idx % 3 == 1) ? COL_BLUE : COL_ORANGE;
        _tft.fillRect(0, iy + 2, 4, itemH - 4, accent);

        // Ayirici cizgi (altta)
        _tft.drawFastHLine(4, iy + itemH - 1, SCR_W - 4, COL_BAR_BG);

        // Profil adi (sol)
        _tft.setTextFont(2);
        _tft.setTextSize(1);
        _tft.setTextDatum(ML_DATUM);
        _tft.setTextColor(COL_TEXT, rowBg);
        _tft.setTextPadding(0);
        _tft.drawString(p->name, 12, iy + itemH / 2);

        // Gun sayisi (sag)
        _tft.setTextDatum(MR_DATUM);
        _tft.setTextColor(COL_DIM, rowBg);
        char days[12];
        snprintf(days, sizeof(days), "%d gun", p->totalDays);
        _tft.drawString(days, SCR_W - 8, iy + itemH / 2);
    }

    // ---- Alt alan: asagi ok veya bos ----
    _tft.fillRect(0, listEndY, SCR_W, SCR_H - listEndY, COL_BG);
    if (_scrollOffset < maxScroll) {
        _tft.drawFastHLine(0, listEndY + 2, SCR_W, COL_DIM);
        _tft.setTextFont(2);
        _tft.setTextSize(1);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(COL_CYAN, COL_BG);
        _tft.setTextPadding(0);
        _tft.drawString("v  ASAGI  v", SCR_W / 2, listEndY + ftrH / 2);
    }
}

// ==================== SAYFA: AYAR ====================

void DisplayManager::drawSettings(const DisplayData &data) {
    int cy = 2;

    // Ag Bilgisi
    if (_bgDraw) _tft.fillRoundRect(2, cy, SCR_W - 4, 70, 4, COL_CARD);
    _tft.setTextFont(1);
    _tft.setTextSize(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(0);
    _tft.drawString("AG BILGISI", 10, cy + 4);

    _tft.setTextFont(2);
    int ly = cy + 18;
    if (data.apActive) {
        _tft.setTextColor(COL_ORANGE, COL_CARD);
        _tft.drawString("AP:", 10, ly);
        _tft.setTextColor(COL_TEXT, COL_CARD);
        _tft.setTextPadding(SCR_W - 50);
        char ap[40];
        snprintf(ap, sizeof(ap), "%s [%d]", data.apIP.c_str(), data.apClients);
        _tft.drawString(ap, 36, ly);
        _tft.setTextPadding(0);
        ly += 18;
    }
    if (data.staConnected) {
        _tft.setTextColor(COL_BLUE, COL_CARD);
        _tft.drawString("WiFi:", 10, ly);
        _tft.setTextColor(COL_TEXT, COL_CARD);
        _tft.drawString(data.staIP, 56, ly);
        ly += 18;
    }
    if (!data.apActive && !data.staConnected) {
        _tft.setTextColor(COL_RED, COL_CARD);
        _tft.drawString("Bagli degil", 10, ly);
    }

    // Sensor Durumu
    cy = 78;
    if (_bgDraw) _tft.fillRoundRect(2, cy, SCR_W - 4, 52, 4, COL_CARD);
    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("SENSOR DURUMU", 10, cy + 4);

    _tft.setTextFont(2);
    ly = cy + 18;
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.drawString("SHT20:", 10, ly);
    _tft.setTextColor(data.sensor1OK ? COL_GREEN : COL_RED, COL_CARD);
    _tft.drawString(data.sensor1OK ? "OK" : "HATA", 80, ly);

    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.drawString("RS485:", 130, ly);
    _tft.setTextColor(data.sensor2OK ? COL_GREEN : COL_RED, COL_CARD);
    _tft.drawString(data.sensor2OK ? "OK" : "HATA", 196, ly);

    // Sistem Bilgisi
    cy = 136;
    if (_bgDraw) _tft.fillRoundRect(2, cy, SCR_W - 4, 68, 4, COL_CARD);
    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("SISTEM BILGISI", 10, cy + 4);

    _tft.setTextFont(2);
    ly = cy + 18;
    _tft.setTextColor(COL_TEXT, COL_CARD);

    // Uptime
    unsigned long s = data.uptimeSec;
    int upH = s / 3600;
    int upM = (s % 3600) / 60;
    int upS = s % 60;
    char ut[32];
    snprintf(ut, sizeof(ut), "Uptime: %ds %dd %ds", upH, upM, upS);
    _tft.drawString(ut, 10, ly);

    // Heap
    ly += 18;
    char hp[32];
    snprintf(hp, sizeof(hp), "RAM: %lu KB", data.freeHeap / 1024);
    _tft.drawString(hp, 10, ly);

    // Durum
    ly += 18;
    int si = constrain(data.systemState, 0, 5);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("Durum:", 10, ly);
    _tft.setTextColor(ST_CLR[si], COL_CARD);
    _tft.drawString(ST_SHORT[si], 80, ly);

    cy = 210;
    if (_bgDraw) _tft.fillRoundRect(2, cy, SCR_W - 4, 52, 4, COL_CARD);
    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("BASLANGIC TARIHI", 10, cy + 4);

    if (!_editDateInit && data.startDate.length() > 0) {
        int d = 0, m = 0, y = 0;
        if (sscanf(data.startDate.c_str(), "%d/%d/%d", &d, &m, &y) == 3) {
            if (y >= 2000 && y <= 2099) _editYear = (uint16_t)y;
            if (m >= 1 && m <= 12) _editMonth = (uint8_t)m;
            if (d >= 1 && d <= 31) _editDay = (uint8_t)d;
        }
        _editDateInit = true;
    }

    _tft.setTextFont(2);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    char dt[16];
    snprintf(dt, sizeof(dt), "%02u/%02u/%04u", (unsigned)_editDay, (unsigned)_editMonth, (unsigned)_editYear);
    _tft.drawString(dt, 10, cy + 18);

    const int btnW = 28;
    const int btnH = 18;
    const int row1Y = cy + 18;
    const int row2Y = cy + 34;
    const int col1X = 4 + 8;
    const int col2X = 4 + 78;
    const int col3X = 4 + 148;
    const int saveX = 4 + (SCR_W - 8) - 56;
    const int saveW = 48;
    const int saveH = 18;

    drawButton(col1X, row1Y, btnW, btnH, "-", COL_BAR_BG, COL_TEXT);
    drawButton(col1X + btnW + 2, row1Y, btnW, btnH, "+", COL_BAR_BG, COL_TEXT);
    drawButton(col2X, row1Y, btnW, btnH, "-", COL_BAR_BG, COL_TEXT);
    drawButton(col2X + btnW + 2, row1Y, btnW, btnH, "+", COL_BAR_BG, COL_TEXT);
    drawButton(col3X, row1Y, btnW, btnH, "-", COL_BAR_BG, COL_TEXT);
    drawButton(col3X + btnW + 2, row1Y, btnW, btnH, "+", COL_BAR_BG, COL_TEXT);
    drawButton(saveX, row2Y, saveW, saveH, "OK", COL_GREEN, COL_TEXT);

    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("G/A/Y", 10, cy + 38);
}

// ==================== YARDIMCI: PROGRESS BAR ====================

void DisplayManager::drawBar(int x, int y, int w, int h, int val, int maxVal,
                              uint16_t fg, uint16_t bg) {
    int r = h / 2;
    _tft.fillRoundRect(x, y, w, h, r, bg);
    int fw = (int)((long)constrain(val, 0, maxVal) * w / maxVal);
    if (fw > 0) {
        if (fw < h) fw = h;
        _tft.fillRoundRect(x, y, fw, h, r, fg);
    }
}

// ==================== YARDIMCI: BUTON ====================

void DisplayManager::drawButton(int x, int y, int w, int h,
                                 const char* label, uint16_t bg, uint16_t fg) {
    _tft.fillRoundRect(x, y, w, h, 6, bg);
    _tft.setTextFont(2);
    _tft.setTextSize(1);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(fg, bg);
    _tft.setTextPadding(0);
    _tft.drawString(label, x + w / 2, y + h / 2);
}
