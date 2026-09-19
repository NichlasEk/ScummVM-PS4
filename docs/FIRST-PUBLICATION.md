# Första publiceringen — sparat 2026-09-19

## Uppdatering efter PS4-test

Publicering av 1.01 avvaktar: användaren rapporterade ryckig pekare och
CE-34878-0 när spel skulle läggas till. Se [1.02-utredningen](PS4-1.02-REGRESSION.md).
Mappen nedan är en äldre kandidat och ska ersättas efter verifierad korrigering.

## Var vi står

Forumkontot väntar på manuell granskning, enligt användaren. Vi avvaktar
kontots godkännande. Ingen video behövs och ingen forumtråd har publicerats.
GitHub-repot var PRIVATE vid senaste kontrollen 2026-09-19:
https://github.com/NichlasEk/ScummVM-PS4
Synligheten har inte ändrats av assistenten.

Portens implementation finns i commit `9b8466db7b2c8e27f992b8114e962e3fa2c07277`.
Det är en inofficiell native PS4-port av ScummVM 2.9.1 med SCUMM-motorn.
DOTT har bekräftats fungera på fysisk PS4 av användaren. Det fristående
1.01-paketet har ScummVM-ikon och 8× pekarhastighet. USB-bläddring och
extern speldetektering har verifierats i emulatorn. Fysisk USB-läsning,
nya känsligheten och save/load behöver dokumenteras vid fortsatt testning.

## Förberedd mapp

`dist/first-upload-1.01/` är en lokal publiceringskandidat, ignorerad av Git.

- `FORUM-POST.md`: engelskt utkast att klistra in när kontot godkänts.
- `BEFORE-UPLOAD.md`: denna återupptagningsanteckning.
- `files/ScummVM-PS4-2.9.1.pkg`: fristående paket, 12 845 056 byte.
- `files/README.md`: installation, separata spel, kontroller och begränsningar.
- `files/LICENSE`: GPLv3-licenstext.
- `files/scummvm-2.9.1-upstream-source.tar.gz`: ren, exakt upstream-källkod.
- `files/scummvm-ps4-port-source-9b8466d.tar.gz`: portens patch, byggskript,
  ikon och dokumentation från den implementation som byggde paketet.
- `files/SHA256SUMS`: kontrollsummor för samtliga ovanstående releasefiler.

Paketets SHA-256:
`2699e40e22e9a11346ed420d601c22ddd4cbe3689995725de8079c1702122020`

Upstream-revision: `9d9b9e93108a276c551aeffa390169ccc5148e15`.
Källarkiven tillsammans ger ScummVM-källkoden och PS4-ändringarna.
De omfattar ännu inte alla medföljande runtime-komponenters källkod.

## Återstår innan en publik binärrelease

1. Kontrollera ursprung, licenser och motsvarande källkod för komponenterna
   från OpenOrbis: statiskt länkade bibliotek, `libc.prx`, `libSceFios2.prx`
   och `right.sprx`. Samla deras licensnotiser och nödvändig källkod.
   Kontrollera även distributionsvillkoren för upstream-ikonen.
   Detta är ännu inte en fullständig licensgranskad release.
2. Gör källkoden tillgänglig för mottagarna. Repot är för närvarande privat;
   ändra synlighet först när användaren väljer att publicera det.
3. Ladda upp enbart det fristående paketet och tillhörande källkod/licenser.
   Publicera aldrig `-DOTT.pkg`, ISO, USB-innehållet eller speldata.
4. När kontot godkänts och nedladdningen är tillgänglig: ersätt platshållarna
   i forumutkastet med riktiga publika länkar och publicera manuellt.

Inga kommersiella spelfiler, ISO-filer eller Sony SDK-filer finns i de
förberedda källarkiven. Paketets extraherade fillista kontrollerades tidigare
utan speldata. Vid nästa tillfälle: verifiera `SHA256SUMS` före uppladdning.

Bakgrund:
https://docs.scummvm.org/en/latest/help/faq.html#is-scummvm-free
https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain
