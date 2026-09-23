#ifdef SOTC_TESTBENCH
 char gamePath[256] = "";
 for (int slot = 0; slot < 8 && !gamePath[0]; ++slot) {
  const char *suffixes[] = {"/scummvm/games/sotc/SHADOW", "/scummvm/games/sotc", "/SHADOW", ""};
  for (int alias = 0; alias < 2 && !gamePath[0]; ++alias) for (int i = 0; i < 4 && !gamePath[0]; ++i) {
   char candidate[256], file[300];
   snprintf(candidate, sizeof(candidate), alias ? "/scummvm_usb%d%s" : "/mnt/usb%d%s", slot, suffixes[i]);
   snprintf(file, sizeof(file), "%s/RES.PAK", candidate);
   FILE *probe = fopen(file, "rb");
   if (probe) { fclose(probe); snprintf(gamePath, sizeof(gamePath), "%s", candidate); }
  }
 }
 int res;
 if (gamePath[0]) {
  char pathArg[300]; snprintf(pathArg, sizeof(pathArg), "--path=%s", gamePath);
  char a0[] = "scummvm", a1[] = "--debuglevel=3", a2[] = "sotc";
  char *args[] = {a0, pathArg, a1, a2};
  printf("[sotc-boot] game=%s\n", gamePath);
  res = scummvm_main(4, args);
 } else res = scummvm_main(argc, argv);
#else
 int res = scummvm_main(argc, argv);
#endif
