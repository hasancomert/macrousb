# MacroUSB

STM32 tabanlı, bilgisayara takıldığında **hem USB HID klavye hem de USB ağ
adaptörü** olarak görünen bir cihaz. Cihazın içinde küçük bir web sunucusu
çalışır; tarayıcıdan cihazın IP adresine girince bir **ayar sayfası** açılır.
Oradan bir metin ve tekrar aralığı girip **Başlat** dediğinde cihaz, o metni
belirlenen aralıkla klavye girdisi olarak yazar.

> **Ne işe yarar / amaç:** Kişisel bir **otomasyon / makro (oto-yazıcı)**
> aracıdır — kendi bilgisayarında, senin elle başlattığın, durumu LED ile belli
> olan şeffaf bir cihaz. Açılışta **otomatik yazmaya başlamaz**; yalnızca ayar
> sayfasındaki **Başlat** ile çalışır. Kendi donanımında ve kendi bilgisayarında
> kullanım için tasarlanmıştır.

---

## İçindekiler
- [Mimari](#mimari)
- [Donanım](#donanım)
- [Proje yapısı](#proje-yapısı)
- [Derleme](#derleme)
- [Flashleme](#flashleme)
- [Ağ / bağlanma (IP ve URL)](#ağ--bağlanma-ip-ve-url)
- [Windows notları (sürücü + güvenlik duvarı)](#windows-notları-sürücü--güvenlik-duvarı)
- [Linux notları](#linux-notları)
- [Doğrulama (Adım 2)](#doğrulama-adım-2)
- [Bilinen kısıtlar](#bilinen-kısıtlar)
- [Durum / yol haritası](#durum--yol-haritası)
- [Üçüncü parti ve lisans](#üçüncü-parti-ve-lisans)

---

## Mimari

Tek firmware, ayrı masaüstü uygulaması yok. USB'de **composite** cihaz olarak
enumerate olur:

| Arabirim | Sınıf | İş |
|----------|-------|----|
| 0 + 1 | USB ağ (varsayılan **RNDIS**, derlemeyle **CDC-ECM**) | Cihaz-içi web sunucusuna erişim |
| 2 | **HID klavye** | Asıl yazma işi |

Firmware içi bileşenler:

- **TinyUSB** — USB aygıt yığını (RNDIS/ECM ağ sınıfı + HID), STM32 OTG-FS
  (dwc2) sürücüsüyle.
- **lwIP** (NO_SYS, raw API) — TCP/IP yığını.
- **DHCP sunucusu** — host'a otomatik IP verir (cihaz `192.168.7.1`, host
  `192.168.7.2`).
- **DNS sunucusu** — `macrousb`, `setup.usb` gibi adları cihaza çözer.
- **HTTP/1.0 sunucusu** (bu projede yazılmış, ~250 satır, lwIP raw-TCP üzerinde)
  — tek sayfalık ayar arayüzünü ve `/api/*` uçlarını servis eder.
- **Ayar sayfası** — flash'a gömülü tek HTML dosyası (`web/index.html` →
  `build/generated/web_content.h`, `web/tools/embed.py` ile derleme sırasında
  üretilir).

### USB ağ sınıfı neden RNDIS?

Birincil şart "Windows'ta sürücüsüz çalışsın". Seçenekler:

| Sınıf | Windows (sürücüsüz) | Linux | macOS |
|-------|---------------------|-------|-------|
| **RNDIS** (varsayılan) | ✅ dahili | ✅ `rndis_host` | ❌ |
| CDC-ECM (`make USE_ECM=1`) | ❌ (ekstra .inf) | ✅ `cdc_ether` | ✅ |
| CDC-NCM | ⚠️ tutarsız | ✅ | ✅ |

Bu yüzden **RNDIS birincil**; Linux/macOS testini kolaylaştırmak için
derleme-zamanı bir anahtarla **ECM** varyantı da üretilebilir. NCM, Windows'ta
güvenilmez olduğu için kullanılmadı.

---

## Donanım

**Hedef: STM32F411CEU6 "Black Pill" V2.0 (WeAct).** 512 KB flash / 128 KB RAM,
USB OTG-FS.

| İşlev | Pin | Not |
|-------|-----|-----|
| USB D− / D+ | PA11 / PA12 | AF10 (OTG-FS), VBUS algılama **kapalı** |
| Durum LED'i | PC13 | Aktif-düşük (karttaki yerleşik LED) |
| Kullanıcı butonu | PA0 | Aktif-düşük ("KEY") |
| Saat | 25 MHz HSE | → 84 MHz SYSCLK, 48 MHz USB |

Kart-bağımlı her şey tek yerde: [`firmware/board/`](firmware/board). Başka bir
F4/F7'ye taşımak için yeni bir `board_<isim>.c` yazıp Makefile'daki
`CFG_TUSB_MCU`, startup dosyası ve linker scriptini güncellemek yeterli.

---

## Proje yapısı

```
macrousb/
├── firmware/
│   ├── board/                 # KART-BAĞIMLI kod (clock, pin, USB, LED)
│   │   ├── board.h
│   │   └── board_f411_blackpill.c
│   ├── src/
│   │   ├── main.c             # süper döngü (tud_task + lwIP + app)
│   │   ├── app.c/.h           # config + yazma durum makinesi + /api + LED + auto-open
│   │   ├── keymap_us.c/keymap.h  # ASCII -> HID (US QWERTY)
│   │   ├── flash_store.c/.h   # config'i flash sektör 7'de sakla/yükle
│   │   ├── tusb_config.h      # TinyUSB yapılandırması
│   │   ├── usb_descriptors.c  # composite RNDIS/ECM + HID tanımlayıcıları
│   │   ├── net_glue.c/.h      # lwIP netif + DHCP/DNS sunucu kurulumu
│   │   ├── http_server.c/.h   # minimal HTTP sunucusu + /api dağıtımı
│   │   ├── lwipopts.h         # lwIP yapılandırması
│   │   ├── stm32f4xx_hal_conf.h
│   │   ├── syscalls.c         # newlib stub'ları
│   │   └── arch/cc.h          # lwIP port başlığı
│   ├── linker/STM32F411CEUx_FLASH.ld
│   ├── third_party/           # fetch_deps.sh ile çekilir (git'e dahil değil)
│   │   └── fetch_deps.sh
│   └── Makefile
├── web/
│   ├── index.html             # tek sayfa ayar arayüzü
│   └── tools/embed.py         # index.html -> flash'a gömülü C dizisi
└── README.md
```

---

## Derleme

Gereksinimler: `arm-none-eabi-gcc` (+ `binutils`, `newlib`), `make`, `git`,
`python3`.

Debian/Ubuntu:
```bash
sudo apt-get update
sudo apt-get install -y gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi make git python3
```

Bağımlılıkları çek (TinyUSB, lwIP, STM32 HAL, CMSIS — sabit sürümlere sabitli):
```bash
cd firmware/third_party
./fetch_deps.sh
```

Derle:
```bash
cd firmware
make                 # varsayılan: RNDIS (Windows)
make USE_ECM=1       # alternatif: CDC-ECM (Linux/macOS)
```

Çıktı: `firmware/build/macrousb.elf`, `.bin`, `.hex`. Örnek boyut:
```
FLASH: 64 KB / 512 KB  (%12),   RAM: 42 KB / 128 KB  (%32)
```

> CubeIDE kullanmak istersen: CubeMX'i yalnızca clock/pin/USB-PHY init'ini
> incelemek/üretmek için kullanabilirsin; USB yığını olarak ST'nin kütüphanesi
> yerine TinyUSB kullanılır (RNDIS + composite + web sunucusu için en gerçekçi
> yol). Bu depo kendi başına `make` ile derlenir.

---

## Flashleme

Black Pill'de iki yol var:

**ST-Link ile** (SWD: SWDIO/SWCLK/GND/3V3):
```bash
make flash-st        # st-flash kullanır
# veya STM32CubeProgrammer:
STM32_Programmer_CLI -c port=SWD -w build/macrousb.hex -rst
```

**USB DFU ile** (ST-Link gerekmez): BOOT0 butonuna basılı tutarken karta güç ver
(veya NRST'ye bas), cihaz DFU moduna girsin, sonra:
```bash
make flash-dfu       # dfu-util kullanır
```

---

## Ağ / bağlanma (IP ve URL)

Cihaz takılınca bir USB ağ adaptörü olarak görünür ve host'a DHCP ile IP verir:

| | Adres |
|-|-------|
| Cihaz (web sunucusu) | **192.168.7.1** |
| Host (senin PC'n, DHCP) | 192.168.7.2 |

Ayar sayfası: **http://192.168.7.1/**
(DNS sunucusu sayesinde `http://macrousb/` veya `http://setup.usb/` de çalışır.)

---

## Windows notları (sürücü + güvenlik duvarı)

- **Sürücü:** RNDIS ile Windows cihazı sürücüsüz tanır ("USB RNDIS Adapter" /
  "Remote NDIS Compatible Device"). Ekstra kurulum gerekmez.
- **Güvenlik duvarı:** Windows Defender Firewall yeni USB ağını/HTTP'yi
  bloklayabilir. Test için en kolayı, yeni ağ "Genel/Ortak" olarak işaretlendiyse
  ona izin vermek; ya da (hocanın istediği gibi) test sırasında güvenlik
  duvarını geçici kapatmak:
  - Denetim Masası → Windows Defender Güvenlik Duvarı → Aç/Kapat, ya da
  - `http://192.168.7.1/` ilk açılışta çıkan ağ izni istemine izin ver.
- Ağ adaptörü "Tanımlanamayan ağ" görünebilir; bu normaldir, HTTP yine çalışır.

---

## Linux notları

- RNDIS build'i `rndis_host`, ECM build'i (`make USE_ECM=1`) `cdc_ether` ile
  tanınır. Çoğu dağıtımda otomatik yüklenir.
- Arayüzü ve IP'yi kontrol et:
  ```bash
  ip link                       # yeni usbX/enX arayüzü
  dmesg | tail                  # rndis_host/cdc_ether bağlandı mı
  ```
- DHCP otomatik IP vermezse arayüz genelde kendiliğinden 192.168.7.2 alır; sorun
  olursa elle:
  ```bash
  sudo ip addr add 192.168.7.2/24 dev enXXX
  sudo ip link set enXXX up
  ```

---

## Kullanım

1. Cihazı tak, tarayıcıda **http://192.168.7.1/** aç.
2. **Yazılacak metin** kutusuna metni gir (ör. `w24rafsfe`).
3. **Tekrar aralığı**nı saniye olarak gir (ondalık olabilir, ör. `2.5`).
4. İstersen **Her tekrardan sonra Enter**'ı işaretle.
5. **Başlat**'a bas. ⚠️ Yazılan tuşlar **o an odakta olan pencereye** gider;
   Başlat'tan sonra metnin gitmesini istediğin pencereye (ör. bir metin
   editörü) tıkla. İlk tuş, güvenlik için **bir aralık (interval) sonra** basılır.
6. **Durdur** ile her an durdurabilirsin. Durum ve tekrar sayısı sayfada
   canlı görünür.

### Opsiyonel: ayarları kaydetme ve otomatik açma

- **Ayarları cihaza kaydet (flash):** Girdiğin metin/aralık/seçenekler cihazın
  flash'ına yazılır ve **tak-çıkar sonrası hatırlanır** (otomatik başlatmaz,
  sadece alanları doldurur). Kaydederken flash silinirken USB **~1-2 sn kısa
  süre duraklayabilir**; bu normaldir.
- **Takınca bu sayfayı otomatik aç (deneysel):** İşaretleyip kaydedersen, cihaz
  bir sonraki takılışında ~8 sn sonra **Win+R → cihaz URL'si → Enter** dizisini
  klavye olarak basar, böylece varsayılan tarayıcıda ayar sayfası açılır.
  Zamanlaması hassastır (sürücü kurulumu/odak); yalnızca Windows'ta anlamlıdır ve
  başarısız olabilir — bu yüzden varsayılan **kapalı**.

### Durum LED'i (PC13)

| Durum | LED |
|-------|-----|
| Host'a bağlı değil | hızlı yanıp söner (~8 Hz) |
| Boşta (durduruldu) | ~2 sn'de bir kısa çakma |
| Çalışıyor, bekliyor | sabit yanık |
| Yazıyor (tuş basıyor) | titrer (~8 Hz) |

## Doğrulama

### Adım 2 — enumerate + web sayfası

Bu adımın hedefi: **cihaz USB ağ olarak enumerate olsun ve statik ayar sayfası
tarayıcıdan açılsın.** (Not: Bu ortamda derleme doğrulandı; aşağıdaki adımlar
gerçek donanımda senin yapacağın kontroller.)

1. `make` ile firmware'i derle, karta flash'la.
2. Kartı USB ile PC'ye tak.
3. **Enumerate kontrolü:**
   - Windows: Aygıt Yöneticisi → Ağ bağdaştırıcıları → "USB RNDIS…" görünmeli.
   - Linux: `ip link` yeni bir arayüz, `dmesg` `rndis_host` satırı göstermeli.
4. Durum LED'i (PC13): host'a bağlıyken, boştayken ~2 sn'de bir kısa çakar
   (LED tablosuna bak).
5. Tarayıcıda **http://192.168.7.1/** aç → MacroUSB ayar sayfası gelmeli.

### Adım 3–4 — ayar formu + yazma

1. Bir metin editörü aç (Not Defteri / gedit / herhangi bir metin kutusu).
2. Ayar sayfasında metni ve aralığı gir, **Başlat**'a bas, sonra editör
   penceresine tıkla.
3. Her `interval` saniyede metnin yazıldığını gör; "Enter" işaretliyse her
   tekrardan sonra yeni satır. Sayfadaki durum "Çalışıyor" ve tekrar sayacı
   artmalı; LED sabit yanıp yazma anında titremeli.
4. **Durdur**'a bas → yazma durur, durum "Boşta" olur.
5. Farklı aralık/metin ile tekrar dene; aralık `0.1`–`3600` sn aralığına
   sıkıştırılır.

### Adım 5 — opsiyonel özellikler

- **Flash kalıcılığı:** Ayarları gir, **Ayarları cihaza kaydet**'e bas (sayfa
  "kaydedildi" der). Kartı çıkar-tak; sayfayı aç → alanların kayıtlı değerlerle
  dolu geldiğini gör (yazma yine elle Başlat ister).
- **Otomatik aç:** "Takınca bu sayfayı otomatik aç"ı işaretle, kaydet, kartı
  çıkar-tak. ~8 sn sonra Windows'ta Çalıştır penceresi açılıp URL yazılmalı ve
  tarayıcı ayar sayfasını açmalı. Açılmazsa (odak/sürücü/zamanlama) elle
  `http://192.168.7.1/` adresine gir.

---

## Bilinen kısıtlar

- **Klavye düzeni:** Firmware US QWERTY varsayar (karakter→HID eşlemesi
  `src/keymap_us.c`). Host'un klavye düzeni farklıysa (ör. TR-Q), basılan tuşlar
  farklı karakterler üretebilir. Başka düzen için `keymap_us.c` değiştirilir.
- **Yazılabilen karakterler:** Yalnızca US QWERTY'de tek tuş/Shift ile üretilen
  ASCII karakterler. Eşlenemeyen karakterler atlanır.
- **macOS:** RNDIS'i desteklemez. macOS için `make USE_ECM=1` ile ECM build'i
  kullan.
- **STM32F103 "Blue Pill" neden yetersiz:** Bu firmware tek başına ~**50 KB RAM**
  kullanıyor (lwIP tamponları + USB + TinyUSB). F103'ün toplam **20 KB RAM**'i
  bunun çok altında; LwIP + USB ağ + HTTP aynı anda sığmaz. Ayrıca F103'ün USB
  çevre birimi OTG değil tam-hızlı device-only'dir ve bu yığın onun için
  uygun değildir. Bu yüzden birincil tasarım F4 üzerinedir.
- **Otomatik aç (deneysel):** Zamanlamaya dayanır (sürücü kurulumu, pencere
  odağı, ağ hazır olması). Başarısız olabilir; elle URL'ye girmek her zaman
  çalışır. Yalnızca Windows'ta anlamlıdır.
- **Flash kaydı:** Kaydetme işlemi tüm 128 KB'lik sektörü siler ve bu sırada MCU
  flash'tan kod çalıştıramadığı için USB **~1-2 sn duraklar**. Her ayar
  değişiminde değil, yalnızca "Kaydet"e basınca yazılır (flash ömrü için).
- **VID/PID:** `0xCafe/0x4020` — TinyUSB'nin test kimlikleri; kayıtlı USB
  kimlikleri değildir. Kişisel kullanım için uygundur, ürünleştirme için değil.

---

## Durum / yol haritası

- [x] **Adım 1** — Plan + USB sınıfı kararı (RNDIS) + yığın (TinyUSB) onayı.
- [x] **Adım 2** — USB ağ enumerate + statik ayar sayfası (web sunucusu çalışır).
- [x] **Adım 3** — Ayar formu + config saklama (`/api/config|start|stop|status`).
- [x] **Adım 4** — HID klavye yazma (Başlat/Durdur + aralık + Enter) + US keymap +
      idle/running/typing LED durumları.
- [x] **Adım 5** — (Opsiyonel) "sayfayı otomatik aç" (Win+R dizisi) + config'i
      flash'ta saklama.

---

## Üçüncü parti ve lisans

`firmware/third_party/fetch_deps.sh` ile çekilen, sabit sürümlere sabitli
bağımlılıklar (depoya dahil değil):

| Bileşen | Sürüm | Lisans |
|---------|-------|--------|
| TinyUSB | 0.21.0 | MIT |
| lwIP | STABLE-2_2_1_RELEASE | BSD-3 |
| STM32F4 HAL | v1.8.5 | BSD-3 |
| CMSIS Device F4 | v2.6.11 | Apache-2.0 |
| CMSIS Core | v5.9.0 | Apache-2.0 |

DHCP/DNS sunucusu ve RNDIS protokol yardımcıları TinyUSB'nin
`lib/networking` örneğinden (MIT, © 2020 Peter Lawrence) gelir. lwIP netif
tümleştirmesi TinyUSB `net_lwip_webserver` örneğine dayanır.

Bu depodaki özgün firmware/web kodu için lisans: MIT.
