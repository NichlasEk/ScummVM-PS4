#ifdef PICE_TESTBENCH
 // Prefer the USB layout; retain bare-root mounts for emulator regression profiles.
 char gamePath[256] = "/mnt/usb0", usbConfig[256] = "";
 bool found = false;
 for (int attempt = 0; attempt < 2 && !found; ++attempt) {
  if (attempt) ps4_refresh_usb_mounts();
  for (int slot = 0; slot < 8 && !found; ++slot) for (int alias = 0; alias < 2 && !found; ++alias) {
   char root[64]; snprintf(root, sizeof(root), alias ? "/scummvm_usb%d" : "/mnt/usb%d", slot);
   for (int nested = 1; nested >= 0 && !found; --nested) {
    char candidate[256], resource[300];
    snprintf(candidate, sizeof(candidate), "%s%s", root, nested ? "/scummvm/games/pice" : "");
    snprintf(resource, sizeof(resource), "%s/KSVGA.KRO", candidate);
    FILE *probe = fopen(resource, "rb");
    if (!probe) {
     snprintf(resource, sizeof(resource), "%s/MAC_KRO/KSVGA.KRO", candidate);
     probe = fopen(resource, "rb");
    }
    if (!probe) continue;
    fclose(probe); found = true;
    snprintf(gamePath, sizeof(gamePath), "%s", candidate);
    snprintf(usbConfig, sizeof(usbConfig), "%s/scummvm/pice.ini", root);
   }
  }
 }
 char pathArg[300]; snprintf(pathArg, sizeof(pathArg), "--path=%s", gamePath);
 char arg0[] = "scummvm", arg2[] = "--debuglevel=3", arg3[] = "--debugflags=detection", arg4[] = "pice";
 char configArg[] = "--config=/data/scummvm/pice.ini";
 char *testArgs[] = {arg0, pathArg, arg2, arg3, arg4, nullptr, nullptr};
 int testArgc = 5;
 FILE *seed = found ? fopen(usbConfig, "rb") : nullptr;
 if (seed) {
  // USB sandbox mounts are read-only. Seed a separate writable PICE config
  // once, keeping DOTT settings and subsequent PICE option changes intact.
  mkdir("/data/scummvm/pice-saves", 0777);
  FILE *existing = fopen("/data/scummvm/pice.ini", "rb");
  if (existing) fclose(existing);
  else {
   FILE *destination = fopen("/data/scummvm/pice.ini", "wb");
   if (destination) {
    char buffer[1024]; size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), seed)) != 0) fwrite(buffer, 1, count, destination);
    fclose(destination);
   }
  }
  fclose(seed); testArgs[4] = configArg; testArgs[testArgc++] = arg4;
 }
 printf("[pice-boot] game=%s config=%s\n", gamePath, testArgc == 6 ? "/data/scummvm/pice.ini" : "default");
 int res = scummvm_main(testArgc, testArgs);
#else
 int res = scummvm_main(argc, argv);
#endif
