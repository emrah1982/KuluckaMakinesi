#include "DisplayManager.h"
#include "AnimalIcons.h"
#include "CandlingScheduler.h"
#include <esp_task_wdt.h>
#include "WdtFeed.h"
#include "StorageService.h"
#include <math.h>

// Derece -> Radyan donusumu
#define DEG2RAD 0.0174532925f

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
// 0=INIT 1=AUTOTUNE 2=RUNNING 3=PAUSED 4=COMPLETED 5=EMERGENCY 6=CLEANING
static const char* ST_BADGE[]  = {"BASLA", "TUNE", "CALISIYOR", "DURAKLAT", "TAMAM", "ACIL!", "TEMIZLIK"};
static const char* ST_SHORT[]  = {"Basla", "Tune", "Calisiyor", "Duraklat", "Tamam", "ACIL!", "Temizlik"};
static const uint16_t ST_CLR[] = {COL_DIM, COL_PURPLE, COL_GREEN, COL_ORANGE, COL_BLUE, COL_RED, COL_CYAN};

// Sekme isimleri (5 sekme: Durum, Olcum, Kontrol, Profil, Ayar)
static const char* TAB_LABEL[] = {"Durum", "Olcum", "Kontrol", "Profil", "Ayar"};

// Profilleri alfabetik siralayan index haritasi (gercek index degismez!)
// _profileSortMap[i] = ALL_PROFILES dizisindeki gercek index
static uint8_t _profileSortMap[PROFILE_COUNT];
static bool _profileSortReady = false;

static void buildProfileSortMap() {
    for (uint8_t i = 0; i < PROFILE_COUNT; i++) _profileSortMap[i] = i;
    // Insertion sort: alfabetik (case-insensitive)
    for (uint8_t i = 1; i < PROFILE_COUNT; i++) {
        uint8_t key = _profileSortMap[i];
        int8_t j = i - 1;
        while (j >= 0 && strcmp(ALL_PROFILES[_profileSortMap[j]]->name,
                                ALL_PROFILES[key]->name) > 0) {
            _profileSortMap[j + 1] = _profileSortMap[j];
            j--;
        }
        _profileSortMap[j + 1] = key;
    }
    _profileSortReady = true;
}

static inline uint8_t profileRealIdx(uint8_t sortedPos) {
    if (!_profileSortReady) buildProfileSortMap();
    return _profileSortMap[sortedPos];
}

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
    , _prevTemp(-999)
    , _prevHum(-999)
    , _prevCO2(0)
    , _prevTempMin(-999)
    , _prevTempMax(-999)
    , _prevHumMin(-999)
    , _prevHumMax(-999)
    , _prevTempColor(0)
    , _prevHumColor(0)
    , _prevCO2Color(0)
    , _loadingTaskHandle(nullptr)
    , _loadingActive(false)
{
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

    // ========== SECGEM SPLASH ANIMASYONU ==========
    showSplashAnimation();

    _lastUpdate = 0;
    _lastTouch  = 0;
    _tab = TAB_DASH;
    _forceRedraw = true;
    _bgDraw = true;  // Tab bar ve arka plan cizimi icin

    DEBUG_PRINTLN("[TOUCH] OK");
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

    // --- ALARM MODAL aciksa ---
    // Tum alarmlarda 3 buton: SUSTUR (10dk), ERTELE (1sa), KAPAT
    // CANDLING'de SUSTUR butonu yerine direkt ERTELE/KAPAT (kullanici karari net olsun)
    if (_alarmModalOpen) {
        bool isCandling = (data.alarmType == ALARM_CANDLING_DUE);
        const int btnX = 20;
        const int btnW = SCR_W - 40;
        const int btnH = 38;
        const int btnGap = 6;
        // 3 buton: SUSTUR / ERTELE / KAPAT (alt alta)
        const int totalH = (isCandling ? 2 : 3) * btnH + (isCandling ? 1 : 2) * btnGap;
        const int firstY = SCR_H - totalH - 14;

        if (isCandling) {
            // CANDLING: ERTELE + KAPAT (2 buton)
            int snoozeY  = firstY;
            int dismissY = firstY + btnH + btnGap;
            if (touchInRect(tx, ty, btnX, snoozeY, btnW, btnH)) {
                _alarmModalOpen = false;
                _alarmModalDrawn = false;
                _forceRedraw = true;
                return TOUCH_ALARM_SNOOZE;
            }
            if (touchInRect(tx, ty, btnX, dismissY, btnW, btnH)) {
                _alarmModalOpen = false;
                _alarmModalDrawn = false;
                _forceRedraw = true;
                return TOUCH_ALARM_DISMISS;
            }
        } else {
            // STANDART: SUSTUR + ERTELE + KAPAT (3 buton)
            int ackY     = firstY;
            int snoozeY  = firstY + btnH + btnGap;
            int dismissY = firstY + 2 * (btnH + btnGap);
            if (touchInRect(tx, ty, btnX, ackY, btnW, btnH)) {
                _alarmModalOpen = false;
                _alarmModalDrawn = false;
                _forceRedraw = true;
                return TOUCH_ALARM_ACK;
            }
            if (touchInRect(tx, ty, btnX, snoozeY, btnW, btnH)) {
                _alarmModalOpen = false;
                _alarmModalDrawn = false;
                _forceRedraw = true;
                return TOUCH_ALARM_SNOOZE;
            }
            if (touchInRect(tx, ty, btnX, dismissY, btnW, btnH)) {
                _alarmModalOpen = false;
                _alarmModalDrawn = false;
                _forceRedraw = true;
                return TOUCH_ALARM_DISMISS;
            }
        }
        // Modal disindaki dokunmalar yutulur — kullanici acikca buton tikladi
        return TOUCH_NONE;
    }

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
            }
            return TOUCH_NONE;
        }

        // Asagi ok butonuna dokunma (alt alan, y >= listEndY)
        if (ty >= listEndY) {
            if (_scrollOffset < maxScroll) {
                _scrollOffset++;
                _forceRedraw = true;
            }
            return TOUCH_NONE;
        }

        // Liste icerisindeki profillere dokunma
        int idx = (ty - listY) / itemH;
        idx = constrain(idx, 0, maxVisible - 1);
        int sortedIdx = idx + _scrollOffset;
        int realIdx = -1;
        if (sortedIdx >= 0 && sortedIdx < PROFILE_COUNT) {
            realIdx = profileRealIdx(sortedIdx);  // Alfabetik -> gercek index
            _pendingProfileIdx = (uint8_t)realIdx;
            _profileListOpen = false;
            _scrollOffset = 0;
            _forceRedraw = true;
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
                _tab = newTab;
                _scrollOffset = 0;  // Tab degisince scroll sifirla
                _profileListOpen = false;
                _forceRedraw = true;
            }
        }
        return TOUCH_NONE;
    }

    // --- Profil EDITOR aciksa: tum ekran ona ait ---
    if (_tab == TAB_CTRL && _profileEditorOpen) {
        const int hdrH    = 32;
        const int rowH    = 42;
        const int rowGap  = 3;
        const int rowsY   = hdrH + 3;
        const int padX    = 8;
        const int btnW    = 50;
        const int btnH    = rowH - 8;
        const int rowCount = 4;   // Sicaklik, Nem Alt, Nem Ust, Cev Aralik

        // Satir bazli ±butonlar
        // NOT: forceRedraw set ETMIYORUZ — sadece deger metni guncellenir,
        // arkaplan/butonlar olduklari yerde kalir (flicker olmaz).
        for (int i = 0; i < rowCount; i++) {
            int y = rowsY + i * (rowH + rowGap);
            int btnY = y + rowH - btnH - 4;
            int minusX = padX + 8;
            int plusX  = SCR_W - padX - 8 - btnW;
            if (touchInRect(tx, ty, minusX, btnY, btnW, btnH)) {
                if (i == 0) return TOUCH_EDIT_TEMP_DEC;
                if (i == 1) return TOUCH_EDIT_HUMLOW_DEC;
                if (i == 2) return TOUCH_EDIT_HUMHIGH_DEC;
                if (i == 3) return TOUCH_EDIT_TURNINT_DEC;
            }
            if (touchInRect(tx, ty, plusX, btnY, btnW, btnH)) {
                if (i == 0) return TOUCH_EDIT_TEMP_INC;
                if (i == 1) return TOUCH_EDIT_HUMLOW_INC;
                if (i == 2) return TOUCH_EDIT_HUMHIGH_INC;
                if (i == 3) return TOUCH_EDIT_TURNINT_INC;
            }
        }

        // Alt: Fabrika + Kapat
        int botY = rowsY + rowCount * (rowH + rowGap) + 4;
        int botBtnH = 32;
        int botBtnW = (SCR_W - padX * 3) / 2;
        int closeX  = SCR_W - padX - botBtnW;

        if (touchInRect(tx, ty, padX, botY, botBtnW, botBtnH)) {
            // Fabrika butonunun rengi degisecek (override silindi -> gri),
            // bu yuzden BG yeniden cizilmeli.
            _forceRedraw = true;
            return TOUCH_EDIT_RESET_FACTORY;
        }
        if (touchInRect(tx, ty, closeX, botY, botBtnW, botBtnH)) {
            _profileEditorOpen = false;
            _forceRedraw = true;
            return TOUCH_NONE;
        }
        // Editor aciksa baska bolgeye dokunmayi yutuyoruz
        return TOUCH_NONE;
    }

    // --- Kontrol sayfasi butonlari (yeni tasarim) ---
    if (_tab == TAB_CTRL) {
        const int cardMargin = 4;
        const int cardW = SCR_W - cardMargin * 2;
        const int btnGap = 4;
        const int btnStartY = 34;
        const int btnH2 = 38;
        const int btnW2 = (cardW - btnGap) / 2;
        int lx = cardMargin;
        int rx = cardMargin + btnW2 + btnGap;

        // 2x2 buton grid
        if (touchInRect(tx, ty, lx, btnStartY, btnW2, btnH2)) return TOUCH_START;
        if (touchInRect(tx, ty, rx, btnStartY, btnW2, btnH2)) return TOUCH_PAUSE;
        if (touchInRect(tx, ty, lx, btnStartY + btnH2 + btnGap, btnW2, btnH2)) return TOUCH_RESUME;
        if (touchInRect(tx, ty, rx, btnStartY + btnH2 + btnGap, btnW2, btnH2)) return TOUCH_STOP;

        // Alt satir: Profil Sec (SOL) + Profili Duzenle (SAG)
        const int pidH = 44;
        const int nemH = 44;
        const int py = 122;
        const int ny = py + pidH + 4;
        const int cy = ny + nemH + 4;  // 218
        const int bottomH = 28;
        const int halfW = (cardW - 4) / 2;
        int px = cardMargin + halfW + 4;  // sag yari baslangici

        // Profil Sec — SOL
        if (touchInRect(tx, ty, cardMargin, cy, halfW, bottomH)) {
            _profileListOpen = true;
            _scrollOffset = 0;
            _forceRedraw = true;
            return TOUCH_NONE;
        }
        // Profili Duzenle — SAG (editoru ac)
        if (touchInRect(tx, ty, px, cy, halfW, bottomH)) {
            _profileEditorOpen = true;
            _forceRedraw = true;
            return TOUCH_NONE;
        }

        // En alt satir — sag yarida TEMIZLIK butonu
        const int ty2 = cy + bottomH + 4;
        const int turnH = 28;
        int clnX = cardMargin + halfW + 4;
        if (touchInRect(tx, ty, clnX, ty2, halfW, turnH)) {
            _forceRedraw = true;
            return TOUCH_CLEANING_TOGGLE;
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
        // YENI TASARIM (Tarih kart 28 + AG 58 + SENSOR 28 + SISTEM 58 + PROFIL 46 + IR 36)
        // Yumurta sayisi -/+ butonlari (Kart 4 PROFIL icinde, kompakt)
        // PROFIL cy = 2+28+4+58+4+28+4+58+4 = 190, btnY = cy+22 = 212
        const int btnY = 212;
        const int btnW = 28;
        const int btnH = 20;
        const int leftBtnX  = 4 + 8 + 6;             // cardPadX + cardMargin + 6 = 18
        const int rightBtnX = leftBtnX + btnW + 42;  // 88
        if (touchInRect(tx, ty, leftBtnX, btnY, btnW, btnH))  return TOUCH_EGG_DEC;
        if (touchInRect(tx, ty, rightBtnX, btnY, btnW, btnH)) return TOUCH_EGG_INC;

        // IR YUMURTA toggle butonu (Kart 5 - cy=240, tgl y = cy+6 = 246)
        const int tglW = 72;
        const int tglH = 24;
        const int tglX = SCR_W - 4 - 8 - tglW;
        const int tglY = 246;
        if (touchInRect(tx, ty, tglX, tglY, tglW, tglH)) {
            _forceRedraw = true;
            return TOUCH_EGG_SENSOR_TOGGLE;
        }
        // IR YUMURTA "BUL" butonu (Kart 5, sol-alt) — cy=240, btnY = cy+18 = 258
        const int bulX = 4 + 8;       // 12
        const int bulY = 254;         // dokunma zonu cy+14..cy+34
        const int bulW = 64;
        const int bulH = 22;
        if (touchInRect(tx, ty, bulX, bulY, bulW, bulH)) {
            _forceRedraw = true;
            return TOUCH_EGG_DISCOVER;
        }

        if (ty > 285) {
            _scrollOffset++;
            _forceRedraw = true;
            return TOUCH_NONE;
        }
    }

    // --- Dashboard: alarm kartina dokunma -> kullanici alarmi sustur (ack) ---
    // (Onceki davranis TOUCH_SAFETY_RESET idi; v3'te alarm ack daha sik kullanilir.
    // Safety reset acil durumda Kontrol tab'indan zaten yapilabilir.)
    if (_tab == TAB_DASH) {
        if (touchInRect(tx, ty, 2, ALM_Y, SCR_W - 4, ALM_H) && data.alarmActive) {
            _forceRedraw = true;
            return TOUCH_ALARM_ACK;
        }
    }

    return TOUCH_NONE;
}

// ==================== UPDATE ====================

TouchAction DisplayManager::update(const DisplayData &data) {
    // Loading animasyonu calisiyorsa once durdur, landscape splash'ten
    // portrait dashboard'a gec (dashboard ilk kez ciziliyor)
    if (_loadingActive) {
        stopLoadingAnimation();
        _tft.fillScreen(COL_BG);
        _tft.setRotation(0);     // portrait'a gec
        _tft.fillScreen(COL_BG);
        _forceRedraw = true;
        _bgDraw      = true;
    }
    // Alarm modal otomatik ac/kapat (alarm aktif + mute degil -> ac)
    if (data.alarmAutoShow && !_alarmModalOpen) {
        _alarmModalOpen  = true;
        _alarmModalDrawn = false;
        _forceRedraw     = true;
    }
    // Alarm temizlendi -> modal'i kapat
    if (!data.alarmActive && _alarmModalOpen) {
        _alarmModalOpen  = false;
        _alarmModalDrawn = false;
        _forceRedraw     = true;
    }

    // Cleaning gecisinde tam ekran yenile (banner alanini temizlemek icin)
    static bool s_prevCleaning = false;
    if (data.cleaningActive != s_prevCleaning) {
        _forceRedraw = true;
        s_prevCleaning = data.cleaningActive;
    }

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

    // Modal acik degilse normal tab cizimi
    if (!_alarmModalOpen) {
        draw(data);
    } else {
        // Modal: tum tab'larin ustunde tam ekran (tab bar dahil)
        drawAlarmModal(data);
    }
    return action;
}

void DisplayManager::draw(const DisplayData &data) {
    drawTabBar();

    switch (_tab) {
        case TAB_DASH:  drawDashboard(data); break;
        case TAB_GRAPH: drawGraph(data);     break;
        case TAB_CTRL:  drawControl(data);   break;
        case TAB_PROF:  drawProfile(data);   break;
        case TAB_SET:   drawSettings(data);  break;
        default:        drawDashboard(data); break;
    }

    // ===== TEMIZLIK MODU UYARI BANTI (tum sekmeler uzerinde) =====
    // Aktifken her cizimden sonra ust kisima sari/turuncu yanip sonen bir
    // bant ciziyoruz: kullanici hangi sekmede olursa olsun modun aktif
    // oldugunu net gorur.
    if (data.cleaningActive) {
        drawCleaningBanner(data);
    }

    // ===== CANDLING (DOL KONTROLU) BILDIRISI =====
    // Bugun kontrol gunu ise temizlik banneri yoksa ust bant goster
    if (data.candlingToday && !data.cleaningActive) {
        drawCandlingBanner(data);
    }
}

void DisplayManager::drawCleaningBanner(const DisplayData &data) {
    // Yanip sonen efekt (500 ms periyot) — kullanici sezmesin diye 2 ton
    bool blink = ((millis() / 500) % 2) == 0;
    uint16_t bg = blink ? COL_ORANGE : COL_RED;

    const int bh = 16;
    const int by = 0;
    _tft.fillRect(0, by, SCR_W, bh, bg);
    _tft.drawFastHLine(0, bh, SCR_W, COL_RED);

    // Sol: uyari ikonu/metin (kucuk font 1)
    _tft.setTextFont(1);
    _tft.setTextSize(1);
    _tft.setTextDatum(ML_DATUM);
    _tft.setTextColor(COL_TEXT, bg);
    _tft.setTextPadding(0);
    _tft.drawString("! TEMIZLIK MODU AKTIF", 4, by + bh / 2);

    // Sag: kalan dakika + heater/fan PWM (live geri bildirim)
    unsigned long mins = (data.cleaningRemainMs + 59999UL) / 60000UL;
    char buf[40];
    snprintf(buf, sizeof(buf), "H%u F%u %lud",
             data.heaterPWM, data.fanPWM, mins);
    _tft.setTextDatum(MR_DATUM);
    _tft.drawString(buf, SCR_W - 4, by + bh / 2);
    _tft.setTextDatum(TL_DATUM);
}

void DisplayManager::drawCandlingBanner(const DisplayData &data) {
    // Yanip sonen efekt (500 ms periyot) — kirmizi/sari (dikkat cekici)
    bool blink = ((millis() / 500) % 2) == 0;
    uint16_t bg = blink ? COL_RED : COL_ORANGE;

    const int bh = 18;
    const int by = 0;
    _tft.fillRect(0, by, SCR_W, bh, bg);
    _tft.drawFastHLine(0, bh, SCR_W, COL_RED);

    // Sol: dol kontrolu uyari metni (kalinca yazı)
    _tft.setTextFont(2);
    _tft.setTextSize(1);
    _tft.setTextDatum(ML_DATUM);
    _tft.setTextColor(COL_TEXT, bg);
    _tft.setTextPadding(0);
    char buf[56];
    snprintf(buf, sizeof(buf), "🔔 DOL KONTROLU ZAMANI! %s", data.candlingLabel ? data.candlingLabel : "");
    _tft.drawString(buf, 4, by + bh / 2);

    // Sag: lockdown gunu
    _tft.setTextFont(1);
    snprintf(buf, sizeof(buf), "Lockdown: G%u", data.candlingLockdownDay);
    _tft.setTextDatum(MR_DATUM);
    _tft.drawString(buf, SCR_W - 4, by + bh / 2);
    _tft.setTextDatum(TL_DATUM);
}

// ==================== TAB BAR (ALTTA) ====================

void DisplayManager::drawTabBar() {
    int tabW = SCR_W / TAB_COUNT;
    int by = PAGE_H;

    // Tab bar her zaman cizilsin (ilk acilista ve sekme degisiminde)
    _tft.fillRect(0, by, SCR_W, TAB_H, COL_CARD);
    _tft.drawFastHLine(0, by, SCR_W, COL_DIM);

    for (int i = 0; i < TAB_COUNT; i++) {
        int bx = i * tabW;
        bool active = (i == (int)_tab);

        _tft.setTextFont(2);
        _tft.setTextSize(1);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextPadding(0);

        if (active) {
            _tft.fillRect(bx + 1, by + 1, tabW - 2, TAB_H - 1, COL_BG);
            _tft.fillRect(bx + 4, by + 1, tabW - 8, 2, COL_BLUE);
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

// ==================== SAYFA: GRAFIK (GAUGE TARZI - FLICKER-FREE) ====================

void DisplayManager::drawGauge(int cx, int cy, int r, float value, float minVal,
                                float maxVal, float target, uint16_t color,
                                const char* label, const char* unit,
                                float bandLo, float bandHi, float devRef) {
    const bool showDev = (devRef > GAUGE_NONE + 1.0f);
    // Sadece ilk cizimde statik elemanlari ciz
    if (_bgDraw) {
        // Arkaplan yay (koyu gri) - sadece 1 kez cizilir
        for (int i = 0; i <= 180; i += 3) {
            float angle = (180.0f - i) * DEG2RAD;
            int x1 = cx + cos(angle) * (r - 6);
            int y1 = cy - sin(angle) * (r - 6);
            int x2 = cx + cos(angle) * r;
            int y2 = cy - sin(angle) * r;
            _tft.drawLine(x1, y1, x2, y2, 0x2104);
        }
        
        // Isaretciler - sadece 1 kez cizilir.
        //   set noktasi : kalin (2px) turuncu, yayin disina tasar
        //   bant sinirlari: ince (1px) turuncu, alarm esiklerini gosterir
        auto drawMark = [&](float t, bool thick) {
            float tr = (t - minVal) / (maxVal - minVal);
            tr = constrain(tr, 0.0f, 1.0f);
            float tAngle = (180.0f - tr * 180.0f) * DEG2RAD;
            int tx1 = cx + cos(tAngle) * (r - 10);
            int ty1 = cy - sin(tAngle) * (r - 10);
            int tx2 = cx + cos(tAngle) * (r + (thick ? 4 : 2));
            int ty2 = cy - sin(tAngle) * (r + (thick ? 4 : 2));
            _tft.drawLine(tx1, ty1, tx2, ty2, COL_ORANGE);
            if (thick) _tft.drawLine(tx1 + 1, ty1, tx2 + 1, ty2, COL_ORANGE);
        };
        if (bandLo > GAUGE_NONE + 1.0f) drawMark(bandLo, false);
        if (bandHi > GAUGE_NONE + 1.0f) drawMark(bandHi, false);
        if (target > GAUGE_NONE + 1.0f) drawMark(target, true);
        
        // Min/Max etiketleri - sadece 1 kez
        _tft.setTextFont(1);
        _tft.setTextDatum(TC_DATUM);
        _tft.setTextColor(COL_DIM, COL_BG);
        char buf[8];
        sprintf(buf, "%.0f", minVal);
        _tft.drawString(buf, cx - r + 8, cy + 6);
        sprintf(buf, "%.0f", maxVal);
        _tft.drawString(buf, cx + r - 8, cy + 6);
        
        // Birim - sadece 1 kez.
        // Sapma satiri gosterilecekse ayni yeri o kullanir (birimi kendi
        // icinde yazar), burada cizmiyoruz; yoksa ust uste binerdi.
        if (!showDev) {
            _tft.setTextFont(2);
            _tft.setTextDatum(MC_DATUM);
            _tft.setTextColor(COL_DIM, COL_BG);
            _tft.drawString(unit, cx, cy + 42);
        }
        
        // Etiket (ust) - sadece 1 kez
        _tft.setTextFont(2);
        _tft.setTextColor(COL_TEXT, COL_BG);
        _tft.drawString(label, cx, cy - r - 12);
    }
    
    // Deger yayi (renkli) - her zaman guncellenir
    float ratio = (value - minVal) / (maxVal - minVal);
    ratio = constrain(ratio, 0.0f, 1.0f);
    int sweepDeg = (int)(ratio * 180);

    // Kabul edilebilir bant. Yay her karede yeniden cizildigi icin turuncu
    // sinir cizgilerinin yay uzerine denk gelen kismi siliniyor; bandi
    // ibrenin ulasmadigi bolgede koyu gri yerine soluk yesil cizerek kalici
    // hale getiriyoruz. Boylece "normal aralik" ibre nerede olursa olsun
    // gorunur kalir.
    int bandDegLo = -1, bandDegHi = -1;
    if (bandLo > GAUGE_NONE + 1.0f && bandHi > GAUGE_NONE + 1.0f) {
        float lo = fminf(bandLo, bandHi);
        float hi = fmaxf(bandLo, bandHi);
        float rLo = constrain((lo - minVal) / (maxVal - minVal), 0.0f, 1.0f);
        float rHi = constrain((hi - minVal) / (maxVal - minVal), 0.0f, 1.0f);
        bandDegLo = (int)(rLo * 180.0f);
        bandDegHi = (int)(rHi * 180.0f);
    }

    for (int i = 0; i <= 180; i += 3) {
        float angle = (180.0f - i) * DEG2RAD;
        int x1 = cx + cos(angle) * (r - 6);
        int y1 = cy - sin(angle) * (r - 6);
        int x2 = cx + cos(angle) * r;
        int y2 = cy - sin(angle) * r;
        uint16_t offCol = (bandDegLo >= 0 && i >= bandDegLo && i <= bandDegHi)
                            ? 0x0320      // soluk yesil = normal aralik
                            : 0x2104;     // koyu gri    = aralik disi
        uint16_t col = (i <= sweepDeg) ? color : offCol;
        _tft.drawLine(x1, y1, x2, y2, col);
    }
    
    // Merkez daire (ibre merkezi)
    _tft.fillCircle(cx, cy, 5, COL_TEXT);
    
    // Deger (buyuk, ortada) - textPadding ile uzerine yaz
    _tft.setTextFont(4);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(color, COL_BG);
    _tft.setTextPadding(60);
    char buf[8];
    sprintf(buf, "%.1f", value);
    _tft.drawString(buf, cx, cy + 22);
    _tft.setTextPadding(0);

    // Sapma satiri: set noktasindan fark (orn. "-0.3 C").
    // Kulucka'da 37.5 mi 37.8 mi oldugunu okuyup kafadan cikarmak yerine
    // sapmayi dogrudan gormek cok daha hizli. Renk ISA-101 mantigiyla:
    // normal aralikta soluk gri (dikkat cekmesin), disina cikinca kadran
    // rengi (goz oraya gitsin).
    if (showDev) {
        float dev = value - devRef;
        bool inBand = true;
        if (bandLo > GAUGE_NONE + 1.0f && bandHi > GAUGE_NONE + 1.0f) {
            inBand = (value >= fminf(bandLo, bandHi) && value <= fmaxf(bandLo, bandHi));
        }
        char devBuf[16];
        snprintf(devBuf, sizeof(devBuf), "%+.1f %s", dev, unit);

        _tft.setTextFont(2);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(inBand ? COL_DIM : color, COL_BG);
        _tft.setTextPadding(64);
        _tft.drawString(devBuf, cx, cy + 42);
        _tft.setTextPadding(0);
    }
}

void DisplayManager::drawGraph(const DisplayData &data) {
    const int cardMargin = 4;
    const int cardPadX = 8;
    const int cardW = SCR_W - cardMargin * 2;
    
    // ========== UST KART: PROFIL BILGISI ==========
    const int hdrH = 42;
    if (_bgDraw) {
        _tft.fillRoundRect(cardMargin, 2, cardW, hdrH, 6, COL_CARD);
        _tft.drawRoundRect(cardMargin, 2, cardW, hdrH, 6, 0x3186);
        _prevTemp = -999;
        _prevHum = -999;
    }
    
    // Hayvan ikonu (sol) - profil adi ile eslestirilir (index sirasindan bagimsiz)
    // Her hayvanin kendi rengi var (tavuk kahve, kaz beyaz, papagan yesil, ari sari vb.)
    if (_bgDraw) {
        const AnimalIconEntry* entry = getAnimalIconEntryByName(data.profileName);
        _tft.drawXBitmap(cardMargin + 6, 7, entry->icon, ICON_WIDTH, ICON_HEIGHT, entry->color);
    }
    
    // Profil adi ve gun bilgisi (ikonun yaninda)
    _tft.setTextFont(2);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(100);
    _tft.drawString(data.profileName ? data.profileName : "---", cardMargin + ICON_WIDTH + 14, 8);
    
    _tft.setTextFont(1);
    _tft.setTextColor(COL_ORANGE, COL_CARD);
    char dayInfo[24];
    sprintf(dayInfo, "Gun %d/%d | Kalan: %d", data.currentDay, data.totalDays, data.remainingDays);
    _tft.drawString(dayInfo, cardMargin + ICON_WIDTH + 14, 26);
    
    // secgem.com (sag ust)
    _tft.setTextFont(2);
    _tft.setTextDatum(TR_DATUM);
    _tft.setTextColor(COL_CYAN, COL_CARD);
    _tft.drawString("secgem.com", SCR_W - cardMargin - cardPadX, 6);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextPadding(0);
    
    // ========== GAUGE ALANI ==========
    // Ust satir: Sicaklik (sol) + Nem (sag) yan yana
    // Alt satir: CO2 (ortada)
    const int GAUGE_R = 38;  // Ust gauge radius
    const int GAUGE_R_CO2 = 32;  // CO2 gauge biraz kucuk
    const int GAUGE_Y1 = hdrH + 21 + GAUGE_R;  // Ust satir Y (+13px asagi)
    const int GAUGE1_X = SCR_W / 4;           // Sicaklik (sol)
    const int GAUGE2_X = 3 * SCR_W / 4;       // Nem (sag)
    const int GAUGE_Y2 = GAUGE_Y1 + GAUGE_R + 8 + GAUGE_R_CO2;  // Alt satir Y
    const int GAUGE3_X = SCR_W / 2;           // CO2 (orta)
    
    // Deger degisti mi kontrol et
    bool tempChanged = (fabs(data.temperature - _prevTemp) >= 0.1f);
    bool humChanged = (fabs(data.humidity - _prevHum) >= 0.1f);
    bool co2Changed = (data.co2 != _prevCO2);
    
    // ---- Kadran olcek sinirlari (uyarlanabilir) ----
    // Normalde hedefin +/-3 C'lik dar bir penceresi kullanilir; kulucka
    // calisirken bu yuksek cozunurluk verir.
    //
    // Ancak ISINMA sirasinda olcum bu pencerenin tamamen disinda kalir
    // (orn. hedef 37.8, oda 28). drawGauge oranı constrain() ile 0'a
    // kirptigi icin ibre 28 -> 34.8 arasindaki tum yolculuk boyunca
    // sifirda cakili durur; kullanici hedefe yaklasmayi izleyemez.
    //
    // Cozum: olcum disarda kaldiginda pencereyi olcumu icine alacak sekilde
    // genislet. Genisletme 5'er derecelik adimlarla yapilir; her okumada
    // olcegin kaymasini onler, ibre gercekten hareket eder.
    float tempMin = data.targetTemp - 3.0f;
    float tempMax = data.targetTemp + 3.0f;
    if (data.temperature < tempMin) tempMin = floorf(data.temperature / 5.0f) * 5.0f;
    if (data.temperature > tempMax) tempMax = ceilf(data.temperature / 5.0f) * 5.0f;

    float humMin = data.targetHumLow - 10.0f;
    float humMax = data.targetHumHigh + 10.0f;
    if (data.humidity < humMin) humMin = floorf(data.humidity / 10.0f) * 10.0f;
    if (data.humidity > humMax) humMax = ceilf(data.humidity / 10.0f) * 10.0f;
    if (humMin < 0.0f)   humMin = 0.0f;
    if (humMax > 100.0f) humMax = 100.0f;

    // Olcek degistiyse min/max etiketleri ve hedef isaretcisi yenilenmeli.
    // Bunlar yalnizca _bgDraw'da cizildigi icin tam yeniden cizim gerekir.
    // Adimli genisletme sayesinde bu nadiren tetiklenir (titreme olmaz).
    if (tempMin != _prevTempMin || tempMax != _prevTempMax ||
        humMin  != _prevHumMin  || humMax  != _prevHumMax) {
        _prevTempMin = tempMin;  _prevTempMax = tempMax;
        _prevHumMin  = humMin;   _prevHumMax  = humMax;
        _forceRedraw = true;
    }
    
    // CO2 sinir degerleri (profil bazli)
    float co2Min = 0;
    float co2Max = (float)data.co2Critical + 1000;  // Kritik + 1000 ppm
    float co2Target = (float)data.co2Low;           // Hedef = alt limit
    
    // Sicaklik rengi
    uint16_t tempColor;
    float tempDiff = data.temperature - data.targetTemp;
    if (fabs(tempDiff) <= 0.5f) {
        tempColor = COL_GREEN;
    } else if (tempDiff > 0) {
        tempColor = COL_RED;
    } else {
        tempColor = COL_BLUE;
    }
    
    // Nem rengi
    uint16_t humColor;
    if (data.humidity >= data.targetHumLow && data.humidity <= data.targetHumHigh) {
        humColor = COL_GREEN;
    } else if (data.humidity > data.targetHumHigh) {
        humColor = COL_CYAN;
    } else {
        humColor = COL_ORANGE;
    }
    
    // CO2 rengi (profil bazli dinamik limitler)
    uint16_t co2Color;
    if (!data.co2Valid) {
        // Sensor yok/okunamiyor: gri goster. Aksi halde 0 ppm "co2Low'un
        // altinda" sayilip YESIL cizilir ve "hava mukemmel" izlenimi verir;
        // oysa hicbir olcum yoktur. Yanlis guven en tehlikeli gostergedir.
        co2Color = COL_DIM;
    } else if (data.co2 >= data.co2Critical) {
        co2Color = COL_RED;      // Kritik
    } else if (data.co2 >= data.co2High) {
        co2Color = COL_ORANGE;   // Yuksek
    } else if (data.co2 <= data.co2Low) {
        co2Color = COL_GREEN;    // Normal
    } else {
        co2Color = COL_YELLOW;   // Uyari
    }
    
    bool tempColorChanged = (tempColor != _prevTempColor);
    bool humColorChanged = (humColor != _prevHumColor);
    bool co2ColorChanged = (co2Color != _prevCO2Color);
    
    // Sicaklik gauge (sol ust)
    // Sicaklikta TEK dogru deger vardir (set noktasi) + cevresinde alarm
    // uretmeyen tolerans bandi. Bu yuzden kalin set noktasi cizgisi ile
    // birlikte +/- ALARM_TEMP_TOLERANCE bandi gosterilir: operator hem
    // "nereye gitmeli" hem "ne zaman alarm calar" bilgisini ayni anda gorur.
    if (tempChanged || tempColorChanged || _bgDraw) {
        drawGauge(GAUGE1_X, GAUGE_Y1, GAUGE_R,
                  data.temperature, tempMin, tempMax, data.targetTemp,
                  tempColor, "SICAKLIK", "C",
                  data.targetTemp - ALARM_TEMP_TOLERANCE,
                  data.targetTemp + ALARM_TEMP_TOLERANCE,
                  data.targetTemp);   // sapma satiri: set noktasindan fark
        _prevTemp = data.temperature;
        _prevTempColor = tempColor;
    }
    
    // Nem gauge (sag ust)
    // Nem TEK bir hedef degil ARALIK ister (orn. %55-65). Ortalamayi tek
    // isaretci olarak gostermek yaniltiyordu: %56 da %64 de hedefte olmasina
    // ragmen ibre isaretciden uzak gorunuyordu. Artik alt ve ust sinir ayri
    // cizgilerle, aralarindaki bant da soluk yesil ile gosteriliyor.
    if (humChanged || humColorChanged || _bgDraw) {
        drawGauge(GAUGE2_X, GAUGE_Y1, GAUGE_R,
                  data.humidity, humMin, humMax,
                  GAUGE_NONE,          // nemde tek set noktasi yok
                  humColor, "NEM", "%",
                  data.targetHumLow, data.targetHumHigh);
        _prevHum = data.humidity;
        _prevHumColor = humColor;
    }
    
    // CO2 gauge (alt orta) - sensor yoksa birim "YOK" yazar
    if (co2Changed || co2ColorChanged || _bgDraw) {
        // CO2'de "dusuk iyidir": set noktasi yok, tabandan alt limite kadar
        // olan bolge kabul edilebilir bant.
        drawGauge(GAUGE3_X, GAUGE_Y2, GAUGE_R_CO2,
                  (float)data.co2, co2Min, co2Max, GAUGE_NONE,
                  co2Color, "CO2", data.co2Valid ? "ppm" : "YOK",
                  co2Min, co2Target);
        _prevCO2 = data.co2;
        _prevCO2Color = co2Color;
    }
    
    // ========== ALT KART: DETAY BILGISI ==========
    const int INFO_Y = GAUGE_Y2 + GAUGE_R_CO2 + 14;
    const int INFO_H = 60;  // 3. satir (IR yumurta) icin genisletildi
    
    if (_bgDraw) {
        _tft.fillRoundRect(cardMargin, INFO_Y, cardW, INFO_H, 6, COL_CARD);
        _tft.drawRoundRect(cardMargin, INFO_Y, cardW, INFO_H, 6, 0x3186);
        // IR satiri icin ince ayirici
        _tft.drawFastHLine(cardMargin + 4, INFO_Y + 41, cardW - 8, 0x2104);
    }
    
    // Ust satir: Evre ve CO2 limitleri
    _tft.setTextFont(2);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_GREEN, COL_CARD);
    _tft.setTextPadding(80);
    _tft.drawString(data.phaseName ? data.phaseName : "---", cardPadX + cardMargin, INFO_Y + 6);
    
    _tft.setTextDatum(TR_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(120);
    char co2LimitStr[32];
    sprintf(co2LimitStr, "CO2: %u/%u ppm", data.co2High, data.co2Critical);
    _tft.drawString(co2LimitStr, SCR_W - cardMargin - cardPadX, INFO_Y + 6);
    
    // Alt satir: Hedef degerler ve yumurta sayisi
    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(100);
    char targetStr[32];
    sprintf(targetStr, "%.1fC | %%%d-%%%d", data.targetTemp, (int)data.targetHumLow, (int)data.targetHumHigh);
    _tft.drawString(targetStr, cardPadX + cardMargin, INFO_Y + 28);
    
    _tft.setTextDatum(TR_DATUM);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(100);
    char eggStr[24];
    sprintf(eggStr, "Yumurta: %u", (unsigned)data.eggCount);
    _tft.drawString(eggStr, SCR_W - cardMargin - cardPadX, INFO_Y + 28);

    // Alt satir: Yumurta IR sicaklik (3. satir)
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(80);
    _tft.drawString("Yumurta Sicaklik:", cardPadX + cardMargin, INFO_Y + 46);
    
    char eggTempStr[18];
    if (data.eggTempValid) {
        // °C sembolu: extended ASCII 0xB0 (TFT_eSPI font 2/4 destekler)
        snprintf(eggTempStr, sizeof(eggTempStr), "%.1f \xB0""C", data.eggTemp);
        _tft.setTextColor(COL_YELLOW, COL_CARD);
    } else {
        snprintf(eggTempStr, sizeof(eggTempStr), "Baglanti yok");
        _tft.setTextColor(COL_DIM, COL_CARD);
    }
    _tft.setTextDatum(TR_DATUM);
    _tft.setTextPadding(120);
    _tft.drawString(eggTempStr, SCR_W - cardMargin - cardPadX, INFO_Y + 46);
    _tft.setTextPadding(0);
    _tft.setTextDatum(TL_DATUM);
}

void DisplayManager::drawHeader(const DisplayData &data) {
    if (_bgDraw) {
        _tft.fillRect(0, HDR_Y, SCR_W, HDR_H, COL_CARD);
    }

    _tft.setTextFont(2);
    _tft.setTextSize(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(100);
    _tft.drawString(data.profileName ? data.profileName : "---", 5, HDR_Y + 1);

    char sub[40];
    int rd = data.remainingDays < 0 ? 0 : data.remainingDays;
    if (data.hatchDate.length() > 0) {
        snprintf(sub, sizeof(sub), "Gun %d/%d | C:%s",
                 data.currentDay, data.totalDays, data.hatchDate.c_str());
    } else {
        snprintf(sub, sizeof(sub), "Gun %d/%d | %s | %dg",
                 data.currentDay, data.totalDays,
                 data.phaseName ? data.phaseName : "---", rd);
    }
    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString(sub, 5, HDR_Y + 18);

    int si = constrain(data.systemState, 0, 6);
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
    const int bw = 56;
    const int gap = (SCR_W - 4 * bw - 4) / 3;
    int x1 = 2, x2 = x1 + bw + gap, x3 = x2 + bw + gap, x4 = x3 + bw + gap;

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
    int si = constrain(data.systemState, 0, 6);
    _tft.setTextFont(2);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(bw - 4);
    _tft.drawString(ST_SHORT[si], x3 + bw / 2, INF_Y + 14);
    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(0);
    _tft.drawString(data.profileName ? data.profileName : "---", x3 + bw / 2, INF_Y + 35);

    // Yumurta
    if (_bgDraw) _tft.fillRoundRect(x4, INF_Y, bw, INF_H, 4, COL_CARD);
    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("YUM", x4 + bw / 2, INF_Y + 3);
    _tft.setTextFont(2);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(bw - 4);
    char ec[10];
    snprintf(ec, sizeof(ec), "%u", (unsigned)data.eggCount);
    _tft.drawString(ec, x4 + bw / 2, INF_Y + 14);
    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.setTextPadding(0);
    _tft.drawString("adet", x4 + bw / 2, INF_Y + 35);
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
    _tft.drawString(SENSOR_NAME, cx1, SNS_Y + 2);
    // Uc durum ayrimi: OK (calisiyor) / HATA (takili ama cevap vermiyor) /
    // YOK (donanim hic takili degil). Takili olmayan bir sensore surekli
    // kirmizi HATA yazmak gercek arizalari da siradanlastirir.
    _tft.setTextColor(data.sensor1OK ? COL_GREEN
                                     : (data.sensor1Present ? COL_RED : COL_DIM), COL_CARD);
    _tft.drawString(data.sensor1OK ? "OK"
                                   : (data.sensor1Present ? "HATA" : "YOK"), cx1, SNS_Y + 13);

    int cx2 = 2 + sw + sw / 2;
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString(SENSOR_BUS_NAME, cx2, SNS_Y + 2);
    _tft.setTextColor(data.sensor2OK ? COL_GREEN
                                     : (data.sensor2Present ? COL_RED : COL_DIM), COL_CARD);
    _tft.drawString(data.sensor2OK ? "OK"
                                   : (data.sensor2Present ? "HATA" : "YOK"), cx2, SNS_Y + 13);

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
        if (txt.length() > 22) txt = txt.substring(0, 19) + "...";
        _tft.drawString(txt, 8, ALM_Y + 12);

        // Alt satir: dokunma ipucu
        _tft.setTextFont(1);
        _tft.setTextColor(COL_DIM, COL_CARD);
        _tft.setTextPadding(SCR_W - 16);
        _tft.drawString("Dokun: SUSTUR", 8, ALM_Y + ALM_H - 10);
        _tft.setTextFont(2);
    } else {
        _tft.setTextColor(COL_GREEN, COL_CARD);
        _tft.drawString("Alarm yok", 8, ALM_Y + ALM_H / 2);
    }
    _tft.setTextPadding(0);
}

// ==================== SAYFA: KONTROL ====================

void DisplayManager::drawControl(const DisplayData &data) {
    // Dropdown aciksa tam sayfa profil listesi goster
    if (_profileListOpen) {
        if (_bgDraw) drawProfileList();
        return;
    }
    // Editor acik ise tam sayfa profil duzenleme goster
    if (_profileEditorOpen) {
        drawProfileEditor(data);
        return;
    }

    const int cardMargin = 4;
    const int cardPadX = 8;
    const int cardW = SCR_W - cardMargin * 2;

    // ========== BASLIK KARTI ==========
    if (_bgDraw) {
        _tft.fillRoundRect(cardMargin, 2, cardW, 28, 6, COL_CARD);
        _tft.drawRoundRect(cardMargin, 2, cardW, 28, 6, 0x3186);
    }
    _tft.setTextFont(1);
    _tft.setTextSize(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_CYAN, COL_CARD);
    _tft.drawString("! SISTEM KONTROL", cardPadX + cardMargin, 10);

    // ========== KONTROL BUTONLARI (2x2) ==========
    {
        const int btnGap = 4;
        const int btnStartY = 34;
        const int btnH2 = 38;
        const int btnW2 = (cardW - btnGap) / 2;
        int lx = cardMargin;
        int rx = cardMargin + btnW2 + btnGap;

        int st = data.systemState;
        uint16_t cStart  = (st == 0 || st == 3 || st == 4 || st == 5) ? COL_GREEN : 0x2104;
        uint16_t cPause  = (st == 1 || st == 2) ? COL_ORANGE : 0x2104;
        uint16_t cResume = (st == 3) ? COL_CYAN : 0x2104;
        uint16_t cStop   = (st != 0) ? COL_RED : 0x2104;

        drawButton(lx, btnStartY, btnW2, btnH2, "BASLAT", cStart, COL_TEXT);
        drawButton(rx, btnStartY, btnW2, btnH2, "DURAKLAT", cPause, COL_TEXT);
        drawButton(lx, btnStartY + btnH2 + btnGap, btnW2, btnH2, "DEVAM", cResume, COL_TEXT);
        drawButton(rx, btnStartY + btnH2 + btnGap, btnW2, btnH2, "DURDUR", cStop, COL_TEXT);
    }

    // ========== PID PARAMETRELERI ==========
    int py = 122;
    const int pidH = 44;
    if (_bgDraw) {
        _tft.fillRoundRect(cardMargin, py, cardW, pidH, 6, COL_CARD);
        _tft.drawRoundRect(cardMargin, py, cardW, pidH, 6, 0x3186);
    }
    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_CYAN, COL_CARD);
    _tft.drawString("* PID", cardPadX + cardMargin, py + 6);

    _tft.setTextFont(2);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextDatum(TL_DATUM);
    
    // Kp
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("Kp", cardPadX + cardMargin, py + 24);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    char kpStr[10]; snprintf(kpStr, sizeof(kpStr), "%.1f", data.kp);
    _tft.drawString(kpStr, cardPadX + cardMargin + 22, py + 24);
    
    // Ki
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("Ki", cardPadX + cardMargin + 70, py + 24);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    char kiStr[10]; snprintf(kiStr, sizeof(kiStr), "%.2f", data.ki);
    _tft.drawString(kiStr, cardPadX + cardMargin + 88, py + 24);
    
    // Kd
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("Kd", cardPadX + cardMargin + 145, py + 24);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    char kdStr[10]; snprintf(kdStr, sizeof(kdStr), "%.1f", data.kd);
    _tft.drawString(kdStr, cardPadX + cardMargin + 165, py + 24);

    // ========== NEM ESIKLERI ==========
    int ny = py + pidH + 4;
    const int nemH = 44;
    if (_bgDraw) {
        _tft.fillRoundRect(cardMargin, ny, cardW, nemH, 6, COL_CARD);
        _tft.drawRoundRect(cardMargin, ny, cardW, nemH, 6, 0x3186);
    }
    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_CYAN, COL_CARD);
    _tft.drawString("% NEM ESIKLERI", cardPadX + cardMargin, ny + 6);

    _tft.setTextFont(2);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("Min:", cardPadX + cardMargin, ny + 24);
    _tft.setTextColor(COL_BLUE, COL_CARD);
    char minH[8]; snprintf(minH, sizeof(minH), "%%%d", (int)data.targetHumLow);
    _tft.drawString(minH, cardPadX + cardMargin + 35, ny + 24);
    
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("Max:", cardPadX + cardMargin + 90, ny + 24);
    _tft.setTextColor(COL_ORANGE, COL_CARD);
    char maxH[8]; snprintf(maxH, sizeof(maxH), "%%%d", (int)data.targetHumHigh);
    _tft.drawString(maxH, cardPadX + cardMargin + 130, ny + 24);

    // ========== ALT SATIR: PROFIL SEC + PROFIL DUZENLE ==========
    int cy = ny + nemH + 4;
    const int bottomH = 28;
    const int halfW = (cardW - 4) / 2;

    // Profil Sec butonu (sol)
    if (_bgDraw) _tft.fillRoundRect(cardMargin, cy, halfW, bottomH, 6, COL_BLUE);
    _tft.setTextFont(2);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_TEXT, COL_BLUE);
    _tft.setTextPadding(halfW - 8);
    char profBtn[24];
    // Override varsa profil adi yanina "*" ekle (rozet)
    bool hasOverride = _customProfileStorage &&
                       _customProfileStorage->hasProfileOverride(
                           (uint8_t)_pendingProfileIdx);
    snprintf(profBtn, sizeof(profBtn), "%s%s v",
             data.profileName ? data.profileName : "Sec",
             hasOverride ? " *" : "");
    _tft.drawString(profBtn, cardMargin + halfW / 2, cy + bottomH / 2);
    _tft.setTextPadding(0);

    // Profili Duzenle butonu (sag) — turuncu, vurgu icin
    int px = cardMargin + halfW + 4;
    if (_bgDraw) _tft.fillRoundRect(px, cy, halfW, bottomH, 6, COL_ORANGE);
    _tft.setTextFont(2);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_TEXT, COL_ORANGE);
    _tft.setTextPadding(halfW - 8);
    _tft.drawString("Profili Duzenle", px + halfW / 2, cy + bottomH / 2);
    _tft.setTextPadding(0);

    // ========== ALT SATIR: CEVIRME DURUMU (sol) + TEMIZLIK BTN (sag) ==========
    int ty = cy + bottomH + 4;     // 250
    const int turnH = 28;
    const int halfW2 = (cardW - 4) / 2;

    // -- Sol: Cevirme durumu --
    if (_bgDraw) {
        _tft.fillRoundRect(cardMargin, ty, halfW2, turnH, 6, COL_CARD);
        _tft.drawRoundRect(cardMargin, ty, halfW2, turnH, 6, 0x3186);
    }
    _tft.setTextFont(1);
    _tft.setTextDatum(ML_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("Cevirme:", cardPadX + cardMargin, ty + turnH / 2);
    _tft.setTextDatum(MR_DATUM);
    _tft.setTextFont(2);
    _tft.setTextColor(data.turningEnabled ? COL_GREEN : COL_RED, COL_CARD);
    _tft.setTextPadding(40);
    _tft.drawString(data.turningEnabled ? "ON" : "OFF",
                    cardMargin + halfW2 - 8, ty + turnH / 2);
    _tft.setTextPadding(0);

    // -- Sag: Temizlik (Bakim) butonu --
    // Aktifken kirmizi "DURDUR", pasifken yesil "TEMIZLIK"
    int clnX = cardMargin + halfW2 + 4;
    uint16_t clnBg = data.cleaningActive ? COL_RED : COL_GREEN;
    // Buton arkaplani her cizimde guncellensin (aktif/pasif rengi degistigi icin)
    _tft.fillRoundRect(clnX, ty, halfW2, turnH, 6, clnBg);
    _tft.drawRoundRect(clnX, ty, halfW2, turnH, 6, 0x3186);
    _tft.setTextFont(2);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_TEXT, clnBg);
    _tft.setTextPadding(halfW2 - 8);
    if (data.cleaningActive) {
        // Kalan dakika
        unsigned long mins = (data.cleaningRemainMs + 59999UL) / 60000UL;
        char lbl[16];
        snprintf(lbl, sizeof(lbl), "DURDUR %lud", mins);
        _tft.drawString(lbl, clnX + halfW2 / 2, ty + turnH / 2);
    } else {
        _tft.drawString("TEMIZLIK", clnX + halfW2 / 2, ty + turnH / 2);
    }
    _tft.setTextPadding(0);
    _tft.setTextDatum(TL_DATUM);
}

// ==================== SAYFA: PROFIL ====================

void DisplayManager::drawProfile(const DisplayData &data) {
    const int cardMargin = 4;
    const int cardPadX = 6;
    const int cardW = SCR_W - cardMargin * 2;
    
    // ========== BASLIK KARTI ==========
    const int hdrH = 62;
    if (_bgDraw) {
        _tft.fillRoundRect(cardMargin, 2, cardW, hdrH, 6, COL_CARD);
        _tft.drawRoundRect(cardMargin, 2, cardW, hdrH, 6, 0x3186);
    }

    // Profil adi (buyuk, sol)
    _tft.setTextFont(2);
    _tft.setTextSize(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_CYAN, COL_CARD);
    _tft.drawString("PROFIL", cardPadX + cardMargin, 4);
    
    const char* pn = data.profileName ? data.profileName : "---";
    _tft.setTextFont(4);  // Buyuk font
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(140);
    _tft.drawString(pn, cardPadX + cardMargin, 20);
    _tft.setTextPadding(0);

    // Gun bilgisi (sag ust)
    _tft.setTextFont(2);
    _tft.setTextDatum(TR_DATUM);
    _tft.setTextColor(COL_ORANGE, COL_CARD);
    char dayInfo[20];
    snprintf(dayInfo, sizeof(dayInfo), "Gun %d/%d", data.currentDay, data.totalDays);
    _tft.drawString(dayInfo, SCR_W - cardMargin - cardPadX, 4);
    
    // Aktif evre (sag orta)
    _tft.setTextColor(COL_GREEN, COL_CARD);
    _tft.drawString(data.phaseName ? data.phaseName : "---", SCR_W - cardMargin - cardPadX, 24);
    
    // CO2 limitleri (alt satir - profil bazli dinamik)
    _tft.setTextFont(2);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("CO2:", cardPadX + cardMargin, 46);
    _tft.setTextColor(COL_GREEN, COL_CARD);
    char co2Str[32];
    snprintf(co2Str, sizeof(co2Str), "<%u", data.co2Low);
    _tft.drawString(co2Str, cardPadX + cardMargin + 38, 46);
    _tft.setTextColor(COL_YELLOW, COL_CARD);
    snprintf(co2Str, sizeof(co2Str), "<%u", data.co2High);
    _tft.drawString(co2Str, cardPadX + cardMargin + 90, 46);
    _tft.setTextColor(COL_RED, COL_CARD);
    snprintf(co2Str, sizeof(co2Str), ">%u", data.co2Critical);
    _tft.drawString(co2Str, cardPadX + cardMargin + 142, 46);
    _tft.setTextDatum(TL_DATUM);

    // Evre listesi
    const AnimalProfile* prof = data.profile;
    if (!prof) {
        _tft.setTextFont(2);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(COL_DIM, COL_BG);
        _tft.drawString("Profil bulunamadi", SCR_W / 2, 120);
        return;
    }

    // ========== EVRE KARTLARI ==========
    // Faz sayisina gore kart yuksekligi dinamik (4 faz icin kompakt)
    int totalCards = prof->phaseCount;
    const int phH = (totalCards >= 4) ? 44 : 56;
    const int gap = (totalCards >= 4) ? 2 : 3;
    // Faz icerigi koordinatlari da dinamik (kompakt mod)
    const int phRow1Y = (totalCards >= 4) ? 3  : 4;    // Baslik + gun araligi
    const int phRow2Y = (totalCards >= 4) ? 18 : 26;   // Sicaklik + nem
    const int phRow3Y = (totalCards >= 4) ? 30 : 40;   // Cevirme

    int startY = hdrH + 6;
    int maxVisible = (PAGE_H - startY) / (phH + gap);

    int maxScroll = totalCards - maxVisible;
    if (maxScroll < 0) maxScroll = 0;
    if (_scrollOffset > maxScroll) _scrollOffset = maxScroll;

    for (uint8_t idx = 0; idx < maxVisible && (idx + _scrollOffset) < totalCards; idx++) {
        uint8_t i = idx + _scrollOffset;
        const IncubationPhase &ph = prof->phases[i];
        if (ph.startDay == 0 && ph.endDay == 0) continue;

        int cy = startY + idx * (phH + gap);
        if (cy + phH > PAGE_H) break;

        bool isActive = (i == data.currentPhaseIndex);
        uint16_t cardBg = isActive ? 0x0A2A : COL_CARD;

        if (_bgDraw) {
            _tft.fillRoundRect(cardMargin, cy, cardW, phH, 5, cardBg);
            // Sol kenar renk gostergesi
            uint16_t accent = isActive ? COL_GREEN : COL_PURPLE;
            _tft.fillRoundRect(cardMargin, cy + 2, 5, phH - 4, 2, accent);
            if (isActive) {
                _tft.drawRoundRect(cardMargin, cy, cardW, phH, 5, COL_GREEN);
            }
        }

        // Evre adi (sol ust) - Font 2
        _tft.setTextFont(2);
        _tft.setTextDatum(TL_DATUM);
        _tft.setTextColor(isActive ? COL_GREEN : COL_TEXT, cardBg);
        _tft.setTextPadding(100);
        _tft.drawString(ph.phaseName, cardPadX + cardMargin + 8, cy + phRow1Y);
        _tft.setTextPadding(0);

        // Gun araligi (sag ust) - Font 2
        _tft.setTextDatum(TR_DATUM);
        _tft.setTextColor(COL_ORANGE, cardBg);
        char dayRange[16];
        snprintf(dayRange, sizeof(dayRange), "Gun %d-%d", ph.startDay, ph.endDay);
        _tft.drawString(dayRange, SCR_W - cardMargin - cardPadX, cy + phRow1Y);
        _tft.setTextDatum(TL_DATUM);

        // Sicaklik (sol alt) - Font 2
        _tft.setTextColor(COL_DIM, cardBg);
        _tft.drawString("Sic:", cardPadX + cardMargin + 8, cy + phRow2Y);
        _tft.setTextColor(COL_TEXT, cardBg);
        char tempStr[20];
        if (ph.tempEnd > 0.0f && ph.tempEnd != ph.temperature) {
            snprintf(tempStr, sizeof(tempStr), "%.1f>%.1f", ph.temperature, ph.tempEnd);
        } else {
            snprintf(tempStr, sizeof(tempStr), "%.1fC", ph.temperature);
        }
        _tft.drawString(tempStr, cardPadX + cardMargin + 38, cy + phRow2Y);

        // Nem (sag orta) - Font 2
        _tft.setTextDatum(TR_DATUM);
        _tft.setTextColor(COL_DIM, cardBg);
        char humStr[20];
        snprintf(humStr, sizeof(humStr), "Nem: %%%d-%%%d", (int)ph.humidityLow, (int)ph.humidityHigh);
        _tft.setTextColor(COL_CYAN, cardBg);
        _tft.drawString(humStr, SCR_W - cardMargin - cardPadX, cy + phRow2Y);
        _tft.setTextDatum(TL_DATUM);

        // Cevirme (sol alt) - Font 2
        _tft.setTextColor(ph.turningEnabled ? COL_GREEN : COL_RED, cardBg);
        _tft.drawString(ph.turningEnabled ? "Cevirme: ACIK" : "Cevirme: KAPALI",
                         cardPadX + cardMargin + 8, cy + phRow3Y);

        // Cooling/Sprey indikatoru (sag alt) - 4 fazli kompakt modda
        if (totalCards >= 4 && (ph.coolingEnabled || ph.sprayingEnabled)) {
            _tft.setTextDatum(TR_DATUM);
            _tft.setTextColor(COL_PURPLE, cardBg);
            const char* coolSym = ph.sprayingEnabled ? "* Sogut+Sprey" : "* Sogutma";
            _tft.drawString(coolSym, SCR_W - cardMargin - cardPadX, cy + phRow3Y);
            _tft.setTextDatum(TL_DATUM);
        }
    }

    // Scroll gostergesi
    if (maxScroll > 0) {
        _tft.setTextFont(2);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(COL_DIM, COL_BG);
        _tft.setTextPadding(60);
        char sc[12];
        snprintf(sc, sizeof(sc), "%d/%d", _scrollOffset + 1, maxScroll + 1);
        _tft.drawString(sc, SCR_W / 2, PAGE_H - 12);
        _tft.setTextPadding(0);
        _tft.setTextDatum(TL_DATUM);
    }

    // ========== DOL KONTROLU (CANDLING) KARTI ==========
    // Evre kartlarinin altinda kontrol gunlerini goster
    // 4 fazli profilde KOMPAKT mod (~32px tek satir), 2-3 fazli profilde detayli liste
    if (data.candlingLockdownDay > 0) {
        CandlingSchedule cs;
        buildCandlingSchedule(prof, cs);

        bool compactMode = (totalCards >= 4);

        int candY = startY + (min((int)prof->phaseCount, maxVisible)) * (phH + gap) + 2;
        int cardH = compactMode ? 32 : (20 + (cs.checkCount * 12) + 8);

        if (candY + cardH < PAGE_H) {
            // Bugun kontrol gunu mu? (vurgu icin)
            bool todayIsCheck = false;
            int  todayCheckIdx = -1;
            for (int k = 0; k < cs.checkCount; k++) {
                if (cs.checkDays[k] == data.currentDay) {
                    todayIsCheck = true;
                    todayCheckIdx = k;
                    break;
                }
            }

            // Kart arka plan (bugun kontrol ise vurgulu)
            uint16_t cardColor = todayIsCheck ? 0x4A20 : COL_CARD;   // koyu turuncu / kart
            if (_bgDraw) {
                _tft.fillRoundRect(cardMargin, candY, cardW, cardH, 5, cardColor);
                _tft.fillRoundRect(cardMargin, candY + 2, 5, cardH - 4, 2, COL_YELLOW);
                _tft.drawRoundRect(cardMargin, candY, cardW, cardH, 5, 0x8482);
            }

            if (compactMode) {
                // KOMPAKT MOD (4 fazli profil): tek satir kart + gunler
                // Format: "Dol: G5 G10 G15 L18 [BUGUN!]" — sigistirilmis
                _tft.setTextFont(2);
                _tft.setTextDatum(TL_DATUM);
                _tft.setTextColor(COL_YELLOW, cardColor);
                _tft.drawString("Dol:", cardPadX + cardMargin + 8, candY + 4);

                // Gunler yan yana
                _tft.setTextFont(1);
                int gx = cardPadX + cardMargin + 36;
                for (int k = 0; k < cs.checkCount; k++) {
                    int controlDay = cs.checkDays[k];
                    bool isToday = (data.currentDay == controlDay);
                    bool isPast  = (data.currentDay > controlDay);

                    if (isToday) {
                        _tft.setTextColor(COL_YELLOW, cardColor);
                    } else if (isPast) {
                        _tft.setTextColor(0x3A5A, cardColor);
                    } else {
                        _tft.setTextColor(COL_CYAN, cardColor);
                    }

                    char buf[8];
                    // Son entry lockdown ise "L" prefix, digerleri "G"
                    if (k == cs.checkCount - 1) {
                        snprintf(buf, sizeof(buf), "L%u", controlDay);
                    } else {
                        snprintf(buf, sizeof(buf), "G%u", controlDay);
                    }
                    _tft.drawString(buf, gx, candY + 6);
                    gx += (controlDay >= 10 ? 24 : 20);
                }

                // Alt satir: bugunse "BUGUN!" yanip soner, degilse en yakin kalan gun
                _tft.setTextFont(1);
                if (todayIsCheck) {
                    bool blink = ((millis() / 500) % 2) == 0;
                    if (blink) {
                        _tft.setTextDatum(MR_DATUM);
                        _tft.setTextColor(COL_YELLOW, cardColor);
                        _tft.drawString("* BUGUN KONTROL ZAMANI! *",
                                       SCR_W - cardMargin - cardPadX - 4, candY + 22);
                        _tft.setTextDatum(TL_DATUM);
                    }
                } else {
                    // En yakin gelecek kontrol gunu
                    int nextDay = 0;
                    for (int k = 0; k < cs.checkCount; k++) {
                        if (cs.checkDays[k] > data.currentDay) {
                            nextDay = cs.checkDays[k];
                            break;
                        }
                    }
                    if (nextDay > 0) {
                        char nbuf[32];
                        snprintf(nbuf, sizeof(nbuf), "Sonraki: Gun %d (%d gun)",
                                 nextDay, nextDay - data.currentDay);
                        _tft.setTextDatum(MR_DATUM);
                        _tft.setTextColor(COL_DIM, cardColor);
                        _tft.drawString(nbuf, SCR_W - cardMargin - cardPadX - 4, candY + 22);
                        _tft.setTextDatum(TL_DATUM);
                    }
                }
            } else {
                // DETAYLI MOD (2-3 fazli profil): dikey liste eski tasarim
                _tft.setTextFont(2);
                _tft.setTextDatum(TL_DATUM);
                _tft.setTextColor(COL_YELLOW, cardColor);
                _tft.drawString("Dol Kontrolu", cardPadX + cardMargin + 8, candY + 3);

                _tft.setTextFont(1);
                _tft.setTextSize(1);
                int cy = candY + 18;

                bool startDateValid = (data.startYear >= 2020 && data.startYear <= 2099 &&
                                       data.startMonth >= 1 && data.startMonth <= 12 &&
                                       data.startDay >= 1 && data.startDay <= 31);
                DateTime startDt(data.startYear, data.startMonth, data.startDay);

                for (int k = 0; k < cs.checkCount; k++) {
                    int controlDay = cs.checkDays[k];
                    bool isToday = (data.currentDay == controlDay);
                    bool isPast  = (data.currentDay > controlDay);
                    int daysRemaining = controlDay - data.currentDay;

                    if (isToday)      _tft.setTextColor(COL_YELLOW, cardColor);
                    else if (isPast)  _tft.setTextColor(0x3A5A, cardColor);
                    else              _tft.setTextColor(COL_CYAN, cardColor);

                    char labelBuf[48];
                    if (startDateValid) {
                        DateTime checkDt = startDt + TimeSpan(controlDay - 1, 0, 0, 0);
                        if (k < 3) {
                            snprintf(labelBuf, sizeof(labelBuf), "%d.Kontrol: %02d.%02d.%04d",
                                     k + 1, checkDt.day(), checkDt.month(), checkDt.year());
                        } else {
                            snprintf(labelBuf, sizeof(labelBuf), "Lockdown: %02d.%02d.%04d",
                                     checkDt.day(), checkDt.month(), checkDt.year());
                        }
                    } else {
                        if (k < 3) {
                            snprintf(labelBuf, sizeof(labelBuf), "%d.Kontrol: Gun %d",
                                     k + 1, controlDay);
                        } else {
                            snprintf(labelBuf, sizeof(labelBuf), "Lockdown: Gun %d", controlDay);
                        }
                    }
                    _tft.setTextDatum(TL_DATUM);
                    _tft.drawString(labelBuf, cardPadX + cardMargin + 8, cy);

                    char dayBuf[24];
                    if (daysRemaining == 0)       snprintf(dayBuf, sizeof(dayBuf), "BUGUN!");
                    else if (daysRemaining > 0)   snprintf(dayBuf, sizeof(dayBuf), "%d gun kaldi", daysRemaining);
                    else                           snprintf(dayBuf, sizeof(dayBuf), "%d gun gecti", -daysRemaining);
                    _tft.setTextColor(COL_DIM, cardColor);
                    _tft.setTextDatum(TR_DATUM);
                    _tft.drawString(dayBuf, SCR_W - cardMargin - cardPadX - 2, cy);
                    _tft.setTextDatum(TL_DATUM);

                    cy += 12;
                }
            }

            // Font sifirla
            _tft.setTextFont(2);
            _tft.setTextSize(1);
        }
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
        // Alfabetik sirala, ama gercek index korunsun
        uint8_t realIdx = profileRealIdx(i);
        const AnimalProfile* p = ALL_PROFILES[realIdx];

        // Override durumu (kullanici bu profili duzenlemis mi?)
        bool isOverride = _customProfileStorage &&
                          _customProfileStorage->hasProfileOverride(realIdx);

        // Satir arkaplan (alternating)
        uint16_t rowBg = (idx % 2 == 0) ? COL_CARD : COL_BG;
        _tft.fillRect(0, iy, SCR_W, itemH, rowBg);

        // Sol kenar renk gostergesi
        uint16_t accent = (idx % 3 == 0) ? COL_GREEN : (idx % 3 == 1) ? COL_BLUE : COL_ORANGE;
        _tft.fillRect(0, iy + 2, 4, itemH - 4, accent);

        // Ayirici cizgi (altta)
        _tft.drawFastHLine(4, iy + itemH - 1, SCR_W - 4, COL_BAR_BG);

        // Profil adi (sol) — override varsa "*" ekle
        _tft.setTextFont(2);
        _tft.setTextSize(1);
        _tft.setTextDatum(ML_DATUM);
        _tft.setTextColor(COL_TEXT, rowBg);
        _tft.setTextPadding(0);
        char nameBuf[36];
        if (isOverride) {
            snprintf(nameBuf, sizeof(nameBuf), "%s *", p->name);
        } else {
            snprintf(nameBuf, sizeof(nameBuf), "%s", p->name);
        }
        _tft.drawString(nameBuf, 12, iy + itemH / 2);

        // Override "*" rozetinin rengini turuncu yap (vurgu)
        if (isOverride) {
            int nameW = _tft.textWidth(p->name);
            _tft.setTextColor(COL_ORANGE, rowBg);
            _tft.drawString("*", 12 + nameW + 4, iy + itemH / 2);
            _tft.setTextColor(COL_TEXT, rowBg);
        }

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

// ==================== PROFIL EDITOR (TAM EKRAN) ====================
// Aktif fazin sicaklik ve nem esiklerini ±dugmelerle ayarlar.
// Sicaklik: ±0.1 C, Nem: ±%1.
// Override yoksa otomatik olusturulur (IncubationService tarafinda).
void DisplayManager::drawProfileEditor(const DisplayData &data) {
    const int hdrH    = 32;
    const int rowH    = 42;
    const int rowGap  = 3;
    const int rowsY   = hdrH + 3;
    const int padX    = 8;
    const int btnW    = 50;
    const int btnH    = rowH - 8;
    const int rowCount = 4;
    const char* labels[4] = {
        "Sicaklik (C)",
        "Nem Alt (%)",
        "Nem Ust (%)",
        "Cev. Aralik (dk)"
    };
    uint16_t valColors[4] = { COL_ORANGE, COL_BLUE, COL_BLUE, COL_GREEN };

    bool isOverride = _customProfileStorage &&
                      _customProfileStorage->hasProfileOverride(
                          (uint8_t)data.profileIndex);

    // ---- ARKAPLAN + STATIK ELEMANLAR (sadece ilk acilista veya force yenilemede) ----
    if (_bgDraw) {
        _tft.fillRect(0, 0, SCR_W, SCR_H, COL_BG);

        // Baslik bandi
        _tft.fillRect(0, 0, SCR_W, hdrH, COL_CARD);
        _tft.drawFastHLine(0, hdrH - 1, SCR_W, COL_DIM);

        // Satir kart arkaplanlari + ± butonlari + etiket (statik)
        for (int i = 0; i < rowCount; i++) {
            int y = rowsY + i * (rowH + rowGap);
            _tft.fillRoundRect(padX, y, SCR_W - padX * 2, rowH, 6, COL_CARD);

            int btnY = y + rowH - btnH - 4;

            // [-] butonu (sol) - once ciz
            int minusX = padX + 8;
            _tft.fillRoundRect(minusX, btnY, btnW, btnH, 5, COL_RED);
            _tft.setTextFont(2);
            _tft.setTextDatum(MC_DATUM);
            _tft.setTextColor(COL_TEXT, COL_RED);
            _tft.drawString("-", minusX + btnW / 2, btnY + btnH / 2);

            // [+] butonu (sag) - once ciz
            int plusX = SCR_W - padX - 8 - btnW;
            _tft.fillRoundRect(plusX, btnY, btnW, btnH, 5, COL_GREEN);
            _tft.setTextColor(COL_TEXT, COL_GREEN);
            _tft.drawString("+", plusX + btnW / 2, btnY + btnH / 2);

            // Etiket: butonlardan SONRA, iki buton arasina ortali (butonlar kapatmasin)
            _tft.setTextFont(1);
            _tft.setTextSize(1);
            _tft.setTextDatum(TC_DATUM);
            _tft.setTextColor(COL_DIM, COL_CARD);
            _tft.drawString(labels[i], SCR_W / 2, y + 4);
        }

        // Alt: Fabrika + Kapat (statik butonlar)
        int botY = rowsY + rowCount * (rowH + rowGap) + 4;
        int botBtnH = 32;
        int botBtnW = (SCR_W - padX * 3) / 2;

        uint16_t resetColor = isOverride ? COL_RED : 0x2104;
        _tft.fillRoundRect(padX, botY, botBtnW, botBtnH, 6, resetColor);
        _tft.setTextFont(2);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(COL_TEXT, resetColor);
        _tft.drawString("Fabrika", padX + botBtnW / 2, botY + botBtnH / 2);

        int closeX = SCR_W - padX - botBtnW;
        _tft.fillRoundRect(closeX, botY, botBtnW, botBtnH, 6, COL_BLUE);
        _tft.setTextColor(COL_TEXT, COL_BLUE);
        _tft.drawString("Kapat", closeX + botBtnW / 2, botY + botBtnH / 2);
    }

    // ---- BASLIK YAZISI (her cycle, override durumu degisebilir) ----
    _tft.setTextFont(2);
    _tft.setTextSize(1);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_ORANGE, COL_CARD);
    _tft.setTextPadding(SCR_W);
    char hdr[40];
    snprintf(hdr, sizeof(hdr), "DUZENLE: %s%s",
             data.profileName ? data.profileName : "?",
             isOverride ? " *" : "");
    _tft.drawString(hdr, SCR_W / 2, hdrH / 2 - 4);
    _tft.setTextFont(1);
    _tft.setTextColor(COL_DIM, COL_CARD);
    char sub[40];
    snprintf(sub, sizeof(sub), "Faz: %s",
             data.phaseName ? data.phaseName : "-");
    _tft.drawString(sub, SCR_W / 2, hdrH - 6);
    _tft.setTextPadding(0);

    // ---- DEGER YAZILARI (her cycle — text padding ile arkaplan temizlenir) ----
    char vals[4][12];
    snprintf(vals[0], sizeof(vals[0]), "%.1f", data.targetTemp);
    snprintf(vals[1], sizeof(vals[1]), "%d",    (int)data.targetHumLow);
    snprintf(vals[2], sizeof(vals[2]), "%d",    (int)data.targetHumHigh);
    // Cev araligi: 0 ise "KAPALI"; aksi halde dk goster
    if (data.turningIntervalMin == 0) {
        snprintf(vals[3], sizeof(vals[3]), "KAPALI");
    } else {
        snprintf(vals[3], sizeof(vals[3]), "%d", (int)data.turningIntervalMin);
    }

    for (int i = 0; i < rowCount; i++) {
        int y = rowsY + i * (rowH + rowGap);
        int btnY = y + rowH - btnH - 4;

        _tft.setTextFont(2);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(valColors[i], COL_CARD);
        _tft.setTextPadding(80);
        // Degeri alt yariya kaydir - etiket ile cakismasin
        _tft.drawString(vals[i], SCR_W / 2, btnY + btnH / 2 + 6);
    }
    _tft.setTextPadding(0);
    _tft.setTextDatum(TL_DATUM);
}

// ==================== ALARM MODAL (TUM TABLARIN USTUNDE) ====================
// Alarm aktif ve susturulmamissa otomatik tam ekran goster.
// Tum diger tablari kapatir; sadece SUSTUR butonu tiklamayi alir.
void DisplayManager::drawAlarmModal(const DisplayData &data) {
    // CANDLING (dol kontrolu) icin OZEL gorunum: mavi tonlu, tarih ve kontrol bilgileri
    bool isCandling = (data.alarmType == ALARM_CANDLING_DUE);

    // Renkler ve baslik (alarm tipine gore)
    uint16_t bgColor   = isCandling ? 0x0010 : 0x2000;     // koyu lacivert / koyu kirmizi
    uint16_t hdrColor  = isCandling ? 0x1D7F : COL_RED;    // mavi / kirmizi banner
    const char* hdrTxt = isCandling ? "* DOL KONTROLU *" : "! ALARM !";

    // Statik elementler (sadece ilk acilista veya force yenilemede)
    if (_bgDraw || !_alarmModalDrawn) {
        // Tam ekran tonlu arkaplan
        _tft.fillRect(0, 0, SCR_W, SCR_H, bgColor);

        // Ust banner (uyari basligi)
        _tft.fillRect(0, 0, SCR_W, 50, hdrColor);
        _tft.setTextFont(4);
        _tft.setTextSize(1);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(COL_TEXT, hdrColor);
        _tft.drawString(hdrTxt, SCR_W / 2, 25);

        // Butonlar — Standart: 3 (SUSTUR/ERTELE/KAPAT), CANDLING: 2 (ERTELE/KAPAT)
        const int btnX = 20;
        const int btnW = SCR_W - 40;
        const int btnH = 38;
        const int btnGap = 6;
        const int totalH = (isCandling ? 2 : 3) * btnH + (isCandling ? 1 : 2) * btnGap;
        const int firstY = SCR_H - totalH - 14;

        // SUSTUR butonu (sadece standart alarmlarda - yesil)
        int snoozeY, dismissY;
        if (!isCandling) {
            int ackY = firstY;
            _tft.fillRoundRect(btnX, ackY, btnW, btnH, 8, COL_GREEN);
            _tft.drawRoundRect(btnX, ackY, btnW, btnH, 8, COL_TEXT);
            _tft.setTextFont(2);
            _tft.setTextSize(1);
            _tft.setTextDatum(MC_DATUM);
            _tft.setTextColor(COL_TEXT, COL_GREEN);
            _tft.drawString("SUSTUR (10dk)", SCR_W / 2, ackY + btnH / 2);
            snoozeY  = firstY + btnH + btnGap;
            dismissY = firstY + 2 * (btnH + btnGap);
        } else {
            snoozeY  = firstY;
            dismissY = firstY + btnH + btnGap;
        }

        // ERTELE butonu (turuncu)
        _tft.fillRoundRect(btnX, snoozeY, btnW, btnH, 8, COL_ORANGE);
        _tft.drawRoundRect(btnX, snoozeY, btnW, btnH, 8, COL_TEXT);
        _tft.setTextFont(2);
        _tft.setTextSize(1);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(COL_TEXT, COL_ORANGE);
        _tft.drawString("ERTELE (1 saat)", SCR_W / 2, snoozeY + btnH / 2);

        // KAPAT butonu (gri)
        _tft.fillRoundRect(btnX, dismissY, btnW, btnH, 8, 0x4208);
        _tft.drawRoundRect(btnX, dismissY, btnW, btnH, 8, COL_TEXT);
        _tft.setTextFont(2);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(COL_TEXT, 0x4208);
        if (isCandling) {
            _tft.drawString("KAPAT (Bugun gosterme)", SCR_W / 2, dismissY + btnH / 2);
        } else {
            _tft.drawString("KAPAT (Temizle)", SCR_W / 2, dismissY + btnH / 2);
        }

        _alarmModalDrawn = true;
    }

    // ============== CANDLING ALERT (OZEL GORUNUM) ==============
    if (isCandling) {
        // Mesaj alani (basligin altinda)
        _tft.fillRect(8, 60, SCR_W - 16, 28, bgColor);
        _tft.setTextFont(4);
        _tft.setTextSize(1);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextColor(COL_YELLOW, bgColor);
        _tft.drawString("Bugun Kontrol Gunu!", SCR_W / 2, 75);

        // Kontrol gunleri kartı (ortada) — CANDLING modal'da 2 buton var, alani azaltildi
        _tft.fillRect(8, 95, SCR_W - 16, SCR_H - 230, bgColor);
        _tft.setTextFont(2);
        _tft.setTextSize(1);
        _tft.setTextColor(COL_TEXT, bgColor);
        _tft.setTextDatum(TL_DATUM);

        // Profil bilgisinden kontrol gunlerini al
        const AnimalProfile* prof = data.profile;
        if (prof) {
            CandlingSchedule cs;
            buildCandlingSchedule(prof, cs);

            bool startDateValid = (data.startYear >= 2020 && data.startYear <= 2099 &&
                                   data.startMonth >= 1 && data.startMonth <= 12 &&
                                   data.startDay >= 1 && data.startDay <= 31);
            DateTime startDt(data.startYear, data.startMonth, data.startDay);

            int cy = 100;
            char lineBuf[64];
            for (int k = 0; k < cs.checkCount; k++) {
                int controlDay = cs.checkDays[k];
                bool isToday = (data.currentDay == controlDay);
                bool isPast  = (data.currentDay > controlDay);
                int daysRemaining = controlDay - data.currentDay;

                // Renklendirme — bugun ise sari, gecmis ise soluk, gelecek ise beyaz
                if (isToday) {
                    _tft.setTextColor(COL_YELLOW, bgColor);
                } else if (isPast) {
                    _tft.setTextColor(0x4208, bgColor);
                } else {
                    _tft.setTextColor(COL_TEXT, bgColor);
                }

                // Tek satirda: "1.Kontrol: 11.05.2026 - BUGUN" / "5 gun kaldi"
                const char* prefix = (k < 3) ? "Kontrol" : "Lockdown";
                if (startDateValid) {
                    DateTime checkDt = startDt + TimeSpan(controlDay - 1, 0, 0, 0);
                    if (k < 3) {
                        snprintf(lineBuf, sizeof(lineBuf), "%d.%s: %02d.%02d.%04d",
                                 k + 1, prefix,
                                 checkDt.day(), checkDt.month(), checkDt.year());
                    } else {
                        snprintf(lineBuf, sizeof(lineBuf), "%s: %02d.%02d.%04d", prefix,
                                 checkDt.day(), checkDt.month(), checkDt.year());
                    }
                } else {
                    if (k < 3) {
                        snprintf(lineBuf, sizeof(lineBuf), "%d.%s: Gun %d",
                                 k + 1, prefix, controlDay);
                    } else {
                        snprintf(lineBuf, sizeof(lineBuf), "%s: Gun %d", prefix, controlDay);
                    }
                }
                _tft.drawString(lineBuf, 14, cy);

                // Sag tarafa: kalan gun
                char dayBuf[20];
                if (daysRemaining == 0) {
                    snprintf(dayBuf, sizeof(dayBuf), "BUGUN!");
                } else if (daysRemaining > 0) {
                    snprintf(dayBuf, sizeof(dayBuf), "%d gun", daysRemaining);
                } else {
                    snprintf(dayBuf, sizeof(dayBuf), "%d gun", daysRemaining);
                }
                _tft.setTextDatum(TR_DATUM);
                _tft.drawString(dayBuf, SCR_W - 14, cy);
                _tft.setTextDatum(TL_DATUM);

                cy += 18;
            }
        }
        return;
    }

    // ============== STANDART ALARM (Sicaklik/Nem/CO2 vb.) ==============
    // Dinamik kisim: alarm mesaji + sensor degerleri
    _tft.fillRect(8, 60, SCR_W - 16, 70, bgColor);
    _tft.setTextFont(2);
    _tft.setTextSize(1);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_YELLOW, bgColor);

    String msg = data.alarmMsg;
    if (msg.length() > 24) {
        int splitAt = -1;
        for (int i = 12; i < (int)msg.length() && i < 28; i++) {
            if (msg.charAt(i) == ' ') { splitAt = i; break; }
        }
        if (splitAt > 0) {
            String line1 = msg.substring(0, splitAt);
            String line2 = msg.substring(splitAt + 1);
            if (line2.length() > 28) line2 = line2.substring(0, 25) + "...";
            _tft.drawString(line1, SCR_W / 2, 78);
            _tft.drawString(line2, SCR_W / 2, 100);
        } else {
            if (msg.length() > 28) msg = msg.substring(0, 25) + "...";
            _tft.drawString(msg, SCR_W / 2, 90);
        }
    } else {
        _tft.drawString(msg, SCR_W / 2, 90);
    }

    // Sensor durumu: mevcut ve hedef degerler — kompakt (3 buton icin alani azaltildi)
    _tft.fillRect(8, 130, SCR_W - 16, 45, bgColor);
    _tft.setTextFont(2);
    _tft.setTextDatum(MC_DATUM);

    char buf[40];
    // Sicaklik (tek satir: "37.5 / 37.5 C")
    _tft.setTextColor(COL_DIM, bgColor);
    _tft.drawString("Sicaklik", SCR_W / 4, 138);
    snprintf(buf, sizeof(buf), "%.1f/%.1f C", data.temperature, data.targetTemp);
    _tft.setTextColor(COL_TEXT, bgColor);
    _tft.drawString(buf, SCR_W / 4, 160);

    // Nem (tek satir: "%55 / %50-%65")
    _tft.setTextColor(COL_DIM, bgColor);
    _tft.drawString("Nem", 3 * SCR_W / 4, 138);
    snprintf(buf, sizeof(buf), "%%%.0f/%%%.0f-%%%.0f",
             data.humidity, data.targetHumLow, data.targetHumHigh);
    _tft.setTextColor(COL_TEXT, bgColor);
    _tft.drawString(buf, 3 * SCR_W / 4, 160);

    _tft.setTextDatum(TL_DATUM);
}

// ==================== SAYFA: AYAR ====================

void DisplayManager::drawSettings(const DisplayData &data) {
    const int cardMargin = 4;
    const int cardPadX = 8;
    const int cardW = SCR_W - cardMargin * 2;
    const int rowH = 20;
    int cy = 2;

    // ========== KART 0: TARIH / SAAT (Compact, kullanici dostu) ==========
    {
        const int card0H = 28;

        // Tarih gecerliligi kontrolu: yil 2024-2099 disi -> RTC bozuk veya hic set edilmemis
        bool dateValid = (data.todayYear >= 2024 && data.todayYear <= 2099);

        // Kart arka plan + sol kenar vurgu (gecersiz tarih -> kirmizi)
        uint16_t accentColor = dateValid ? COL_GREEN : COL_RED;
        if (_bgDraw) {
            _tft.fillRoundRect(cardMargin, cy, cardW, card0H, 6, COL_CARD);
            _tft.fillRoundRect(cardMargin, cy + 2, 4, card0H - 4, 2, accentColor);
            _tft.drawRoundRect(cardMargin, cy, cardW, card0H, 6, 0x3186);
        }

        if (!dateValid) {
            // Gecersiz tarih: kullaniciya web'den ayarlama bildirimi
            _tft.setTextFont(2);
            _tft.setTextSize(1);
            _tft.setTextDatum(MC_DATUM);
            _tft.setTextColor(COL_RED, COL_CARD);
            _tft.setTextPadding(SCR_W - 30);
            // Yaniip sonen efekt (dikkat cek)
            bool blink = ((millis() / 600) % 2) == 0;
            if (blink) {
                _tft.drawString("! Saat AYARLI DEGIL (Web'den ayarla) !",
                                SCR_W / 2, cy + card0H / 2);
            } else {
                _tft.drawString("Tarih/Saat geçersiz — Web'den ayarlayin",
                                SCR_W / 2, cy + card0H / 2);
            }
            _tft.setTextPadding(0);
            _tft.setTextDatum(TL_DATUM);
        } else {
            // Turkce gun adlari (RTClib dayOfTheWeek: 0=Pazar..6=Cumartesi)
            static const char* DAY_TR[] = {
                "Pazar", "Pazartesi", "Sali", "Carsamba",
                "Persembe", "Cuma", "Cumartesi"
            };
            const char* dayName = (data.todayDow < 7) ? DAY_TR[data.todayDow] : "";

            // Sol: tarih + gun adi
            char leftBuf[40];
            snprintf(leftBuf, sizeof(leftBuf), "%02u.%02u.%04u  %s",
                     (unsigned)data.todayDay, (unsigned)data.todayMonth,
                     (unsigned)data.todayYear, dayName);
            _tft.setTextFont(2);
            _tft.setTextSize(1);
            _tft.setTextDatum(ML_DATUM);
            _tft.setTextColor(COL_TEXT, COL_CARD);
            _tft.setTextPadding(180);
            _tft.drawString(leftBuf, cardPadX + cardMargin + 4, cy + card0H / 2);
            _tft.setTextPadding(0);

            // Sag: saat (parlak yesil, vurgu)
            char timeBuf[8];
            snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u",
                     (unsigned)data.todayHour, (unsigned)data.todayMinute);
            _tft.setTextDatum(MR_DATUM);
            _tft.setTextColor(COL_GREEN, COL_CARD);
            _tft.setTextPadding(48);
            _tft.drawString(timeBuf, SCR_W - cardMargin - cardPadX - 2, cy + card0H / 2);
            _tft.setTextPadding(0);
            _tft.setTextDatum(TL_DATUM);
        }

        cy += card0H + 4;
    }

    // ========== KART 1: AG BILGISI ==========
    const int card1H = 58;
    if (_bgDraw) {
        _tft.fillRoundRect(cardMargin, cy, cardW, card1H, 6, COL_CARD);
        _tft.drawRoundRect(cardMargin, cy, cardW, card1H, 6, 0x3186); // ince cerceve
    }
    
    // Baslik
    _tft.setTextFont(1);
    _tft.setTextSize(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_CYAN, COL_CARD);
    _tft.drawString("@ AG BILGISI", cardPadX + cardMargin, cy + 6);

    _tft.setTextFont(2);
    int ly = cy + 22;
    
    if (data.apActive) {
        _tft.setTextColor(COL_ORANGE, COL_CARD);
        _tft.drawString("AP:", cardPadX + cardMargin, ly);
        _tft.setTextColor(COL_TEXT, COL_CARD);
        char ap[40];
        snprintf(ap, sizeof(ap), "%s [%d]", data.apIP.c_str(), data.apClients);
        _tft.setTextPadding(150);
        _tft.drawString(ap, cardPadX + cardMargin + 30, ly);
        _tft.setTextPadding(0);
        ly += rowH;
        
        _tft.setTextFont(2);
        _tft.setTextColor(COL_CYAN, COL_CARD);
        _tft.drawString("secgem.com", cardPadX + cardMargin, ly);
    } else if (data.staConnected) {
        _tft.setTextColor(COL_GREEN, COL_CARD);
        _tft.drawString("WiFi:", cardPadX + cardMargin, ly);
        _tft.setTextColor(COL_TEXT, COL_CARD);
        _tft.setTextPadding(150);
        _tft.drawString(data.staIP, cardPadX + cardMargin + 50, ly);
        _tft.setTextPadding(0);
    } else {
        _tft.setTextColor(COL_RED, COL_CARD);
        _tft.drawString("Bagli degil", cardPadX + cardMargin, ly);
    }

    // ========== KART 2: SENSOR DURUMU (Kompakt tek satir) ==========
    cy += card1H + 4;
    const int card2H = 28;
    if (_bgDraw) {
        _tft.fillRoundRect(cardMargin, cy, cardW, card2H, 6, COL_CARD);
        _tft.fillRoundRect(cardMargin, cy + 2, 4, card2H - 4, 2, COL_CYAN);
        _tft.drawRoundRect(cardMargin, cy, cardW, card2H, 6, 0x3186);
    }

    _tft.setTextFont(1);
    _tft.setTextDatum(ML_DATUM);
    _tft.setTextColor(COL_CYAN, COL_CARD);
    _tft.drawString("SENSOR", cardPadX + cardMargin + 4, cy + card2H / 2);

    _tft.setTextFont(2);

    // Sensor 1 (orta)
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString(SENSOR_NAME, cardPadX + cardMargin + 56, cy + card2H / 2);
    // Takili olmayan sensor "-" ile gosterilir (kirmizi X degil): ariza degil
    _tft.setTextColor(data.sensor1OK ? COL_GREEN
                                     : (data.sensor1Present ? COL_RED : COL_DIM), COL_CARD);
    _tft.drawString(data.sensor1OK ? "OK"
                                   : (data.sensor1Present ? "X" : "-"),
                    cardPadX + cardMargin + 106, cy + card2H / 2);

    // Sensor 2 (sag)
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString(SENSOR_BUS_NAME, cardPadX + cardMargin + 140, cy + card2H / 2);
    _tft.setTextColor(data.sensor2OK ? COL_GREEN
                                     : (data.sensor2Present ? COL_RED : COL_DIM), COL_CARD);
    _tft.drawString(data.sensor2OK ? "OK"
                                   : (data.sensor2Present ? "X" : "-"),
                    cardPadX + cardMargin + 188, cy + card2H / 2);
    _tft.setTextDatum(TL_DATUM);

    // ========== KART 3: SISTEM ==========
    cy += card2H + 4;
    const int card3H = 58;
    if (_bgDraw) {
        _tft.fillRoundRect(cardMargin, cy, cardW, card3H, 6, COL_CARD);
        _tft.drawRoundRect(cardMargin, cy, cardW, card3H, 6, 0x3186);
    }
    
    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_CYAN, COL_CARD);
    _tft.drawString("* SISTEM", cardPadX + cardMargin, cy + 6);

    _tft.setTextFont(2);
    ly = cy + 22;
    
    // Uptime (saat:dakika:saniye)
    unsigned long s = data.uptimeSec;
    int upH = s / 3600;
    int upM = (s % 3600) / 60;
    int upS = s % 60;
    char ut[24];
    snprintf(ut, sizeof(ut), "%02d:%02d:%02d", upH, upM, upS);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("Uptime", cardPadX + cardMargin, ly);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.drawString(ut, cardPadX + cardMargin + 55, ly);
    
    // RAM
    char hp[16];
    snprintf(hp, sizeof(hp), "%luKB", data.freeHeap / 1024);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("RAM", cardPadX + cardMargin + 130, ly);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.drawString(hp, cardPadX + cardMargin + 165, ly);
    
    // Durum
    ly += rowH;
    int si = constrain(data.systemState, 0, 6);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("Durum:", cardPadX + cardMargin, ly);
    _tft.setTextColor(ST_CLR[si], COL_CARD);
    _tft.setTextPadding(80);
    _tft.drawString(ST_SHORT[si], cardPadX + cardMargin + 55, ly);
    _tft.setTextPadding(0);

    // ========== KART 4: PROFIL & YUMURTA (Kompakt) ==========
    cy += card3H + 4;
    const int card4H = 46;
    if (_bgDraw) {
        _tft.fillRoundRect(cardMargin, cy, cardW, card4H, 6, COL_CARD);
        _tft.fillRoundRect(cardMargin, cy + 2, 4, card4H - 4, 2, COL_ORANGE);
        _tft.drawRoundRect(cardMargin, cy, cardW, card4H, 6, 0x3186);
    }

    // Satir 1: Profil adi + Gun + Cevirme (sag uctan)
    _tft.setTextFont(2);
    _tft.setTextSize(1);
    _tft.setTextDatum(TL_DATUM);

    // Profil adi (sol)
    char pr[32];
    snprintf(pr, sizeof(pr), "%s", data.profileName ? data.profileName : "---");
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(110);
    _tft.drawString(pr, cardPadX + cardMargin + 6, cy + 4);
    _tft.setTextPadding(0);

    // Gun bilgisi (sag)
    char dayStr[16];
    snprintf(dayStr, sizeof(dayStr), "Gun %d/%d", data.currentDay, data.totalDays);
    _tft.setTextColor(COL_ORANGE, COL_CARD);
    _tft.setTextDatum(TR_DATUM);
    _tft.setTextPadding(80);
    _tft.drawString(dayStr, SCR_W - cardMargin - cardPadX - 4, cy + 4);
    _tft.setTextPadding(0);
    _tft.setTextDatum(TL_DATUM);

    // Satir 2: Yumurta butonlari (sol-orta-sag) + Cevirme (en sag)
    const int btnW = 28;
    const int btnH = 20;
    const int btnY = cy + 22;
    const int leftBtnX  = cardPadX + cardMargin + 6;
    const int rightBtnX = leftBtnX + btnW + 42;   // - [50] +

    if (_bgDraw) {
        drawButton(leftBtnX,  btnY, btnW, btnH, "-", COL_ORANGE, COL_TEXT);
        drawButton(rightBtnX, btnY, btnW, btnH, "+", COL_GREEN, COL_TEXT);
    }

    // Yumurta sayisi (butonlar arasinda)
    _tft.setTextFont(2);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_TEXT, COL_CARD);
    _tft.setTextPadding(36);
    char ec[10];
    snprintf(ec, sizeof(ec), "%u", (unsigned)data.eggCount);
    _tft.drawString(ec, leftBtnX + btnW + 21, btnY + btnH / 2);
    _tft.setTextPadding(0);

    // Cevirme durumu (en sag)
    _tft.setTextFont(1);
    _tft.setTextDatum(MR_DATUM);
    _tft.setTextColor(COL_DIM, COL_CARD);
    _tft.drawString("Cev:", SCR_W - cardMargin - cardPadX - 50, btnY + btnH / 2);
    _tft.setTextColor(data.turningEnabled ? COL_GREEN : COL_ORANGE, COL_CARD);
    _tft.drawString(data.turningEnabled ? "ACIK" : "KAPALI",
                    SCR_W - cardMargin - cardPadX - 4, btnY + btnH / 2);
    _tft.setTextDatum(TL_DATUM);

    // ========== KART 5: IR YUMURTA SENSORU ==========
    cy += card4H + 4;  // 250
    const int card5H = 36;
    if (_bgDraw) {
        _tft.fillRoundRect(cardMargin, cy, cardW, card5H, 6, COL_CARD);
        _tft.drawRoundRect(cardMargin, cy, cardW, card5H, 6, 0x3186);
    }

    // Baslik (sol-ust)
    _tft.setTextFont(1);
    _tft.setTextDatum(TL_DATUM);
    _tft.setTextColor(COL_CYAN, COL_CARD);
    _tft.drawString("& IR YUMURTA", cardPadX + cardMargin, cy + 6);

    // BUL butonu (sol-alt) + durum etiketi
    // Discovery status: 0=NONE 1=RUNNING 2=OK 3=FAILED
    const int bulBtnX = cardPadX + cardMargin;            // 12
    const int bulBtnY = cy + 18;                          // 268
    const int bulBtnW = 58;
    const int bulBtnH = 14;
    uint16_t  bulBg   = COL_CYAN;
    const char* bulLbl = "BUL";
    switch (data.eggDiscoveryStatus) {
        case 1: bulBg = COL_ORANGE; bulLbl = "..."; break;   // running
        case 2: bulBg = COL_GREEN;  bulLbl = "OK";  break;   // ok
        case 3: bulBg = COL_RED;    bulLbl = "X";   break;   // failed
        default:bulBg = 0x3186;     bulLbl = "BUL"; break;   // none (koyu gri)
    }
    _tft.fillRoundRect(bulBtnX, bulBtnY, bulBtnW, bulBtnH, 3, bulBg);
    _tft.drawRoundRect(bulBtnX, bulBtnY, bulBtnW, bulBtnH, 3, COL_DIM);
    _tft.setTextFont(1);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(COL_TEXT, bulBg);
    _tft.drawString(bulLbl, bulBtnX + bulBtnW / 2, bulBtnY + bulBtnH / 2 + 1);
    _tft.setTextDatum(TL_DATUM);

    // Aktif/Pasif kucuk yazi (BUL butonu yaninda)
    _tft.setTextFont(1);
    _tft.setTextColor(data.eggSensorEnabled ? COL_GREEN : COL_DIM, COL_CARD);
    _tft.setTextPadding(60);
    _tft.drawString(data.eggSensorEnabled ? "AKTIF" : "PASIF",
                    bulBtnX + bulBtnW + 6, bulBtnY + 4);
    _tft.setTextPadding(0);

    // Toggle butonu (sag)
    const int tglW = 72;
    const int tglH = 24;
    const int tglX = SCR_W - cardMargin - cardPadX - tglW;
    const int tglY = cy + 6;
    uint16_t tglBg = data.eggSensorEnabled ? COL_GREEN : 0x3186;  // koyu gri pasif
    _tft.fillRoundRect(tglX, tglY, tglW, tglH, tglH / 2, tglBg);
    _tft.drawRoundRect(tglX, tglY, tglW, tglH, tglH / 2, COL_DIM);

    // Slider topu (yuvarlak, ON saga PASIF sola)
    const int knobR = 9;
    int knobX = data.eggSensorEnabled
                ? (tglX + tglW - knobR - 3)
                : (tglX + knobR + 3);
    int knobY = tglY + tglH / 2;
    _tft.fillCircle(knobX, knobY, knobR, COL_TEXT);

    // Buton uzerindeki metin (ON / OFF)
    _tft.setTextFont(1);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextColor(data.eggSensorEnabled ? COL_TEXT : COL_DIM, tglBg);
    int lblX = data.eggSensorEnabled
               ? (tglX + (tglW - knobR * 2 - 6) / 2 + 2)
               : (tglX + knobR * 2 + 6 + (tglW - knobR * 2 - 6) / 2);
    _tft.drawString(data.eggSensorEnabled ? "ACIK" : "KAPALI",
                    lblX, knobY + 1);
    _tft.setTextDatum(TL_DATUM);
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

// ==================== SPLASH ANIMASYONU ====================

void DisplayManager::showSplashAnimation() {
    wdtFeed();
    // Geciici olarak yatay (landscape) moda gec — splash sonunda portrait'a doneriz.
    _tft.setRotation(1);
    const int W = 320;
    const int H = 240;
    const int centerY = H / 2 - 10;

    _tft.fillScreen(COL_BG);

    // Ana yazi: SECGEM (buyuk, yatay)
    _tft.setTextFont(4);
    _tft.setTextSize(2);
    _tft.setTextDatum(MC_DATUM);

    // Harf harf belirme
    const char* text = "Secgem";
    int charW = 28;
    int startX = (W - 6 * charW) / 2 + charW / 2;

    for (int i = 0; i < 6; i++) {
        char c[2] = {text[i], '\0'};
        _tft.setTextColor(COL_CYAN, COL_BG);
        _tft.drawString(c, startX + i * charW, centerY);
        delay(80);
        wdtFeed();
    }

    delay(200);
    wdtFeed();

    // Parlama efekti
    _tft.setTextColor(COL_TEXT, COL_BG);
    _tft.drawString("Secgem", W / 2, centerY);
    delay(150);
    _tft.setTextColor(COL_CYAN, COL_BG);
    _tft.drawString("Secgem", W / 2, centerY);

    // Alt yazi
    _tft.setTextFont(2);
    _tft.setTextSize(1);
    _tft.setTextColor(COL_DIM, COL_BG);
    {
        char verBuf[40];
        snprintf(verBuf, sizeof(verBuf), "Akilli Kulucka v%s", FW_VERSION_STRING);
        _tft.drawString(verBuf, W / 2, centerY + 45);
    }

    wdtFeed();
    // Landscape splash GORUNUR KALIR. Altinda animasyonlu "Yukleniyor..."
    // arka plan task'ta donmeye baslar. Init bitince update() ekrani
    // temizleyip portrait dashboard'a gecer.
    startLoadingAnimation();
}

// ==================== LOADING ANIMASYONU ====================
// FreeRTOS task: noktalar her 350ms'de degisir.
// Ana thread init kodu blokladiginda bile ekran canli kalir.
void DisplayManager::loadingTask(void* pvParameters) {
    DisplayManager* dm = static_cast<DisplayManager*>(pvParameters);
    const char* frames[] = {
        "Yukleniyor",
        "Yukleniyor.",
        "Yukleniyor..",
        "Yukleniyor..."
    };
    uint8_t idx = 0;
    // Landscape splash ekraninda (320x240). "Akilli Kulucka v3" alt
    // yazisinin altina yerlestir (centerY+45 zaten alt yazi, +75 daha asagi).
    const int W = 320;
    const int H = 240;
    const int loadingY = H / 2 - 10 + 80;   // splash centerY + 80
    while (dm->_loadingActive) {
        dm->_tft.setTextFont(2);
        dm->_tft.setTextSize(1);
        dm->_tft.setTextDatum(MC_DATUM);
        dm->_tft.setTextColor(COL_CYAN, COL_BG);
        dm->_tft.setTextPadding(180);
        dm->_tft.drawString(frames[idx], W / 2, loadingY);
        dm->_tft.setTextPadding(0);
        dm->_tft.setTextDatum(TL_DATUM);
        idx = (idx + 1) % 4;
        vTaskDelay(pdMS_TO_TICKS(350));
    }
    dm->_loadingTaskHandle = nullptr;
    vTaskDelete(NULL);
}

void DisplayManager::startLoadingAnimation() {
    if (_loadingActive) return;
    _loadingActive = true;
    xTaskCreate(loadingTask, "Loading", 3072, this, 1, &_loadingTaskHandle);
}

void DisplayManager::stopLoadingAnimation() {
    if (!_loadingActive) return;
    _loadingActive = false;
    // Task'in temiz cikis yapmasi icin kisa bekleme
    for (int i = 0; i < 10 && _loadingTaskHandle != nullptr; ++i) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
