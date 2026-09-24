package org.scummvm.scummvm;
import java.io.*;
import java.nio.file.*;
public class SaveImportTest {
 public static void main(String[] args) throws Exception {
  byte[] ps4=SaveImport.decode(Files.newInputStream(Path.of(args[0])));
  byte[] android=SaveImport.decode(Files.newInputStream(Path.of(args[1])));
  byte[] zip=SaveImport.decode(Files.newInputStream(Path.of(args[2])));
  if(!java.util.Arrays.equals(android,zip)) throw new AssertionError("zip changed save");
  byte[] bad=ps4.clone();bad[30]^=1;
  try{SaveImport.validate(bad);throw new AssertionError("corrupt accepted");}catch(IOException expected){}
  try{SaveImport.decode(new ByteArrayInputStream(new byte[2000001]));throw new AssertionError("oversize accepted");}catch(IOException expected){}
  Path root=Files.createTempDirectory("sotc-import-test");
  SaveImport.install(root.toFile(),"sotc-sotc",android);
  SaveImport.install(root.toFile(),"sotc-sotc",ps4);
  for(int i=0;i<2;i++)if(!java.util.Arrays.equals(ps4,Files.readAllBytes(root.resolve("sotc-sotc.quick"+i))))throw new AssertionError();
  boolean kept=false;
  try(var dirs=Files.list(root)) { for(Path d:dirs.filter(Files::isDirectory).toList()) {
   Path old=d.resolve("sotc-sotc.quick0");
   if(Files.exists(old) && java.util.Arrays.equals(android,Files.readAllBytes(old))) kept=true;
  }}
  if(!kept)throw new AssertionError("missing backup");
  Path cfg=root.resolve("scummvm.ini");
  Files.writeString(cfg,"[scummvm]\nlastselectedgame=mygame\n[mygame]\ngameid=sotc\n");
  if(!SaveImport.target(cfg.toFile()).equals("mygame-sotc"))throw new AssertionError();
  System.out.println("PASS: PS4, gzip, zip, checksum/size rejection, replacement, backup and custom target. PS4 tick="+SaveImport.word(ps4,16));
 }
}
