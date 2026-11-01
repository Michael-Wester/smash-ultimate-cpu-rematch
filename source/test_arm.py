#!/usr/bin/env python3
"""Run the actual compiled NRO in Unicorn AArch64 with a simulated Switch runtime.

No game code/assets are used. Game signatures reproduce the uploaded log's layout;
SDK functions are mocks. This verifies compiled ABI/control flow, not live Eden.
SPDX-License-Identifier: AGPL-3.0-or-later
"""
import argparse
from pathlib import Path
import struct
import sys
import time

from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE, UC_HOOK_INTR
from unicorn.arm64_const import *

def u32(b, offset=0): return struct.unpack_from('<I', b, offset)[0]
def u64(b, offset=0): return struct.unpack_from('<Q', b, offset)[0]
def page(n): return (n+0xfff)&~0xfff

class Runtime:
    PLUGIN=0x10000000
    SDK=0x20000000
    STACK=0x30000000
    GAME=0x807c9000
    HEAP=0x90000000
    FRAME=0x135b810
    MODE=0x1743870
    RETRY=0x100000
    ROOT=0x52c3760
    RO=0x3398000
    DATA=0x4f17000
    STOP=SDK+0xff000
    def __init__(self, path, sub_width=8, vtable_data=False, game_base=None, plugin_base=None):
        if game_base is not None:self.GAME=game_base
        if plugin_base is not None:self.PLUGIN=plugin_base
        self.uc=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
        self.uc.reg_write(UC_ARM64_REG_CPACR_EL1,3<<20)
        self.nro=Path(path).read_bytes()
        assert self.nro[16:20]==b'NRO0'
        self.plugin_size=len(self.nro)+u32(self.nro,0x38)
        self.uc.mem_map(self.PLUGIN,page(self.plugin_size))
        self.uc.mem_write(self.PLUGIN,self.nro)
        self.uc.mem_map(self.SDK,0x100000)
        self.uc.mem_map(self.STACK,0x100000)
        self.uc.mem_map(self.GAME,0x6000000)
        self.uc.mem_map(self.HEAP,0x100000)
        self.uc.reg_write(UC_ARM64_REG_SP,self.STACK+0xff000)
        self.stubs={}; self.names={}; self.logs=[]; self.files={}
        self.hook=None; self.retry_calls=0; self.original_calls=0
        self.buttons=0; self.controller_id=0; self.style=1
        self.connected=True; self.ready=True; self.results=False
        self.count=2; self.cpus={1}; self.main_mode=1; self.sub_mode=0
        self.version='13.0.3'; self.sub_width=sub_width
        self.vtable_data=vtable_data
        self.native_log=True
        self.manager_slot=self.GAME+0x52c5000
        self.manager=self.HEAP+0x1000
        self.state=self.HEAP+0x4000
        self.vtable=self.GAME+(self.DATA+0x2000 if vtable_data else self.RO+0x2000)
        self.write64(self.manager_slot,self.manager)
        self.restore_chain()
        self.write64(self.state,self.vtable)
        self.write64(self.vtable+40,self.GAME+self.RETRY)
        self.uc.mem_write(self.GAME+self.RETRY,struct.pack('<I',0xd65f03c0))
        self.install_mode()
        frame=[0xd101c3ff,0xf9000bfb,0xa90267fa,0xa9035ff8,0xa90457f6,0xa9054ff4,0xa9067bfd,0x910183fd,0xf940900b]
        mode=[0xf85103a8,0xf9400108,0xb89483a2,0x8b140100]
        self.uc.mem_write(self.GAME+self.FRAME,struct.pack('<9I',*frame))
        self.uc.mem_write(self.GAME+0x1740584,struct.pack('<4I',*mode))
        self.uc.mem_write(self.GAME+0x1740580,struct.pack('<I',0x94000cbc))
        self.register_sdk()
        self.relocate()
        self.uc.hook_add(UC_HOOK_CODE,self.sdk_hook,begin=self.SDK,end=self.SDK+0xfffff)
        self.uc.hook_add(UC_HOOK_CODE,self.retry_hook,begin=self.GAME+self.RETRY,end=self.GAME+self.RETRY)
        self.uc.hook_add(UC_HOOK_INTR,self.svc_hook)

    def write32(self,p,n): self.uc.mem_write(p,struct.pack('<I',n&0xffffffff))
    def write64(self,p,n): self.uc.mem_write(p,struct.pack('<Q',n&0xffffffffffffffff))
    def read32(self,p): return u32(self.uc.mem_read(p,4))
    def read64(self,p): return u64(self.uc.mem_read(p,8))
    def string(self,p):
        out=bytearray()
        while True:
            c=self.uc.mem_read(p+len(out),1)[0]
            if not c: return out.decode()
            out.append(c)
            assert len(out)<4096
    def reg(self,n): return self.uc.reg_read(UC_ARM64_REG_X0+n)
    def result(self,n): self.uc.reg_write(UC_ARM64_REG_X0,n)
    def restore_chain(self):
        self.write64(self.GAME+self.ROOT,self.HEAP+0x2000)
        self.write64(self.HEAP+0x2000,self.HEAP+0x3000)
        self.write64(self.HEAP+0x3000,self.state)
    def install_mode(self):
        # Execute real ARM stores to reproduce the output-buffer overwrite.
        store=0xf900003f if self.sub_width==8 else 0xb900003f
        code=[0x52800022,0xb9000002,store,0xd65f03c0] # mov w2,1; str w2,[x0]; str xzr/wzr,[x1]; ret
        self.uc.mem_write(self.GAME+self.MODE,struct.pack('<4I',*code))
    def set_mode(self,value):
        self.main_mode=value
        self.write32(self.GAME+self.MODE,0x52800002|((value&0xffff)<<5))
        self.uc.ctl_remove_cache(self.GAME+self.MODE,self.GAME+self.MODE+16)
    def stub(self,name,fn):
        address=self.SDK+0x100+len(self.stubs)*0x10
        self.stubs[address]=fn; self.names[name]=address
        self.uc.mem_write(address,struct.pack('<I',0xd65f03c0))
        return address
    def register_sdk(self):
        self.stub('_ZN2nn2ro12LookupSymbolEPmPKc',self.lookup)
        self.stub('getRegionAddress',lambda: self.result(self.GAME+[0,self.RO,self.DATA][self.reg(0)]))
        self.stub('A64HookFunction',self.install_hook)
        self.stub('_ZN2nn2oe17GetDisplayVersionEPNS0_14DisplayVersionE',lambda:self.uc.mem_write(self.reg(0),self.version.encode()+b'\0'))
        self.names['_ZN3lib9SingletonIN3app14FighterManagerEE9instance_E']=self.manager_slot
        self.stub('_ZN3app8lua_bind32FighterManager__entry_count_implEPNS_14FighterManagerE',lambda:self.result(self.count))
        self.stub('_ZN3app8lua_bind33FighterManager__get_entry_id_implEPNS_14FighterManagerEi',lambda:self.result(self.reg(1)))
        self.stub('_ZN3app8lua_bind44FighterManager__get_fighter_information_implEPNS_14FighterManagerENS_14FighterEntryIDE',lambda:self.result(self.HEAP+0x5000+self.reg(1)*0x40))
        self.stub('_ZN3app8lua_bind41FighterInformation__is_operation_cpu_implEPNS_18FighterInformationE',lambda:self.result(int((self.reg(0)-self.HEAP-0x5000)//0x40 in self.cpus)))
        self.stub('_ZN3app8lua_bind35FighterManager__is_result_mode_implEPNS_14FighterManagerE',lambda:self.result(int(self.results)))
        self.stub('_ZN3app14sv_information11is_ready_goEv',lambda:self.result(int(self.ready)))
        self.stub('_ZN2nn3hid15GetNpadStyleSetERKj',lambda:self.result(self.style if self.read32(self.reg(0))==self.controller_id else 0))
        for name in ['NpadFullKeyState','NpadJoyDualState','NpadHandheldState']:
            self.stub(f'_ZN2nn3hid12GetNpadStateEPNS0_{len(name)}{name}ERKj',self.read_pad)
        self.stub('_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci',self.fs_open)
        self.stub('_ZN2nn2fs10CreateFileEPKcl',self.fs_create)
        self.stub('_ZN2nn2fs11SetFileSizeENS0_10FileHandleEl',lambda:self.files.__setitem__('log',b'') or self.result(0))
        self.stub('_ZN2nn2fs11GetFileSizeEPlNS0_10FileHandleE',lambda:self.write64(self.reg(0),len(self.files.get('log',b''))) or self.result(0))
        self.stub('_ZN2nn2fs9WriteFileENS0_10FileHandleElPKvmRKNS0_11WriteOptionE',self.fs_write)
        self.stub('_ZN2nn2fs9CloseFileENS0_10FileHandleE',lambda:None)
        self.original=self.stub('mock_original_frame',self.original_frame)
    def lookup(self):
        out,name=self.reg(0),self.string(self.reg(1))
        address=self.names.get(name,0)
        if not self.native_log and name.startswith('_ZN2nn2fs'): address=0
        self.write64(out,address); self.result(0 if address else 1)
    def install_hook(self):
        assert self.reg(0)==self.GAME+self.FRAME
        self.hook=self.reg(1); self.write64(self.reg(2),self.original)
    def original_frame(self): self.original_calls+=1
    def read_pad(self):
        out=self.reg(0)
        self.uc.mem_write(out,struct.pack('<QQiiiiII',1,self.buttons,0,0,0,0,int(self.connected),0))
    def fs_open(self):
        assert self.string(self.reg(1))=='sd:/cpu_rematch.log'
        if 'log' not in self.files: self.result(1); return
        self.write64(self.reg(0),1); self.result(0)
    def fs_create(self): self.files['log']=b''; self.result(0)
    def fs_write(self):
        assert self.reg(1)==len(self.files.get('log',b''))
        self.files['log']+=bytes(self.uc.mem_read(self.reg(2),self.reg(3)))
        assert self.read32(self.reg(4))==1
        self.result(0)
    def sdk_hook(self,uc,address,size,data):
        if address==self.STOP: return
        assert address in self.stubs,hex(address)
        self.stubs[address]()
    def retry_hook(self,uc,address,size,data):
        assert self.reg(0)==self.state
        self.retry_calls+=1
    def region(self,address):
        for base,size,permission in [
            (self.PLUGIN,page(self.plugin_size),7),(self.SDK,0x100000,5),
            (self.STACK,0x100000,3),(self.GAME,self.RO,5),
            (self.GAME+self.RO,self.DATA-self.RO,1),
            (self.GAME+self.DATA,0x6000000-self.DATA,3),(self.HEAP,0x100000,3)]:
            if base<=address<base+size:return base,size,permission
        return 0,0,0
    def svc_hook(self,uc,interrupt,data):
        instruction=self.read32(uc.reg_read(UC_ARM64_REG_PC)-4)
        svc=(instruction>>5)&0xffff
        if svc==0x27:
            self.logs.append(bytes(uc.mem_read(self.reg(0),self.reg(1))).decode())
        elif svc==6:
            out,address=self.reg(0),self.reg(2)
            base,size,permission=self.region(address)
            if not size:self.result(1);return
            uc.mem_write(out,struct.pack('<QQIIIIII',base,size,4,0,permission,0,0,0))
            self.result(0);uc.reg_write(UC_ARM64_REG_X1,0)
        else: raise AssertionError(('unexpected svc',svc,hex(uc.reg_read(UC_ARM64_REG_PC))))
    def relocate(self):
        mod=u32(self.nro,4)
        dynamic=mod+struct.unpack_from('<i',self.nro,mod+4)[0]
        tags={}
        while True:
            tag,value=struct.unpack_from('<QQ',self.nro,dynamic);dynamic+=16
            if not tag:break
            tags[tag]=value
        strings=tags[5]; symbols=tags[6]
        count=u32(self.nro,tags[4]+4)
        sym=[];self.entry=None
        for i in range(count):
            n,info,other,section,value,size=struct.unpack_from('<IBBHQQ',self.nro,symbols+i*24)
            name=self.nro[strings+n:].split(b'\0',1)[0].decode()
            sym.append((name,value,section))
            if name=='main':self.entry=self.PLUGIN+value
        for tag,lengthtag in [(23,2),(7,8)]:
            if tag not in tags:continue
            for at in range(tags[tag],tags[tag]+tags[lengthtag],24):
                offset,info,addend=struct.unpack_from('<QQq',self.nro,at)
                index,kind=info>>32,info&0xffffffff
                if kind==1027:value=self.PLUGIN+addend
                elif kind in [1025,1026]:
                    name,value,section=sym[index]
                    value=(self.PLUGIN+value if section else self.names[name])+addend
                else:raise AssertionError(('unsupported relocation',kind))
                self.write64(self.PLUGIN+offset,value)
        assert self.entry is not None
    def call(self,address,arg=0,budget=1_000_000_000):
        self.uc.reg_write(UC_ARM64_REG_X0,arg)
        self.uc.reg_write(UC_ARM64_REG_X30,self.STOP)
        sp=self.uc.reg_read(UC_ARM64_REG_SP)
        saved={UC_ARM64_REG_X19+i:0xbad000+i for i in range(10)}
        saved.update({UC_ARM64_REG_D8+i:0x1234567800000000+i for i in range(8)})
        for register,value in saved.items():self.uc.reg_write(register,value)
        self.uc.emu_start(address,self.STOP,count=budget)
        assert self.uc.reg_read(UC_ARM64_REG_PC)==self.STOP,'instruction budget exhausted'
        assert self.uc.reg_read(UC_ARM64_REG_SP)==sp,'stack pointer corrupted'
        for register,value in saved.items():assert self.uc.reg_read(register)==value,'callee-saved register corrupted'
    def start(self):
        began=time.monotonic();self.call(self.entry)
        assert self.hook,'plugin failed initialization: '+''.join(self.logs)
        print(f'ARM startup passed ({time.monotonic()-began:.2f}s)',flush=True)
    def frame(self,buttons=0):
        self.buttons=buttons
        self.call(self.hook,self.HEAP+0x8000,budget=1_000_000)
    def press(self):self.frame(0);self.frame(0xc8)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('nro')
    parser.add_argument('--vtable-data',action='store_true')
    parser.add_argument('--expect-blocked',action='store_true')
    parser.add_argument('--sub-width',type=int,choices=[4,8],default=8)
    parser.add_argument('--pad-style',type=int,choices=[1,2,4],default=1)
    parser.add_argument('--controller-id',type=int,default=0)
    parser.add_argument('--game-base',type=lambda s:int(s,0))
    parser.add_argument('--plugin-base',type=lambda s:int(s,0))
    args=parser.parse_args()
    r=Runtime(args.nro,sub_width=args.sub_width,vtable_data=args.vtable_data,
        game_base=args.game_base,plugin_base=args.plugin_base)
    r.style=args.pad_style;r.controller_id=args.controller_id
    r.start();r.press()
    label='data' if args.vtable_data else 'rodata'
    folder='compiled-plugin'
    suffix=f'{folder}-{label}-pad{args.pad_style}-{args.controller_id}-width{args.sub_width}'
    Path(f'arm-trace-{suffix}.log').write_text(''.join(r.logs))
    if args.expect_blocked:
        assert r.retry_calls==0
        trace=''.join(r.logs)
        if args.vtable_data:
            assert 'Retry vtable=' in trace and 'Retry target=' not in trace
        else:
            assert 'Eligibility status=0x0000000000010000' in trace
        print('Observed blocked retry as expected; trace captured',flush=True);return
    assert r.retry_calls==1,'shortcut did not reach retry callback; inspect last-arm-trace.log'
    print('PASS: loaded NRO -> initialization -> mode -> CPU checks -> HID -> pointer chain -> retry callback',flush=True)
    for _ in range(200):r.frame(0xc8)
    assert r.retry_calls==1
    r.press();assert r.retry_calls==2
    print('PASS: held input does not loop; release allows another restart after cooldown',flush=True)
    for _ in range(181):r.frame()
    r.ready=False;r.press();assert r.retry_calls==3
    r.ready=True
    print('PASS: simulated post-GAME ready-flag transition retains restart ability while battle stays loaded',flush=True)
    before=r.retry_calls
    for mode in [0,18,45,58]:
        r.set_mode(mode);r.press();assert r.retry_calls==before
    r.set_mode(1)
    for cpus in [set(),{0,1}]:
        r.cpus=cpus;r.press();assert r.retry_calls==before
    r.cpus={1};r.results=True;r.press();assert r.retry_calls==before
    r.results=False;r.count=3;r.press();assert r.retry_calls==before
    print('PASS: menus/training/online/wireless/results and wrong participant layouts are rejected',flush=True)
    r.count=2;r.connected=False;r.press();assert r.retry_calls==before
    r.connected=True;r.ready=False;r.set_mode(0);r.frame();r.set_mode(1);r.press();assert r.retry_calls==before
    r.ready=True;r.frame(0xc8);assert r.retry_calls==before
    r.press();assert r.retry_calls==before+1
    print('PASS: disconnected and pre-countdown input is rejected; release/new press works when ready',flush=True)
    for _ in range(181):r.frame()
    before=r.retry_calls
    r.write64(r.GAME+r.ROOT,0xdead0000);r.press();assert r.retry_calls==before
    r.restore_chain()
    for _ in range(181):r.frame()
    r.write64(r.vtable+40,r.HEAP+0x10000);r.press();assert r.retry_calls==before
    print('PASS: unmapped pointer chain and non-game retry targets are rejected',flush=True)
    assert r.files['log'].decode()==''.join(r.logs),'file and emulator diagnostics diverged'
    print('PASS: native file diagnostics equal emulated debug output; ARM stack/register checks pass',flush=True)

if __name__=='__main__':main()
