#!/usr/bin/env python3
"""Real production callbacks and IPC with fake radio/HAL; not a board test."""
from pathlib import Path
import importlib.util,subprocess,tempfile,os
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('pipeline',ROOT/'tests/run_sensor_zigbee_tests.py')
pipeline=importlib.util.module_from_spec(spec);spec.loader.exec_module(pipeline)
def run(args):
    subprocess.run([str(x) for x in args],cwd=ROOT,check=True,
                   env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
def main():
    flags=['gcc','-std=c11','-g','-Wall','-Wextra','-Werror','-pedantic',
           '-fsanitize=address,undefined','-fno-omit-frame-pointer']
    with tempfile.TemporaryDirectory(prefix='frostkern-flow-rejoin-') as d:
        p=Path(d);source=(ROOT/'STM32_WPAN/App/app_zigbee.c').read_text()
        for label,name in [('parent','APP_ZIGBEE_ParentEvent'),('callback','APP_ZIGBEE_RejoinCallback'),
                           ('process','APP_ZIGBEE_ProcessSlaveRejoin'),('ready','APP_ZIGBEE_LinkReady')]:
            (p/f'rejoin_{label}.inc').write_text(pipeline.function(source,name))
        run(flags+['-DROLE_SLAVE=1','-Itests/integration/fakes','-Itests/integration',
                   '-ICore/Inc','-I'+str(p),'tests/integration/fake_rtos.c',
                   'Core/Src/app_ipc.c','tests/test_slave_rejoin.c','-o',p/'rejoin'])
        run([p/'rejoin'])
        run(flags+['-ICore/Inc','tests/test_flow_freshness.c','-o',p/'flow'])
        run([p/'flow'])
    with tempfile.TemporaryDirectory(prefix='frostkern-flow-sdk-') as d:
        p=Path(d);source=(ROOT/'STM32_WPAN/App/app_zigbee_flow.c').read_text()
        for label,name in [('callback','APP_ZIGBEE_FlowReport'),('now','APP_ZIGBEE_ReportRetryNowMs'),
                           ('config_callback','APP_ZIGBEE_FlowReportConfigCallback'),
                           ('send_config','APP_ZIGBEE_SendFlowReportConfig'),
                           ('endpoint','APP_ZIGBEE_ConfigFlowEndpoint'),('update','APP_ZIGBEE_UpdateFlow'),
                           ('configure','APP_ZIGBEE_ConfigureFlowReports'),('retries','APP_ZIGBEE_ProcessFlowReportRetries'),
                           ('recovery','APP_ZIGBEE_ProcessReportRecovery')]:
            (p/f'flow_{label}.inc').write_text(pipeline.function(source,name))
        for role in ['MASTER','SLAVE']:
            run(flags+['-DROLE_'+role+'=1','-Itests/integration/fakes','-Itests/integration',
                '-ICore/Inc','-I'+str(p),'tests/integration/fake_rtos.c','Core/Src/app_ipc.c',
                'Core/Src/app_zb_report_retry.c','tests/test_flow_sdk.c','-o',p/role])
            run([p/role])
    # Extended end-to-end test includes flow callback, Master dispatch, IPC,
    # actual UART encoder and independent decoder as well as temperature.
    pipeline.main()
if __name__=='__main__':main()
