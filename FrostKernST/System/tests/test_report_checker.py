import json
from pathlib import Path
import sys
import tempfile
import unittest
import subprocess
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from check_zigbee_reports import ReportChecker,read_capture

def telemetry(ieee='00124B000ABCDEF0',update=1000,valid=True,stale=False,age=0):
    return {'type':'TELEMETRY_TEMP','version':1,'node':1,'ieee':ieee,
            'master_update_ms':update,'age_ms':age,'temperature_valid':valid,
            'stale':stale,'report_seen':True,'temperature_c':25.0 if valid else None}

class CheckerTests(unittest.TestCase):
    def test_repeated_uart_is_not_multiple_zigbee_reports(self):
        c=ReportChecker()
        for _ in range(100):c.observe(telemetry())
        self.assertEqual(c.summary(1)['result'],'FAIL')
        self.assertEqual(c.peers['00124B000ABCDEF0']['new_reports'],0)
        c.observe(telemetry(update=61000));c.observe(telemetry(update=121000))
        self.assertEqual(c.summary(1)['result'],'PASS')
        self.assertEqual(c.summary(1)['peers'][0]['intervals_ms'],[60000,60000])

    def test_requires_two_distinct_slaves(self):
        c=ReportChecker()
        for u in (1000,61000,121000):c.observe(telemetry(update=u))
        self.assertEqual(c.summary(2)['result'],'FAIL')
        for u in (1000,61000,121000):c.observe(telemetry(ieee='00124B000ABCDEF1',update=u))
        self.assertEqual(c.summary(2)['result'],'PASS')

    def test_invalid_and_stale_readings(self):
        c=ReportChecker()
        for u in (1000,61000,121000):c.observe(telemetry(update=u,valid=False))
        self.assertEqual(c.summary(1)['result'],'FAIL')
        self.assertEqual(c.summary(1,require_valid=False)['result'],'PASS')
        c.observe(telemetry(update=181000,valid=False,stale=True,age=180010))
        self.assertEqual(c.summary(1,require_valid=False)['result'],'FAIL')

    def test_restart_and_clock_wrap(self):
        c=ReportChecker();c.observe({'type':'HEARTBEAT','uptime_ms':100000})
        for u in (1000,61000,121000):c.observe(telemetry(update=u))
        c.observe({'type':'HEARTBEAT','uptime_ms':100})
        self.assertEqual(c.summary(1)['result'],'FAIL');self.assertEqual(c.restarts,1)
        for u in (0xFFFFFF00,59744,119744):c.observe(telemetry(update=u))
        self.assertEqual(c.summary(1)['result'],'PASS')
        c.observe({'type':'HEARTBEAT','uptime_ms':0xFFFFFF00})
        before=c.restarts;c.observe({'type':'HEARTBEAT','uptime_ms':100})
        self.assertEqual(c.restarts,before)

    def test_live_transport_must_still_be_fresh(self):
        c=ReportChecker()
        for u,at in ((1000,0),(61000,60),(121000,120)):c.observe(telemetry(update=u),at)
        self.assertEqual(c.summary(1,now=121)['result'],'PASS')
        self.assertEqual(c.summary(1,now=126)['result'],'FAIL')

    def test_invalid_formats_and_utf16_capture(self):
        c=ReportChecker();f=telemetry();f['ieee']='bad';c.observe(f)
        self.assertFalse(c.peers);self.assertEqual(c.invalid_frames,1)
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)/'capture.jsonl';p.write_text(json.dumps(telemetry())+'\n',encoding='utf-16')
            self.assertEqual(list(read_capture(p))[0]['ieee'],'00124B000ABCDEF0')
            p.write_text('[]\n',encoding='utf-8')
            with self.assertRaises(ValueError):list(read_capture(p))

    def test_command_line_capture_pass_and_fail(self):
        script=Path(__file__).resolve().parents[1]/'tools/check_zigbee_reports.py'
        with tempfile.TemporaryDirectory() as t:
            p=Path(t)/'capture.jsonl'
            p.write_text(''.join(json.dumps(telemetry(update=u))+'\n' for u in (1000,61000,121000)))
            result=subprocess.run([sys.executable,str(script),'--jsonl',str(p),
                                   '--expected-slaves','1'],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('"result": "PASS"',result.stdout)
            p.write_text(''.join(json.dumps(telemetry())+'\n' for _ in range(10)))
            result=subprocess.run([sys.executable,str(script),'--jsonl',str(p),
                                   '--expected-slaves','1'],capture_output=True,text=True)
            self.assertEqual(result.returncode,1,result.stdout+result.stderr)
            self.assertIn('"result": "FAIL"',result.stdout)

if __name__=='__main__':unittest.main()
