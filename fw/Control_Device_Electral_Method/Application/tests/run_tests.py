"""Host regression and HAL-shim checks. Requires gcc on PATH; no target build."""
from pathlib import Path
import subprocess
import tempfile
import sys
root = Path(__file__).resolve().parents[1]
includes = ['board', 'core', 'drivers', 'ui', 'interfaces', '.']
common = ['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror']
common += ['-I' + str(root / p) for p in includes]
cases = {
    'contract': ['tests/contract_check.c'],
    'job': ['tests/job_test.c', 'core/job.c','core/targets.c','core/linfit.c','core/format.c'],
    'power_max31855': ['tests/power_max31855_test.c', 'drivers/power.c', 'drivers/max31855.c'],
    'board': ['tests/board_integration_test.c', 'board/board.c', 'drivers/power.c'],
    'board_enabled': ['tests/board_integration_test.c', 'board/board.c', 'drivers/power.c'],
    'dwt': ['tests/dwt_test.c', 'board/dwt_us.c'],
    'math': ['tests/math_test.c','core/targets.c','core/linfit.c','core/vf_math.c','core/format.c'],
    'ui': ['tests/ui_driver_test.c','ui/ui.c','ui/assets.c','drivers/oled.c','drivers/encoder.c','core/format.c'],
    'adc': ['tests/adc_test.c','drivers/vf_adc.c','core/vf_math.c'],
    'sdlog': ['tests/sdlog_test.c','drivers/sdlog.c','core/format.c'],
    'sd_board': ['tests/sd_board_test.c','board/sd_io.c'],
    'app': ['tests/app_test.c','app.c','ui/ui.c','ui/assets.c',
        'drivers/oled.c','drivers/encoder.c','drivers/vf_adc.c','drivers/max31855.c',
        'drivers/rtd.c','drivers/power.c','drivers/sdlog.c','core/job.c',
        'core/targets.c','core/linfit.c','core/vf_math.c','core/format.c','core/calibration.c','core/sofia.c'],
    'require_cal': ['tests/require_cal_test.c','core/job.c','core/targets.c',
        'core/linfit.c','core/format.c','ui/ui.c','ui/assets.c'],
}
cases['tsp_slow_sd']=['tests/sofia_slow_sd_test.c','core/sofia.c','drivers/vf_adc.c','core/format.c','core/vf_math.c']
cases['sofia']=['tests/sofia_test.c','core/sofia.c','core/format.c','core/vf_math.c']
cases['sofia_io']=['tests/sofia_io_test.c','board/sofia_io.c']
cases['adc_burst_adapter'] = ['tests/adc_burst_adapter_test.c']
cases['tc_heater'] = ['tests/tc_heater_test.c']
cases['tc_heat_app'] = ['tests/tc_heat_app_test.c','app.c','ui/ui.c','ui/assets.c','core/format.c']
cases['manual_k_job'] = ['tests/manual_k_job_test.c','core/job.c','core/targets.c','core/linfit.c','core/format.c']
cases['sd_crc_retry'] = ['tests/sd_crc_retry_test.c']
cases['tc_menu_app'] = ['tests/tc_menu_app_test.c','app.c','ui/ui.c','ui/assets.c','core/format.c']
cases['tc_calibration'] = ['tests/tc_calibration_test.c']
cases['oled_boot'] = ['tests/oled_boot_test.c','drivers/oled.c']
cases['sd_board_1bit'] = cases['sd_board']
cases['input_temperature'] = ['tests/input_temperature_test.c','drivers/encoder.c','drivers/max31855.c']
cases['oled_profiles'] = ['tests/oled_profiles_test.c','drivers/oled.c']
cases['ui_sh1106'] = cases['ui']
with tempfile.TemporaryDirectory(prefix='thermal-tests-') as temp:
    selected = sys.argv[1:] or list(cases)
    for name in selected:
        sources = cases[name] + (['core/tc_heater.c'] if name in ('app','tc_menu_app','tc_heater','tc_heat_app') else []) + ['drivers/hw_debug.c','core/tc_calibration.c','core/thermal_tsp.c']
        cmd = common.copy()
        if name in ('board', 'board_enabled', 'dwt', 'sdlog', 'sd_board', 'sd_board_1bit','adc_burst_adapter','sofia_io'):
            cmd += ['-I' + str(root / 'tests/stubs')]
        if name == 'adc_burst_adapter':
            adapter=(root/'board/peripheral_io.c').read_text()
            start=adapter.index('bool board_adc_burst(')
            end=adapter.index('\n#include "hw_debug.h"',start)
            (Path(temp)/'adc_burst_under_test.inc').write_text(adapter[start:end])
            cmd+=['-I'+temp]
        if name == 'sd_crc_retry':
            bsp = (root.parent/'FATFS/Target/bsp_driver_sd.c').read_text()
            start = bsp.index('__weak uint8_t BSP_SD_ReadBlocks(')
            end = bsp.index('/* USER CODE BEGIN BeforeWriteBlocksSection */', start)
            function = bsp[start:end].strip()
            function = function[:function.rfind('}')+1]
            (Path(temp)/'sd_crc_read_under_test.inc').write_text(function)
            cmd += ['-I'+temp]
        if name == 'sd_board_1bit':
            cmd += ['-DTHERMAL_SD_BUS_BITS=1']
        if name == 'ui_sh1106':
            cmd += ['-DTEST_OLED_CONTROLLER=2']
        if name == 'job':
            cmd += ['-DCAL_WAIT_TIMEOUT_S=20','-DTEST_TARGET_TIMEOUT_S=20']
        if name == 'sdlog':
            cmd += ['-DTHERMAL_USE_FATFS=1']
        if name == 'app':
            cmd += ['-DTHERMAL_USE_FATFS=0']
        if name in ('require_cal','manual_k_job'):
            cmd += ['-DREQUIRE_CAL=1']
        if name == 'board':
            cmd += ['-DBOARD_POWER_CONTROL_ENABLED=0']
        if name == 'board_enabled':
            cmd += ['-DBOARD_POWER_CONTROL_ENABLED=1']
        exe = Path(temp) / (name + '.exe')
        subprocess.run(cmd + [str(root / p) for p in sources] + ['-lm', '-o', str(exe)], check=True)
        if name == 'ui':
            preview=Path(temp)/'preview';preview.mkdir(exist_ok=True)
            subprocess.run([str(exe),str(preview)],check=True)
        else:
            subprocess.run([str(exe)], check=True)
        print(name + ': PASS', flush=True)
    # Real HAL adapter is compiled against the shim, never called with fake MCU addresses.
    subprocess.run(common + ['-I'+str(root/'tests/stubs'),'-c',
        str(root/'board/peripheral_io.c'),'-o',str(Path(temp)/'peripheral_io.o')],check=True)
    subprocess.run(common + ['-I'+str(root/'tests/stubs'),'-c', str(root/'board/sofia_io.c'),'-o',str(Path(temp)/'sofia_io.o')],check=True)
    print('HAL peripheral adapter syntax: PASS',flush=True)

# Real filesystem integration: temporary images only, never physical cards.
import sys
subprocess.run([sys.executable,str(root/"tests/run_filesystems_test.py")],check=True)

subprocess.run([sys.executable,str(root/'tests/sd_polling_complete_test.py')],check=True)

subprocess.run([sys.executable,str(root/'tests/sd_write_test.py')],check=True)
print('All host checks passed. Hardware validation is separate.',flush=True)
