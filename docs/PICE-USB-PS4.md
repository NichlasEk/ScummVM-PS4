# PICE på riktig PS4

USB-paketet använder den separata titeln **ScummVM PICE Preview**,
SCVM00003. DOTT-paketet SCVM00002 och dess konfiguration behålls.

1. Anslut stickan till PS4 och installera `ScummVM-PS4-PICE-preview.pkg`
   med konsolens Package Installer.
2. Starta **ScummVM PICE Preview** med stickan kvar.
3. Efter startfilmerna: välj **New game**, bekräfta **Yes**.
   Den fullständiga prologen leder till den spelbara bryggan före lådorna.

Spelfilerna ligger i `scummvm/games/pice` på USB. Starten söker USB 0–7,
både direktåtkomst och PS4:s `/scummvm_usbN`-alias. En direkt spelmapp i
USB-roten stöds fortfarande för äldre emulatorprofiler.

`scummvm/pice.ini` på USB används som startkonfiguration och kopieras bara
första gången till `/data/scummvm/pice.ini`. Därefter bevaras dina inställningar
på konsolen. Sparningar skrivs till `/data/scummvm/pice-saves`, eftersom
USB:s sandbox-mappning är skrivskyddad. Ändringar i USB-konfigurationen
efter första starten ersätter alltså inte befintliga konsolinställningar.
Ingen testsparning eller automatiserad input följer med på USB.

Paketet innehåller inga kommersiella spelfiler. Stickan innehåller användarens
separata engelska CD-data. Undertexter är på, tal/musik/effekter börjar på 75 %.
Ingen automatisk laddning av sparning sker vid start.

Bygg och emulator är verifierade. Fysisk PS4, USB-mappning och spelkontroll
behöver fortfarande provas på konsolen; en genomspelning i emulatorn är
inte bevis för att allt fungerar identiskt på hårdvaran.

## Leveranskontroll 2026-09-22

`artifacts/pice/usb-layout-check.log` testar den faktiska USB-stickan monterad
som `/scummvm_usb0` i en isolerad shadPS4-profil. Starten hittar
`/scummvm_usb0/scummvm/games/pice`, skapar den separata konfigurationen,
bevarar den vid omstart, klarar parserns självtester, når huvudmenyn med
tomma PICE-sparplatser och startar originalprologen i rum 39. Filmhopp och
menyval automatiserades endast i emulatorns interna testkonfiguration.
USB-filen innehåller inga sådana testflaggor.

`PICE-SHA256SUMS.txt` på stickan innehåller paketets, konfigurationens och
samtliga 86 spelfilers kontrollsummor. Den äldre DOTT-installationen lämnades
kvar. Lokal kopia av manifestet finns i artifacts/pice/usb-copy-verification.txt.
