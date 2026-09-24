package org.scummvm.scummvm;

import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;

/** Validates either a PS4 quick save or an Android export before replacing any slot. */
final class SaveImport {
    static byte[] readBounded(InputStream in, int limit) throws IOException {
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] buf = new byte[8192]; int n;
        while ((n = in.read(buf)) != -1) {
            if (out.size() + n > limit) throw new IOException("Filen är för stor");
            out.write(buf, 0, n);
        }
        return out.toByteArray();
    }
    static long word(byte[] data, int pos) {
        return ((long)data[pos] & 255) | (((long)data[pos+1] & 255) << 8)
            | (((long)data[pos+2] & 255) << 16) | (((long)data[pos+3] & 255) << 24);
    }
    static byte[] validate(byte[] data) throws IOException {
        if (data.length > 2 && data[0] == (byte)0x1f && data[1] == (byte)0x8b)
            data = readBounded(new GZIPInputStream(new ByteArrayInputStream(data)), 400000);
        if (data.length < 36 || data.length > 400000 || word(data,0) != 0x43544f53L
                || word(data,4) < 1 || word(data,4) > 7 || word(data,8) == 0)
            throw new IOException("Inte en kompatibel SOTC-sparning");
        int hash = (int)2166136261L;
        for (int i=16; i<data.length; i++) hash = (hash ^ (data[i] & 255)) * 16777619;
        if ((hash & 0xffffffffL) != word(data,12)) throw new IOException("Sparningens kontrollsumma stämmer inte");
        return data;
    }
    static byte[] decode(InputStream input) throws IOException {
        byte[] bytes = readBounded(input, 2000000);
        if (bytes.length < 4 || bytes[0] != 'P' || bytes[1] != 'K') return validate(bytes);
        byte[] latest = null; int entries = 0, total = 0;
        try (ZipInputStream zip = new ZipInputStream(new ByteArrayInputStream(bytes))) {
            ZipEntry entry;
            while ((entry = zip.getNextEntry()) != null) {
                if (++entries > 32) throw new IOException("För många filer i arkivet");
                byte[] data = readBounded(zip, 400000); total += data.length;
                if (total > 2000000) throw new IOException("Arkivet är för stort");
                if (!entry.isDirectory() && entry.getName().matches("[^/\\\\]+-sotc\\.quick[01]")) {
                    data = validate(data);
                    if (latest == null || word(data,8) > word(latest,8)) latest = data;
                }
            }
        }
        if (latest == null) throw new IOException("Ingen SOTC-sparning i zip-filen");
        return latest;
    }
    static String target(File config) throws IOException {
        Map<String,Map<String,String>> sections = new LinkedHashMap<>();
        String section = "scummvm";
        if (config.exists()) for (String raw : Files.readAllLines(config.toPath())) {
            String line = raw.trim();
            if (line.startsWith("[") && line.endsWith("]")) section = line.substring(1,line.length()-1);
            else if (line.contains("=") && !line.startsWith("#")) {
                int at = line.indexOf('=');
                sections.computeIfAbsent(section,k -> new HashMap<>()).put(line.substring(0,at).trim(),line.substring(at+1).trim());
            }
        }
        Map<String,String> global = sections.getOrDefault("scummvm", Collections.emptyMap());
        String chosen = global.get("lastselectedgame");
        if (chosen == null || !"sotc".equals(sections.getOrDefault(chosen,Collections.emptyMap()).get("gameid"))) {
            List<String> targets = new ArrayList<>();
            for (String key : sections.keySet()) if ("sotc".equals(sections.get(key).get("gameid"))) targets.add(key);
            if (targets.size() > 1) throw new IOException("Markera rätt SOTC-spel i startlistan först");
            chosen = targets.isEmpty() ? "sotc" : targets.get(0);
        }
        if (!chosen.matches("[A-Za-z0-9_-]+")) throw new IOException("Ogiltigt spel-ID");
        Map<String,String> game = sections.getOrDefault(chosen,Collections.emptyMap());
        if (!game.getOrDefault("savepath", "").isEmpty() || !global.getOrDefault("savepath", "").isEmpty())
            throw new IOException("Välj standardmappen för sparningar i ScummVM först");
        return chosen + "-sotc";
    }
    static void install(File directory, String prefix, byte[] data) throws IOException {
        Files.createDirectories(directory.toPath());
        Path backup = Files.createTempDirectory(directory.toPath(), "before-import-");
        for (int i=0;i<2;i++) {
            Path file = directory.toPath().resolve(prefix+".quick"+i);
            if (Files.exists(file)) Files.copy(file,backup.resolve(file.getFileName()));
        }
        try {
            for (int i=0;i<2;i++) {
                Path file = directory.toPath().resolve(prefix+".quick"+i);
                Path staged = Files.createTempFile(directory.toPath(), "import-", ".tmp");
                try { Files.write(staged,data); Files.move(staged,file,StandardCopyOption.ATOMIC_MOVE,StandardCopyOption.REPLACE_EXISTING); }
                finally { Files.deleteIfExists(staged); }
            }
        } catch (IOException failure) {
            for (int i=0;i<2;i++) {
                Path file = directory.toPath().resolve(prefix+".quick"+i), old = backup.resolve(file.getFileName());
                if (Files.exists(old)) Files.copy(old,file,StandardCopyOption.REPLACE_EXISTING);
                else Files.deleteIfExists(file);
            }
            throw failure;
        }
    }
}
