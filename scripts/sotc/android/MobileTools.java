package org.scummvm.scummvm;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.view.Gravity;
import android.view.KeyEvent;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.Toast;
import java.io.File;
import java.io.FileInputStream;
import java.io.OutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/** Small test-build controls; game logic stays in the shared SOTC engine. */
final class MobileTools {
    static final int EXPORT = 50120;
    static final int IMPORT = 50121;
    static void install(Activity activity, FrameLayout layout, ScummVM engine) {
        Button button = new Button(activity);
        button.setText("SOTC");
        button.setContentDescription("SOTC: spara, ladda och exportera");
        int size = (int)(48 * activity.getResources().getDisplayMetrics().density);
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(size * 2, size,
                Gravity.BOTTOM | Gravity.LEFT);
        layout.addView(button, params);
        button.setOnClickListener(v -> new AlertDialog.Builder(activity)
            .setTitle("Shadow of the Comet · test")
            .setItems(new String[]{"Spara i spelet", "Ladda senaste sparning", "Spelmeny",
                    "Exportera sparningar", "Importera sparning", "Hjälp"}, (dialog, which) -> {
                if (which < 3) {
                    int key = which == 0 ? KeyEvent.KEYCODE_Q : which == 1 ? KeyEvent.KEYCODE_U : KeyEvent.KEYCODE_ESCAPE;
                    int unicode = which == 0 ? 'q' : which == 1 ? 'u' : 27;
                    engine.pushEvent(ScummVMEvents.JE_KEY, KeyEvent.ACTION_DOWN, key, unicode, 0, 0, 0);
                    engine.pushEvent(ScummVMEvents.JE_KEY, KeyEvent.ACTION_UP, key, unicode, 0, 0, 0);
                } else if (which == 3) {
                    Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("application/zip");
                    intent.putExtra(Intent.EXTRA_TITLE, "SOTC-saves-" + System.currentTimeMillis() + ".zip");
                    activity.startActivityForResult(intent, EXPORT);
                } else if (which == 4) {
                    new AlertDialog.Builder(activity).setTitle("Importera sparning")
                        .setMessage("Välj en quick0/quick1-fil eller en exporterad zip. Importen ersätter snabb­sparningen; den gamla sparas som säkerhetskopia. Ladda sedan via SOTC-menyn medan spelet körs.")
                        .setNegativeButton("Avbryt", null)
                        .setPositiveButton("Välj fil", (d, w) -> {
                            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                            intent.addCategory(Intent.CATEGORY_OPENABLE);
                            intent.setType("*/*");
                            activity.startActivityForResult(intent, IMPORT);
                        }).show();
                } else {
                    new AlertDialog.Builder(activity).setTitle("Kom igång")
                        .setMessage("Välj Add Game och mappen med dina egna CD-spelfiler. Välj Shadow of the Comet och Start.\n\nMusknappen uppe till höger växlar pekläge. För markören till bildens överkant för spelets verktygsrad.\n\nSpara och ladda med SOTC-knappen medan spelet körs. Exportera sparningar efter ett stopp och skicka zip-filen tillsammans med en skärmbild. Export gäller standardmappen för sparningar.\n\nExperimentell motor, samma källkod som PS4-versionen. Spelfiler ingår inte.")
                        .setPositiveButton("OK", null).show();
                }
            }).show());
    }
    static void importSave(Activity activity, Uri source) {
        new Thread(() -> {
            String message;
            try (java.io.InputStream input = activity.getContentResolver().openInputStream(source)) {
                byte[] data = SaveImport.decode(input);
                File base = activity.getFilesDir();
                SaveImport.install(new File(base, "saves"), SaveImport.target(new File(base, "scummvm.ini")), data);
                message = "Importerad! Starta spelet och välj SOTC → Ladda senaste sparning.";
            } catch (Exception error) { message = "Import misslyckades: " + error.getMessage(); }
            final String result = message;
            activity.runOnUiThread(() -> new AlertDialog.Builder(activity).setMessage(result).setPositiveButton("OK",null).show());
        }, "sotc-import").start();
    }
    static void export(Activity activity, Uri destination) {
        new Thread(() -> {
            String result;
            try {
                File folder = new File(activity.getFilesDir(), "saves");
                File[] files = folder.listFiles(f -> f.isFile() && f.getName().matches(".*-sotc\\.quick[01]"));
                if (files == null || files.length == 0) throw new java.io.IOException("Ingen SOTC-sparning i standardmappen. Spara i spelet först.");
                try (OutputStream stream = activity.getContentResolver().openOutputStream(destination);
                     ZipOutputStream zip = new ZipOutputStream(stream)) {
                    byte[] buffer = new byte[8192];
                    for (File file : files) {
                        zip.putNextEntry(new ZipEntry(file.getName()));
                        try (FileInputStream in = new FileInputStream(file)) {
                            int n; while ((n = in.read(buffer)) != -1) zip.write(buffer, 0, n);
                        }
                        zip.closeEntry();
                    }
                    zip.putNextEntry(new ZipEntry("README.txt"));
                    zip.write("SOTC Android preview 0.1.1. Alternating quick saves; retain both files.\n".getBytes("UTF-8"));
                    zip.closeEntry();
                }
                result = "Sparningarna har exporterats";
            } catch (Exception error) {
                result = "Export misslyckades: " + error.getMessage();
            }
            final String message = result;
            activity.runOnUiThread(() -> Toast.makeText(activity, message, Toast.LENGTH_LONG).show());
        }, "sotc-export").start();
    }
}
