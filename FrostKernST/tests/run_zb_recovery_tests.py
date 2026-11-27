#!/usr/bin/env python3
"""Host checks: state policy and production helper with real IPC, no physical RF."""
import importlib.util
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('pipeline', ROOT/'tests/run_sensor_zigbee_tests.py')
pipeline = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pipeline)

def run(args):
    subprocess.run([str(v) for v in args], cwd=ROOT, check=True,
                   env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0'})

def main():
    source = (ROOT/'STM32_WPAN/App/app_zigbee.c').read_text()
    task = pipeline.function(source, 'APP_ZIGBEE_NwkFormOrJoinTask')
    assert task.index('APP_ZIGBEE_ProcessRecovery();') < task.index('if (received != pdPASS)')
    flags = ['gcc','-std=c11','-Wall','-Wextra','-Werror','-pedantic','-g',
             '-fsanitize=address,undefined','-fno-omit-frame-pointer','-DROLE_MASTER=1']
    with tempfile.TemporaryDirectory(prefix='frostkern-recovery-') as directory:
        temp = Path(directory)
        (temp/'zigbee_recovery.inc').write_text(pipeline.function(source,'APP_ZIGBEE_ProcessRecovery'))
        (temp/'zigbee_public_permit.inc').write_text(pipeline.function(source,'APP_ZIGBEE_PermitJoin'))
        run(flags+['-ICore/Inc','tests/test_zb_recovery.c','-o',temp/'policy'])
        run([temp/'policy'])
        includes=['-Itests/integration/fakes','-ICore/Inc','-Itests/integration','-I'+str(temp)]
        run(flags+includes+['tests/integration/fake_rtos.c','Core/Src/app_ipc.c',
            'tests/integration/test_zb_recovery_ipc.c','-o',temp/'ipc'])
        run([temp/'ipc'])
        # Check the production task for both roles with SDK-facing functions stubbed.
        for role in ['MASTER','SLAVE']:
            stub = '''#include <stdint.h>
#include "app_ipc.h"
#include "app_zb_recovery.h"
#include "task.h"
#include "log_module.h"
#define UNUSED(x) ((void)(x))
#define osWaitForever UINT32_MAX
static AppZbRecovery joinRecovery;
static int NwkFormStartSemaphore;
static struct {void *pstZigbee;} stZigbeeAppInfo;
void osSemaphoreAcquire(int,uint32_t);
void APP_ZIGBEE_NwkFormOrJoin(void);
void APP_ZIGBEE_UpdateTemperature(void);
void APP_ZIGBEE_ProcessSlaveRejoin(void);
void APP_ZIGBEE_ServiceMeasurements(void);
void APP_ZIGBEE_UpdateFlow(void);
void APP_ZIGBEE_ProcessFlowReportRetries(void);
void APP_ZIGBEE_ProcessReportRecovery(void);
void APP_ZIGBEE_ConfigureFlowReports(uint8_t,uint16_t);
void APP_ZIGBEE_ProcessTemperatureReportRetries(void);
void APP_ZIGBEE_ProcessRecovery(void);
void AppZbPersistence_Process(void);
void APP_ZIGBEE_PermitJoin(uint8_t);
void APP_ZIGBEE_ConfigureTemperatureReports(uint8_t,uint16_t);
uint64_t ZbNwkAddrLookupExt(void *,uint16_t);
'''
            path=temp/('task_'+role+'.c')
            path.write_text(stub+task+'\nvoid RunTask(void) { APP_ZIGBEE_NwkFormOrJoinTask(0); }\n')
            run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-variable',
                 '-pedantic','-DROLE_'+role+'=1']+includes+['-c',path,'-o',temp/(role+'.o')])
    print('PASS Master/Slave task compile checks, including recovery before empty-queue continue.')

if __name__ == '__main__':
    main()
