# Offline console test: 1.05

The user confirmed they use the controller's analog stick and prefer no
network/FTP setup. Treat physical USB-mouse support as outside this regression.

Version 1.05 retains the 1.04 empty-mount filtering, vsync-reporting correction,
elapsed-time pointer integration, quadratic response and SDL neutral fix.
It adds a compact status headline to the root of the Add Game browser:

- USB ready: a populated path is readable by the app.
- U1: the homebrew storage service could not provide credentials.
- U2: console storage access could not be established.
- U3: no readable non-dot files were found in any real USB source directory.
- U4: a populated source was found, but its app-local mapping is not readable.
  The bracketed number is the mapping result.
- U6: restoring storage-access credentials failed.

When available, "input N Hz" is appended. This is the average virtual-pointer
update rate during recent active movement, not a display-FPS measurement.
Move the analog stick for a few seconds before opening Add Game; return to
root with Go up to read/photograph the headline. No network is required.

The headline reports the latest storage probe and pointer sample; it is not
continuously refreshed while the browser is stationary. Returning to root or
reopening the dialog refreshes it. Detailed local logs are still retained.

The storage sanitizer tests assert the U1, U3 and ready states. Real-console
USB access and subjective pointer smoothness still require the user's retest.
This remains a private test build; the libjbc release audit is unresolved.

## Delivery

Final 1.05 package SHA256:
261d7c3d74c20ae6e1e0d601f22b61bb4bcb92a05f402e7d549075ac1373e9d9

The package passes PkgTool validation and the storage status assertions pass
with address/undefined sanitizers. Copied to SCUMMVM_PS4 USB, with original
DOTT files separate in scummvm/games/dott and offline instructions at root.
All delivered files were hashed again after remount; USB was safely ejected.
On-screen rendering and real-console behavior remain for the offline retest.
