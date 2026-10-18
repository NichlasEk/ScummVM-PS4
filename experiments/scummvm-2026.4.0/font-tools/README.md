# Jönsson-dialogfont: ingen extra fil i det nya testbygget

Den manuella konverteringen nedan är ersatt i det senaste lokala testbygget.
ScummVM läser Sand-fontens PFR1-data direkt från XMED-resursen i spelets
`Data/Dialog.cst`, konverterar den en gång i minnet och ritar kantutjämnad text.
Det behövs ingen `jonsson-dialogue.ttf`, inget Python och inget extra steg för
spelaren. En gammal TTF-fil används inte längre av den nya implementationen.

Ingår från PKG 0.17. Användaren har godkänt utseendet i shadPS4; fontändringen
återstår att verifiera på fysisk PS4. Saknad eller oläsbar font använder reservfont.

## Utveckling och licens

Avkodaren bygger på fontdelen i [LibreShockwave](https://github.com/LibreShockwave/LibreShockwave),
revision `fca530f9ef388d7ff38fa6c7117feae5bb5411c6`, under AGPL-3.0.
Källkod, licens och lokala ändringar finns i det förberedda källträdet under
`engines/director/fonts/pfr1/` och följer Director-patchen. Detta är en ny
licensbunden komponent i testbygget: dess licens och motsvarande källkod måste
följa en distribution. Spelets fontdata ingår aldrig i appen eller repot.

Lokala korrigeringar omfattar felaktig kort/lång loca-flagga och osäkra
bitförskjutningar av negativa koordinater. Konverterargränsen fångar undantag
och begränsar filstorlekar. Utmatningen har testats med ASan/UBSan och kontroller
av synliga A–Z samt svenska bokstäver från användarens lokala spelkopia.

De två äldre konverteringsverktygen i denna katalog behålls endast som
utvecklingsreferens. De är inte längre installationsanvisningar för spelare.
