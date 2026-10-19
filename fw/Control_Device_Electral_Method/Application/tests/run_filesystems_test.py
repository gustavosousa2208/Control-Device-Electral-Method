from pathlib import Path
import subprocess,tempfile,shutil,re,sys
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='fatfs-real-exfat-') as tmp:
 stage=Path(tmp)
 conf=(root/'FATFS/Target/ffconf.h').read_text()
 conf=re.sub(r'^#include "(?:main|stm32f4xx_hal|bsp_driver_sd)\.h"\s*$', '',conf,flags=re.M)
 (stage/'ffconf.h').write_text(conf)
 for p in (root/'Middlewares/Third_Party/FatFs/src').glob('*'):
  if p.name in ['ff.c','ff.h','ff_unicode.c','diskio.h','integer.h']:shutil.copy2(p,stage/p.name)
 exe=stage/'roundtrip.exe'
 subprocess.run(['gcc','-std=c11','-O2','-I'+str(stage),str(Path(__file__).with_name('fatfs_filesystem_roundtrip_test.c')),str(stage/'ff.c'),str(stage/'ff_unicode.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe),str(stage/'test-image.img')],check=True)
