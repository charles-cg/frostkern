#!/usr/bin/env python3
"""Host fault-injection tests; SDK, HAL and radio require separate board validation."""
from pathlib import Path
import os,subprocess,tempfile,importlib.util
ROOT=Path(__file__).resolve().parents[1]
def run(args):
    subprocess.run([str(x) for x in args],cwd=ROOT,check=True,
                   env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
def main():
    flags=['gcc','-std=c11','-g','-Wall','-Wextra','-Werror','-pedantic',
           '-fsanitize=address,undefined','-fno-omit-frame-pointer']
    with tempfile.TemporaryDirectory(prefix='frostkern-persist-') as d:
        p=Path(d)
        spec=importlib.util.spec_from_file_location('pipeline',ROOT/'tests/run_sensor_zigbee_tests.py')
        pipeline=importlib.util.module_from_spec(spec);spec.loader.exec_module(pipeline)
        source=(ROOT/'STM32_WPAN/App/app_zigbee.c').read_text()
        (p/'persistence_startup.inc').write_text(pipeline.function(source,'APP_ZIGBEE_NwkFormOrJoin'))
        (p/'persistence_wait.inc').write_text(pipeline.function(source,'APP_ZIGBEE_WaitForCallback'))
        run(flags+['-I'+str(p),'tests/test_persistence_wait.c','-o',p/'wait'])
        run([p/'wait'])
        run(flags+['-ICore/Inc','Core/Src/app_persist_store.c','tests/test_persist_store.c','-o',p/'journal'])
        run([p/'journal'])
        for role in ['MASTER','SLAVE']:
            run(flags+['-DROLE_'+role+'=1','-Itests/integration/fakes','-Itests/integration','-ICore/Inc',
                'Core/Src/app_persist_store.c','Core/Src/app_zb_persistence.c',
                'Core/Src/app_ipc.c','tests/integration/fake_rtos.c',
                'tests/integration/test_zb_persistence.c','-o',p/role])
            run([p/role])
            run(flags+['-DROLE_'+role+'=1','-Itests/integration/fakes','-ICore/Inc','-I'+str(p),
                       'tests/test_persistence_startup.c','-o',p/(role+'_startup')])
            run([p/(role+'_startup')])
    print('PASS persistence journal and Master/Slave integration with fake Flash/SDK.')
if __name__=='__main__':main()
