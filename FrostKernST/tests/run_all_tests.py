#!/usr/bin/env python3
"""Host regression suite; no STM32/ESP32 radio or hardware is exercised."""
from pathlib import Path
import subprocess,sys,tempfile,os
ROOT=Path(__file__).resolve().parents[1]
def run(args):
    subprocess.run([str(x) for x in args],cwd=ROOT,check=True,
                   env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
def main():
    for runner in ['run_master_control_tests.py','run_reconnect_fresh_sample_tests.py','run_flow_rejoin_tests.py','run_persistence_tests.py',
                   'run_zb_recovery_tests.py','run_zigbee_diagnostics_tests.py']:
        run([sys.executable,ROOT/'tests'/runner])
    flags=['gcc','-std=c11','-Wall','-Wextra','-Werror','-pedantic',
           '-fsanitize=address,undefined','-fno-omit-frame-pointer']
    with tempfile.TemporaryDirectory(prefix='frostkern-unit-') as d:
        p=Path(d)
        for label,sources,extra in [
            ('codec',['Core/Src/app_uart_protocol.c','tests/test_uart_protocol.c'],[]),
            ('retry',['Core/Src/app_zb_report_retry.c','tests/test_zb_report_retry.c'],[]),
            ('bridge',['tests/test_uart_bridge.c','Core/Src/app_uart_protocol.c'],
             ['-DROLE_MASTER=1','-Itests/fakes'])]:
            run(flags+extra+['-ICore/Inc']+sources+['-o',p/label]);run([p/label])
    print('PASS all host regressions. Run ESP32/tests/run.sh separately; board tests still required.')
if __name__=='__main__':main()
