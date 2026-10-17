# MacroUSB

STM32 tabanlı, bilgisayara takıldığında **USB klavye + fare** ve aynı anda bir
**USB ağ adaptörü** olarak görünen bir cihaz. Cihazın içinde küçük bir web
sunucusu çalışır; tarayıcıdan cihazın IP adresine girince bir **ayar arayüzü**
açılır. Orada **birden çok makro** tanımlar, birini seçip **Başlat** dersin ve
cihaz o makroyu klavye/fare girdisi olarak uygular.

Her makro bir **betik**tir ve şunları yapabilir:
- bir **uygulama veya web sitesi açmak** (ör. Not Defteri, bir URL),
- **metin yazmak**, **özel tuşlara** basmak (ok tuşları, Enter, Tab, F-tuşları…),
- **tuş kombinasyonları** (Ctrl+C, Alt+F4…),
- **fareyi** oynatmak / tıklamak,
- adımlar arasında **beklemek**.

> **Amaç / kullanım:** Kişisel bir **otomasyon / makro** aracıdır — kendi
> bilgisayarında, senin elle başlattığın, durumu LED ile belli olan şeffaf bir
> cihaz. Açılışta otomatik başlamaz; yalnızca ayar sayfasındaki **Başlat** ile
> çalışır.

---

## İçindekiler
- [Mimari](#mimari)
- [Donanım](#donanım)
- [Proje yapısı](#proje-yapısı)
- [Derleme](#derleme)
- [Flashleme](#flashleme)
- [Ağ / bağlanma (IP ve URL)](#ağ--bağlanma-ip-ve-url)
- [Makro dili](#makro-dili)
- [Host profili ve "başlat" (Windows/Linux)](#host-profili-ve-başlat-windowslinux)
- [Kullanım](#kullanım)
- [Windows / Linux sürücü + güvenlik duvarı](#windows--linux-sürücü--güvenlik-duvarı)
- [Doğrulama](#doğrulama)
- [Bilinen kısıtlar](#bilinen-kısıtlar)
- [Durum / yol haritası](#durum--yol-haritası)
- [Üçüncü parti ve lisans](#üçüncü-parti-ve-lisans)

---

## Mimari

Tek firmware, ayrı masaüstü uygulaması yok. USB'de **composite** cihaz:

| Arabirim | Sınıf | İş |
|----------|-------|----|
| 0 + 1 | USB ağ (varsayılan **RNDIS**, derlemeyle **CDC-ECM**) | Cihaz-içi web arayüzüne erişim |
| 2 | **HID klavye + fare** (tek arabirim, report ID ile) | Makroların uyguladığı girdi |

> **Neden tek HID arabirimi?** STM32F411 USB OTG-FS'inde yalnızca **4 endpoint**
> var ve kontrol + ağ (bildirim, veri) + HID ile hepsi dolu. Bu yüzden klavye ve
> fare **ayrı arabirim/endpoint yerine aynı HID arabiriminde "report ID" ile**
> taşınır (klavye = ID 1, fare = ID 2). Standart ve endpoint-tutumlu yöntem.

Firmware içi bileşenler:
- **TinyUSB** — USB aygıt yığını (RNDIS/ECM + HID), STM32 OTG-FS (dwc2) sürücüsü.
- **lwIP** (NO_SYS, raw API) — TCP/IP.
- **DHCP + DNS sunucusu** — host'a IP verir (cihaz `192.168.7.1`, host `.7.2`).
- **HTTP sunucusu** (bu projede yazılmış) — tek sayfalık arayüz + `/api/*`.
- **Makro motoru** (`macro.c`) — betiği adım adım, HID hazır oldukça uygular.

### USB ağ sınıfı: RNDIS (birincil)

| Sınıf | Windows (sürücüsüz) | Linux | macOS |
|-------|---------------------|-------|-------|
| **RNDIS** (varsayılan) | ✅ dahili | ✅ `rndis_host` | ❌ |
| CDC-ECM (`make USE_ECM=1`) | ❌ | ✅ `cdc_ether` | ✅ |

Windows'ta sürücüsüz çalışması önemli olduğundan RNDIS birincil; Linux/macOS için
ECM derleme anahtarı var.

---

## Donanım

STM32F4 "Black Pill" kartları. İki MCU profili desteklenir:

| MCU | Flash / RAM | Makefile | Not |
|-----|-------------|----------|-----|
| **STM32F411CEU6** | 512 KB / 128 KB | `make` (varsayılan) | bol RAM |
| **STM32F401RCT6** | 256 KB / 64 KB | `make MCU=f401rc` | küçük-RAM profili (RAM ~%50) |

Pin haritası iki kartta da ortak:

| İşlev | Pin | Not |
|-------|-----|-----|
| USB D− / D+ | PA11 / PA12 | AF10, VBUS algılama kapalı |
| Durum LED'i | PC13 | Aktif-düşük |
| Kullanıcı butonu | PA0 | Aktif-düşük |
| Saat | HSE kristal | PLLM = HSE/1MHz → 84 MHz SYSCLK, 48 MHz USB |

> **Kristal:** Varsayılan 25 MHz. Kartında 8 MHz kristal varsa:
> `make MCU=f401rc HSE_VALUE=8000000`. (USB için harici kristal şart; iç osilatör
> yeterince hassas değil.)

Kart-bağımlı her şey tek yerde: [`firmware/board/board_blackpill.c`](firmware/board/board_blackpill.c).
MCU'ya göre değişen density/startup/linker Makefile'daki `MCU` profilinden gelir.

---

## Proje yapısı

```
macrousb/
├── firmware/
│   ├── board/board_f411_blackpill.c   # KART-BAĞIMLI (clock/pin/USB/LED)
│   ├── src/
│   │   ├── main.c                     # süper döngü
│   │   ├── app.c/.h                    # makro seti + /api + LED
│   │   ├── macro.c/.h                  # makro motoru (parser + yorumlayıcı)
│   │   ├── macro_set.h                 # makro seti veri yapısı (ad+betik)
│   │   ├── keymap_us.c/keymap.h        # ASCII -> HID (US QWERTY)
│   │   ├── usb_descriptors.c           # RNDIS/ECM + HID(klavye+fare) tanımları
│   │   ├── usb_reports.h               # HID report ID'leri
│   │   ├── net_glue.c/.h               # lwIP netif + DHCP/DNS
│   │   ├── http_server.c/.h            # HTTP + /api dağıtımı
│   │   ├── flash_store.c/.h            # makro setini flash'ta sakla
│   │   ├── lwipopts.h / tusb_config.h / stm32f4xx_hal_conf.h / syscalls.c
│   │   └── arch/cc.h
│   ├── linker/STM32F411CEUx_FLASH.ld
│   ├── third_party/fetch_deps.sh       # bağımlılıkları çeker (git'e dahil değil)
│   └── Makefile
├── web/
│   ├── index.html                      # makro listesi + betik editörü
│   └── tools/embed.py                  # index.html -> flash'a gömülü C dizisi
└── README.md
```

---

## Derleme

```bash
sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi make git python3

cd firmware/third_party && ./fetch_deps.sh    # TinyUSB, lwIP, HAL, CMSIS (sabit sürümler)
cd ..
make                 # STM32F411CEU6 (varsayılan)
make MCU=f401rc      # STM32F401RCT6 (256 KB / 64 KB)
# ek anahtarlar birleşebilir: make MCU=f401rc USE_ECM=1 HSE_VALUE=8000000
```
Çıktı `firmware/build/macrousb.{elf,bin,hex}`. Örnek boyut:
- **F411:** FLASH %15, RAM %56
- **F401RC:** FLASH %29, RAM %50 (küçük-RAM profili; 64 KB'a rahat sığar)

---

## Flashleme

```bash
make flash-st     # ST-Link (st-flash) — veya STM32CubeProgrammer
make flash-dfu    # BOOT0'a basılı tutup tak, sonra dfu-util
```

---

## Ağ / bağlanma (IP ve URL)

| | Adres |
|-|-------|
| Cihaz (web arayüzü) | **192.168.7.1** |
| Host (DHCP ile) | 192.168.7.2 |

Arayüz: **http://192.168.7.1/** (DNS ile `http://macrousb/` de çalışır).

---

## Makro dili

Her makro, satır başına bir komut olan basit bir betiktir. `#` ile başlayan
satırlar yorumdur.

| Komut | Açıklama |
|-------|----------|
| `launch url <adres>` | varsayılan tarayıcıda adresi açar |
| `launch app <komut>` | uygulama/komut çalıştırır |
| `text <yazı>` | düz metin yazar (US QWERTY) |
| `key <AD>` | tek tuş: `ENTER TAB ESC SPACE LEFT RIGHT UP DOWN HOME END PAGEUP PAGEDOWN DELETE BACKSPACE INSERT F1..F12` ya da tek karakter |
| `combo <MOD>+<TUŞ>` | kombinasyon: `CTRL+C`, `CTRL+SHIFT+T`, `ALT+F4` (MOD: `CTRL ALT SHIFT GUI/WIN/SUPER`) |
| `wait <ms>` | milisaniye bekler |
| `mouse move <dx> <dy>` | fareyi **göreli** oynatır (piksel, +/−) |
| `mouse click <left\|right\|middle>` | fare tıklaması |

`launch ...`, seçili **host profiline** göre doğru "çalıştır" dizisine açılır
(aşağıya bak). `launch`'tan sonra uygulamanın açılması için **`wait` koy**.

### Örnekler

Not Defteri açıp yaz (Windows):
```
launch app notepad
wait 1500
text Merhaba MacroUSB
key ENTER
```

Bir web sitesi aç (Windows):
```
launch url https://example.com
```

Metin editöründe imleç + fare oynat:
```
launch app notepad
wait 1500
text deneme metni
key LEFT
key LEFT
key RIGHT
mouse move 60 0
mouse move -60 0
mouse click left
```

---

## Host profili ve "başlat" (Windows/Linux)

Cihaz **ekranı görmez**; bir uygulamayı "başlatmak" için o işletim sisteminin
"çalıştır" kısayolunu klavyeyle basar. Doğru diziyi seçmek için arayüzdeki
**Host** ayarını kullan:

| Host | `launch` dizisi | Not |
|------|-----------------|-----|
| **Windows** | **Win+R** → komut → Enter | URL ve uygulama adları çalışır |
| **Linux — KDE/XFCE** | **Alt+F2** (KRunner) → komut → Enter | Linux'ta en güvenilir; URL ve komut çalışır |
| **Linux — GNOME** | **Süper** tuşu → ad → Enter | Uygulama adıyla çalışır; URL güvenilir değil |

> Linux'ta evrensel bir "çalıştır" kısayolu olmadığından KDE/XFCE (Alt+F2) en
> sağlam seçenektir; GNOME'da uygulama adı çalışır ama URL açmak güvenilir
> değildir. Başlatma çalışmazsa komutu elle yazabilirsin — makronun geri kalanı
> yine çalışır.

---

## Kullanım

1. Cihazı tak, **http://192.168.7.1/** adresini aç.
2. **Makro seç** menüsünden bir makro seç ya da **+ Yeni** ile ekle.
3. **Ad** ve **Makro betiği**ni düzenle; **Değişiklikleri uygula**ya bas.
4. **Host** profilini (Windows / Linux-KDE / Linux-GNOME) seç. İstersen **Döngü**'yü
   açıp **aralık** ver (makro, her aralıkta tekrar çalışır); kapalıysa bir kez çalışır.
5. **Başlat**'a bas. ⚠️ Çıktı **o an odakta olan pencereye** gider; makro bir
   uygulama açmıyorsa, Başlat'tan önce/sonra hedefe geçmek için betiğe `wait` koy.
6. **Durdur** ile her an durdur.
7. **Cihaza kaydet (flash)**: tüm makrolar + ayarlar cihazda saklanır, tak-çıkar
   sonrası hatırlanır. (Flash yazılırken USB ~1-2 sn duraklar; normaldir.)

**Durum LED'i (PC13):** host yok → hızlı yanıp söner · boşta → ~2 sn'de bir çakma
· çalışıyor (bekliyor) → sabit · makro gönderiliyor → titrer.

---

## Windows / Linux sürücü + güvenlik duvarı

- **Windows:** RNDIS ile sürücüsüz tanınır ("USB RNDIS Adapter"). Defender
  Firewall yeni USB ağını/HTTP'yi bloklayabilir; test için bu ağa izin ver ya da
  (hocanın istediği gibi) güvenlik duvarını geçici kapat. Cihaz hem ağ
  bağdaştırıcısı hem **klavye + fare (HID)** olarak görünür.
- **Linux:** RNDIS build'i `rndis_host`, ECM build'i (`make USE_ECM=1`)
  `cdc_ether` ile tanınır. `ip link` / `dmesg` ile kontrol et. Gerekirse:
  `sudo ip addr add 192.168.7.2/24 dev enXXX && sudo ip link set enXXX up`.

---

## Doğrulama

> Bu ortamda yalnızca **derleme** doğrulandı (temiz derlenip linklenir, hem RNDIS
> hem ECM). Aşağıdaki davranış kontrolleri gerçek donanımda senin yapacağın adımlar.

1. `make` → flash → kartı tak.
2. **Enumerate:** Windows Aygıt Yöneticisi'nde ağ bağdaştırıcısı **ve** HID
   (klavye+fare) görünmeli; Linux'ta `ip link` yeni arayüz + `dmesg` `rndis_host`.
3. `http://192.168.7.1/` açılmalı; makro listesi gelmeli.
4. Bir metin editörü aç. Basit bir makro yaz (`text ...`, `key LEFT` vb.),
   **Başlat**, editöre tıkla → tuşların uygulandığını gör.
5. Fare testi: `mouse move 60 0` / `mouse click left` → imlecin oynadığını gör.
6. `launch` testi: host profilini seç, `launch app notepad` (veya Linux'ta uygun
   komut) + `wait` + `text ...` → uygulamanın açılıp makronun çalıştığını gör.
7. **Kaydet** → çıkar-tak → makroların hatırlandığını gör.

---

## Bilinen kısıtlar

- **Cihaz "kör":** Ekranı göremez; uygulamanın açıldığını/odağı algılayamaz.
  "Aç → makro" **sabit `wait` gecikmeleriyle** çalışır (açık döngü). Uygulama geç
  açılırsa gecikmeyi artır.
- **Linux başlatma masaüstüne bağlı:** KDE/XFCE (Alt+F2) sağlam; GNOME'da URL
  açmak güvenilir değil (host profiline bak).
- **Klavye düzeni US QWERTY** varsayılır (`src/keymap_us.c`). Host'ta TR-Q ise
  yazılan karakterler farklı olabilir. Eşlenemeyen karakterler atlanır.
- **Fare göreli hareket eder** (mutlak konum yok); cihaz imlecin nerede olduğunu
  bilmez. Hassas konumlama için küçük adımlar kullan.
- **macOS:** RNDIS yok → `make USE_ECM=1`. (Launch dizileri macOS için ayarlı
  değil.)
- **Flash kaydı** sırasında USB ~1-2 sn duraklar (128 KB sektör silme); bu yüzden
  otomatik değil, "Kaydet" ile.
- **STM32F103 "Blue Pill" yetersiz:** firmware tek başına ~**73 KB RAM** kullanır
  (çoğu lwIP tamponları); F103'ün 20 KB RAM'i yetmez, USB çevre birimi de bu yığın
  için uygun değildir. Birincil tasarım F4 üzerinedir.
- **VID/PID:** `0xCafe/0x4020` — TinyUSB test kimlikleri, kayıtlı değil.
- **Makro limitleri:** F411'de en çok **6 makro × 512 bayt**; F401RC'de (küçük-RAM)
  **4 makro × 256 bayt**.
- **F401RC (64 KB RAM):** Küçük-RAM profiliyle çalışır (RAM ~%50); lwIP tamponları
  ve makro boyutları F411'e göre daha küçüktür. Yoğun yükte (çok eşzamanlı
  bağlantı) F411 kadar bol değil ama tek tarayıcı için fazlasıyla yeterli.

---

## Durum / yol haritası

- [x] USB ağ + web sunucusu (RNDIS/ECM) + DHCP/DNS.
- [x] HID klavye + **fare** (tek arabirim, report ID).
- [x] **Makro motoru**: `launch / text / key / combo / wait / mouse`.
- [x] **Host profilleri**: Windows (Win+R) + Linux (KDE Alt+F2 / GNOME Süper).
- [x] **Birden çok makro**: arayüzden ekle/sil/seç/düzenle, flash'ta sakla.
- [ ] İsteğe bağlı sonraki: TR-Q klavye haritası, makro adım sayacı/ilerleme,
      mutlak-fare (HID digitizer) denemesi.

---

## Üçüncü parti ve lisans

`fetch_deps.sh` ile çekilen sabit sürümler (depoya dahil değil):

| Bileşen | Sürüm | Lisans |
|---------|-------|--------|
| TinyUSB | 0.21.0 | MIT |
| lwIP | STABLE-2_2_1_RELEASE | BSD-3 |
| STM32F4 HAL | v1.8.5 | BSD-3 |
| CMSIS Device F4 | v2.6.11 | Apache-2.0 |
| CMSIS Core | v5.9.0 | Apache-2.0 |

DHCP/DNS sunucusu ve RNDIS yardımcıları TinyUSB `lib/networking` (MIT, © 2020
Peter Lawrence); lwIP netif tümleştirmesi TinyUSB `net_lwip_webserver` örneğine
dayanır. Özgün firmware/web kodu: MIT.
