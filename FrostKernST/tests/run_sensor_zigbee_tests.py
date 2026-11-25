#!/usr/bin/env python3
"""Run PC integration tests with fake HAL/SDK; never a radio/hardware test."""
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]

def function(source,name):
    for match in re.finditer(r'\b'+re.escape(name)+r'\s*\(',source):
        end=source.index(')',match.end())+1
        while source[end].isspace(): end+=1
        if source[end]!='{': continue
        start=source.rfind('\n',0,match.start())+1
        # Signatures here start on a single line, followed by multiline params.
        depth=0
        for pos in range(end,len(source)):
            if source[pos]=='{':depth+=1
            elif source[pos]=='}':
                depth-=1
                if depth==0:return source[start:pos+1]+'\n'
    raise AssertionError('function not found: '+name)

def run(command):
    result=subprocess.run([str(x) for x in command],cwd=ROOT,text=True,
                          stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                          env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
    if result.returncode:raise RuntimeError(result.stdout)
    if result.stdout:print(result.stdout.strip())
    return result.stdout

def main():
    with tempfile.TemporaryDirectory(prefix='frostkern-test-') as directory:
        temp=Path(directory)
        endpoint=(ROOT/'STM32_WPAN/App/app_zigbee_endpoint.c').read_text()
        (temp/'zigbee_temperature_update.inc').write_text(function(endpoint,'APP_ZIGBEE_UpdateTemperature'))
        flow=(ROOT/'STM32_WPAN/App/app_zigbee_flow.c').read_text()
        (temp/'zigbee_flow_callback.inc').write_text(function(flow,'APP_ZIGBEE_FlowReport'))
        (temp/'zigbee_report_callback.inc').write_text(function(endpoint,'APP_ZIGBEE_TemperatureReport'))
        task=(ROOT/'STM32_WPAN/App/app_zigbee.c').read_text()
        marker='command.type == APP_ZB_COMMAND_TEMP_RECEIVED'
        start=task.index('{',task.index(marker));depth=0
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
        (temp/'zigbee_flow_dispatch.inc').write_text(
            'static void Integration_DispatchFlow(AppZbCommand command)\n'+task[flow_start:flow_end+1]+'\n')
        flags=['gcc','-std=c11','-Wall','-Wextra','-Werror','-pedantic','-g']
        if os.environ.get('FROSTKERN_SANITIZE')=='1':
            flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
        includes=['-Itests/integration/fakes','-ICore/Inc','-Itests/integration','-I'+str(temp)]
        common=['tests/integration/fake_rtos.c','Core/Src/app_ipc.c']
        run(flags+includes+['-DROLE_SLAVE=1']+common+[
            'Core/Src/app_temperature_report.c','tests/integration/test_sensor_ipc.c',
            '-lm','-o',temp/'sensor'])
        output=run([temp/'sensor'])
        measured=int(re.search(r'temperature_centi_c=(-?\d+)',output).group(1))
        run(flags+includes+['-DROLE_MASTER=1']+common+[
            'Core/Src/app_uart_protocol.c','tests/integration/test_master_reports_uart.c',
            '-o',temp/'master'])
        capture=temp/'frames.bin'
        run([temp/'master',str(measured),capture])
        run(flags+['-ICore/Inc','Core/Src/app_temperature_report.c',
                   'tests/integration/test_temperature_report.c','-o',temp/'freshness'])
        run([temp/'freshness'])
        spec=importlib.util.spec_from_file_location('uart_probe',ROOT/'tools/uart_probe.py')
        probe=importlib.util.module_from_spec(spec);spec.loader.exec_module(probe)
        frames=list(probe.extract(bytearray(capture.read_bytes())))
        data=[probe.describe(f) for f in frames if f[0]==0x10]
        assert data and data[0]['temperature_c']==measured/100
        assert data[0]['ieee']=='00124B000ABCDEF0'
        assert any(d['ieee']=='00124B000ABCDEF1' for d in data)
        assert any(not d['temperature_valid'] for d in data)
        flow=[probe.describe(f) for f in frames if f[0]==0x12]
        assert any(d['flow_valid'] and d['flow_ml_min']==7500 for d in flow)
        assert any(d['flow_valid'] and d['flow_ml_min']==0 for d in flow)
        assert any(not d['flow_valid'] and d['stale'] for d in flow)
        assert any(d['ieee']=='00124B000ABCDEF1' and d['flow_valid'] for d in flow)
        print('PIPELINE_PC_PASS: original ADC sample -> real shared snapshot -> ZCL attribute; report callback -> real Master IPC -> actual UART encoder -> independent Python decoder.')
        print('This result uses fake HAL/SDK/RTOS. Live RF reports remain a separate board test.')

if __name__=='__main__':main()
