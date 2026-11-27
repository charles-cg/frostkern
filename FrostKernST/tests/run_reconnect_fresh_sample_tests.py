#!/usr/bin/env python3
"""Fresh-reconnect production path against fake HAL/radio; not a PCB test."""
from pathlib import Path
import importlib.util,subprocess,tempfile,os
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('pipeline',ROOT/'tests/run_sensor_zigbee_tests.py')
pipeline=importlib.util.module_from_spec(spec);spec.loader.exec_module(pipeline)
def main():
    source=(ROOT/'STM32_WPAN/App/app_zigbee_refresh.c').read_text()
    endpoint=(ROOT/'STM32_WPAN/App/app_zigbee_endpoint.c').read_text()
    flow=(ROOT/'STM32_WPAN/App/app_zigbee_flow.c').read_text()
    with tempfile.TemporaryDirectory(prefix='frostkern-fresh-') as d:
        p=Path(d)
        for label,name in [('request_sample','RequestSample'),('invalidate','InvalidateAttributes'),
                           ('request_reports','APP_ZIGBEE_RequestFreshReports'),
                           ('cancel','APP_ZIGBEE_CancelFreshReports'),
                           ('service','APP_ZIGBEE_ServiceMeasurements')]:
            (p/f'fresh_{label}.inc').write_text(pipeline.function(source,name))
        (p/'fresh_temperature.inc').write_text(pipeline.function(endpoint,'APP_ZIGBEE_UpdateTemperature'))
        (p/'fresh_flow.inc').write_text(pipeline.function(flow,'APP_ZIGBEE_UpdateFlow'))
        flags=['gcc','-std=c11','-Wall','-Wextra','-Werror','-pedantic','-g',
               '-fsanitize=address,undefined','-fno-omit-frame-pointer','-DROLE_SLAVE=1',
               '-Itests/integration/fakes','-Itests/integration','-ICore/Inc','-I'+str(p)]
        pipeline.run(flags+['tests/integration/fake_rtos.c','Core/Src/app_ipc.c',
            'Core/Src/app_temperature_report.c','tests/test_reconnect_fresh_sample.c',
            '-lm','-o',p/'fresh'])
        pipeline.run([p/'fresh'])
if __name__=='__main__':main()
