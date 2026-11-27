#!/usr/bin/env python3
"""Host checks only; does not exercise the ST stack or physical radio."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('pipeline',ROOT/'tests/run_sensor_zigbee_tests.py')
pipeline=importlib.util.module_from_spec(spec);spec.loader.exec_module(pipeline)
def run(args):
    subprocess.run([str(x) for x in args],cwd=ROOT,check=True)
def main():
    endpoint=(ROOT/'STM32_WPAN/App/app_zigbee_endpoint.c').read_text()
    task=(ROOT/'STM32_WPAN/App/app_zigbee.c').read_text()
    with tempfile.TemporaryDirectory(prefix='frostkern-diag-') as directory:
        temp=Path(directory)
        for name,fn in [('update','APP_ZIGBEE_UpdateTemperature'),
                        ('confirm','APP_ZIGBEE_ReportSentDiagnostic')]:
            (temp/f'zigbee_diagnostic_{name}.inc').write_text(pipeline.function(endpoint,fn))
        flags=['gcc','-std=c11','-Wall','-Wextra','-Werror','-pedantic','-g']
        run(flags+['-ICore/Inc','-I'+str(temp),'Core/Src/app_temperature_report.c',
                   'tests/test_zigbee_diagnostics.c','-o',temp/'diag'])
        run([temp/'diag'])
        flow=(ROOT/'STM32_WPAN/App/app_zigbee_flow.c').read_text()
        (temp/'zigbee_flow_callback.inc').write_text(pipeline.function(flow,'APP_ZIGBEE_FlowReport'))
        (temp/'zigbee_report_callback.inc').write_text(pipeline.function(endpoint,'APP_ZIGBEE_TemperatureReport'))
        start=task.index('{',task.index('command.type == APP_ZB_COMMAND_TEMP_RECEIVED'))
        depth=0
        for end in range(start,len(task)):
            if task[end]=='{':depth+=1
            elif task[end]=='}':
                depth-=1
                if depth==0:break
        (temp/'zigbee_master_dispatch.inc').write_text(
            'static void Integration_DispatchTemperature(AppZbCommand command)\n'+task[start:end+1]+'\n')
        flow_start=task.index('{',task.index('command.type == APP_ZB_COMMAND_FLOW_RECEIVED'))
        depth=0
        for flow_end in range(flow_start,len(task)):
            if task[flow_end]=='{':depth+=1
            elif task[flow_end]=='}':
                depth-=1
                if depth==0:break
        join_start=task.index('{',task.index('command.type == APP_ZB_COMMAND_CONFIG_TEMP_REPORT'))
        depth=0
        for join_end in range(join_start,len(task)):
            if task[join_end]=='{':depth+=1
            elif task[join_end]=='}':
                depth-=1
                if depth==0:break
        (temp/'zigbee_join_dispatch.inc').write_text(
            'static void Integration_DispatchJoin(AppZbCommand command)\n'+task[join_start:join_end+1]+'\n')
        (temp/'zigbee_flow_dispatch.inc').write_text(
            'static void Integration_DispatchFlow(AppZbCommand command)\n'+task[flow_start:flow_end+1]+'\n')
        includes=['-Itests/integration/fakes','-ICore/Inc','-Itests/integration','-I'+str(temp)]
        run(flags+includes+['-DROLE_MASTER=1','-DAPP_ZIGBEE_REPORT_DIAGNOSTICS=1',
            'tests/integration/fake_rtos.c','Core/Src/app_ipc.c','Core/Src/app_uart_protocol.c',
            'tests/integration/test_master_reports_uart.c','-o',temp/'master'])
        run([temp/'master','2499',temp/'frames.bin'])
        print('PASS diagnostics-enabled actual Master callback -> real IPC -> UART integration checks.')
if __name__=='__main__':main()
