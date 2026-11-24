#!/usr/bin/env python3
"""Verify observed Zigbee temperature updates through the Master UART v1 stream."""
import argparse
import json
import math
from pathlib import Path
import re
import time
from uart_probe import describe, encode, extract

MASK=0xFFFFFFFF
HALF=0x80000000

class ReportChecker:
    def __init__(self):
        self.peers={}
        self.last_uptime=None
        self.restarts=0
        self.heartbeat_count=0
        self.last_reception=None
        self.invalid_frames=0

    def observe(self,frame,at=None):
        if at is not None:self.last_reception=at
        kind=frame.get('type')
        if kind=='HEARTBEAT':
            self.heartbeat_count+=1
            uptime=frame.get('uptime_ms')
            if isinstance(uptime,int):
                if self.last_uptime is not None and ((uptime-self.last_uptime)&MASK)>=HALF:
                    self.peers.clear();self.restarts+=1
                self.last_uptime=uptime
            return None
        if kind!='TELEMETRY_TEMP':return None
        ieee=str(frame.get('ieee','')).upper()
        node=frame.get('node')
        update=frame.get('master_update_ms')
        age=frame.get('age_ms')
        if (frame.get('version')!=1 or not re.fullmatch(r'[0-9A-F]{16}',ieee)
                or ieee in ('0000000000000000','FFFFFFFFFFFFFFFF')
                or node not in (1,2) or not isinstance(update,int)
                or not 0<=update<=MASK or not isinstance(age,int) or not 0<=age<=MASK):
            self.invalid_frames+=1;return None
        peer=self.peers.setdefault(ieee,{
            'ieee':ieee,'node':node,'new_reports':0,'last_update_ms':None,
            'intervals_ms':[],'temperature_valid':False,'stale':False,
            'report_seen':False,'temperature_c':None,'age_ms':MASK,
            'last_uart_at':None,'uart_frames':0})
        peer['node']=node;peer['uart_frames']+=1;peer['last_uart_at']=at
        value=frame.get('temperature_c')
        valid=(bool(frame.get('temperature_valid')) and
               isinstance(value,(int,float)) and math.isfinite(value) and age<=180000)
        peer.update(temperature_valid=valid,stale=bool(frame.get('stale')),
                    report_seen=bool(frame.get('report_seen')),
                    temperature_c=value if valid else None,age_ms=age)
        if not peer['report_seen']:return None
        previous=peer['last_update_ms']
        peer['last_update_ms']=update
        if previous is None:
            return f"IEEE {ieee}: valor inicial observado; aun no cuenta como reporte nuevo."
        delta=(update-previous)&MASK
        if delta==0:return None # Repeated UART snapshot, not a new Zigbee report.
        if delta>=HALF:
            peer['new_reports']=0;peer['intervals_ms']=[]
            return f"IEEE {ieee}: reloj cambio hacia atras; reinicio la cuenta de reportes."
        peer['new_reports']+=1;peer['intervals_ms'].append(delta)
        label=f"{peer['temperature_c']:.2f} C" if valid else 'temperatura no valida'
        return (f"IEEE {ieee}, Slave {node}: reporte nuevo {peer['new_reports']}, "
                f"intervalo {delta/1000:.2f} s, {label}")

    def summary(self,expected_slaves=2,min_updates=2,require_valid=True,now=None):
        peers=[]
        for peer in self.peers.values():
            reasons=[]
            if peer['new_reports']<min_updates:reasons.append('faltan reportes nuevos')
            if not peer['report_seen']:reasons.append('sin reporte Zigbee observado')
            if peer['stale'] or peer['age_ms']>180000:reasons.append('reporte antiguo')
            if require_valid and not peer['temperature_valid']:reasons.append('temperatura no valida')
            if now is not None and (peer['last_uart_at'] is None or now-peer['last_uart_at']>5):
                reasons.append('sin telemetria UART reciente de este Slave')
            data={k:v for k,v in peer.items() if k!='last_uart_at'}
            data['passes']=not reasons;data['reasons']=reasons;peers.append(data)
        silence=None if now is None or self.last_reception is None else now-self.last_reception
        uart_live=now is None or (silence is not None and silence<=5)
        passing=sum(p['passes'] for p in peers)
        return {'result':'PASS' if passing>=expected_slaves and uart_live else 'FAIL',
                'expected_slaves':expected_slaves,'passing_slaves':passing,
                'required_new_reports_after_baseline':min_updates,
                'heartbeat_count':self.heartbeat_count,'master_restarts_seen':self.restarts,
                'malformed_telemetry_frames':self.invalid_frames,
                'uart_silence_seconds':silence,'peers':peers}

def read_capture(path):
    raw=Path(path).read_bytes()
    text=raw.decode('utf-16') if raw.startswith((b'\xff\xfe',b'\xfe\xff')) else raw.decode('utf-8-sig')
    for number,line in enumerate(text.splitlines(),1):
        if not line.strip():continue
        try:
            value=json.loads(line)
            if not isinstance(value,dict):raise ValueError(f'La linea {number} no contiene un objeto JSON')
            yield value
        except json.JSONDecodeError as error:raise ValueError(f'JSON invalido en linea {number}: {error}') from error

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    source=parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--jsonl',help='captura JSONL generada por uart_probe.py o --save-jsonl')
    source.add_argument('--port',help='puerto serie con los bytes RAW de USART2, por ejemplo COM7')
    parser.add_argument('--seconds',type=float,default=240)
    parser.add_argument('--expected-slaves',type=int,choices=(1,2),default=2)
    parser.add_argument('--min-updates',type=int,default=2)
    parser.add_argument('--allow-invalid-temp',action='store_true',help='comprobar llegada de reportes, sin exigir temperatura valida')
    parser.add_argument('--permit-join',type=int,help='solicitar una ventana de altas de 0..254 s al observar network_started')
    parser.add_argument('--save-jsonl',help='guardar tramas decodificadas de la prueba en vivo')
    args=parser.parse_args()
    if args.min_updates<1 or args.seconds<=0:parser.error('min-updates y seconds deben ser positivos')
    if args.permit_join is not None and not 0<=args.permit_join<=254:parser.error('permit-join debe ser 0..254')
    if args.jsonl and (args.save_jsonl or args.permit_join is not None):parser.error('save-jsonl y permit-join requieren --port')
    checker=ReportChecker();save=None;now=None
    try:
        if args.jsonl:
            for frame in read_capture(args.jsonl):
                message=checker.observe(frame)
                if message:print(message)
        else:
            try:import serial
            except ImportError:parser.error('Instala pyserial: python -m pip install pyserial')
            if args.save_jsonl:save=open(args.save_jsonl,'w',encoding='utf-8')
            buffer=bytearray();start=time.monotonic();last_ping=start-2;last_rx=start
            sequence=0;permit_sent=False
            with serial.Serial(args.port,115200,timeout=0.05) as port:
                while time.monotonic()-start<args.seconds:
                    now=time.monotonic()
                    if now-last_ping>=2:
                        port.write(encode(2,sequence));sequence=(sequence+1)&255;last_ping=now
                    data=port.read(256)
                    if not data:continue
                    if time.monotonic()-last_rx>=0.1:buffer.clear()
                    last_rx=time.monotonic();buffer.extend(data)
                    for raw in extract(buffer):
                        frame=describe(raw)
                        if save:
                            save.write(json.dumps(frame)+'\n');save.flush()
                        message=checker.observe(frame,time.monotonic())
                        if message:print(message,flush=True)
                        if (args.permit_join is not None and not permit_sent and
                            frame.get('type')=='HEARTBEAT' and frame.get('network_started')):
                            port.write(encode(0x20,sequence,bytes((args.permit_join,))))
                            sequence=(sequence+1)&255;permit_sent=True
                            print('PERMIT_JOIN solicitado; su ACK solo confirma encolado.',flush=True)
                    if len(buffer)>70:del buffer[:-70]
            now=time.monotonic()
    except (OSError,ValueError) as error:parser.exit(2,str(error)+'\n')
    finally:
        if save:save.close()
    result=checker.summary(args.expected_slaves,args.min_updates,not args.allow_invalid_temp,now)
    print(json.dumps(result,indent=2,ensure_ascii=False))
    print('PASS comprueba las tramas observadas. Las pruebas de PC con datos simulados no prueban radio real.')
    raise SystemExit(0 if result['result']=='PASS' else 1)

if __name__=='__main__':main()
