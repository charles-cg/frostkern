from pathlib import Path
import subprocess,tempfile,os
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
 for name,sources,flags in [
  ('master',['tests/test_master_control.c','Core/Src/app_ipc.c','Core/Src/app_fan_policy.c','tests/integration/fake_rtos.c'],['-DROLE_MASTER=1','-Itests/integration/fakes','-Itests/integration']),
  ('pwm',['tests/test_fan_pwm.c'],['-DROLE_MASTER=1','-Itests/pwm_fakes']),
  ('ds_bus',['tests/test_ds18b20_bus.c','Core/Src/ds18b20_decode.c'],['-DROLE_MASTER=1','-Itests/ds18_fakes']),
  ('ds18',['tests/test_ds18b20_decode.c','Core/Src/ds18b20_decode.c'],[])]:
  exe=Path(d)/name
  subprocess.run(['gcc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',]+flags+['-ICore/Inc']+sources+['-o',str(exe)],cwd=r,check=True)
  subprocess.run([str(exe)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
