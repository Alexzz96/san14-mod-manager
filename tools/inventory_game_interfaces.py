"""Read-only PE64 RTTI inventory. No process attachment, calls, or game writes.

This inventories symbols, not callable signatures. Use static disassembly and
runtime observation separately before promoting any entry to a writable API.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


def inventory(data):
    def unpack(fmt, at):
        if at < 0 or at + struct.calcsize(fmt) > len(data):
            raise ValueError('Truncated PE structure')
        return struct.unpack_from(fmt, data, at)
    if data[:2] != b'MZ':
        raise ValueError('Expected PE executable')
    nt, = unpack('<I', 0x3c)
    if data[nt:nt+4] != b'PE\0\0':
        raise ValueError('Invalid PE signature')
    machine, count = unpack('<HH', nt+4)
    optional_size, = unpack('<H', nt+20)
    magic, = unpack('<H', nt+24)
    if machine != 0x8664 or magic != 0x20b or count > 96:
        raise ValueError('Expected Windows x64 PE32+')
    image_base, = unpack('<Q', nt+48)
    image_size, = unpack('<I', nt+80)
    if image_size > 512*1024*1024 or image_size < 4096:
        raise ValueError('Invalid image size')
    sections = []
    for n in range(count):
        at = nt+24+optional_size+n*40
        name = data[at:at+8].rstrip(b'\0').decode('ascii', errors='replace')
        size, rva, raw_size, raw_at = unpack('<4I', at+8)
        flags, = unpack('<I', at+36)
        if raw_at+raw_size > len(data) or rva+max(size,raw_size) > image_size:
            raise ValueError('Section outside executable')
        sections.append((name,rva,size,raw_at,raw_size,flags))
    mem=bytearray(image_size)
    for _,rva,_,at,length,_ in sections:
        mem[rva:rva+length]=data[at:at+length]
    def code(rva):
        return any(flags & 0x20000000 and start <= rva < start+max(size,length)
                   for _,start,size,_,length,flags in sections)
    names={m.group(1).decode():m.start() for m in re.finditer(rb'\.\?AV([A-Za-z_][A-Za-z_0-9]*)@@',mem)}
    pattern=re.compile(r'AI$|^CAIManager$|EvaluationNode$|^CListSort|^CDeparture|^CCommand.*(?:Departure|Depature|Transport|Retreat|Target|Government|Post|Search|Item|Learn)|^CGetAttackTarget|^CDomesticOfficer|^CDomesticPriority|^CEditEventCTkEffCmd_(?:AddLike|SetPartner|Set_TreatyBrother|ChangePersonParam|ChangePersonLoyalty|ChangeIllness)$|^C(?:Person|Rank|Item|Tactics|Personality)Data$')
    entries=[]
    for name,rva in sorted(names.items()):
        if not pattern.search(name):
            continue
        descriptor=rva-16
        records=[]
        for m in re.finditer(re.escape(struct.pack('<I',descriptor)),mem):
            col=m.start()-12
            if col<0 or col+24>len(mem):
                continue
            if struct.unpack_from('<I',mem,col)[0]!=1 or struct.unpack_from('<I',mem,col+20)[0]!=col:
                continue
            for v in re.finditer(re.escape(struct.pack('<Q',image_base+col)),mem):
                table=v.start()+8;candidates=[]
                # Executable pointers can also be adjacent callback arrays.
                # RTTI identifies the table start, not its method count.
                for slot in range(128):
                    if table+slot*8+8>len(mem):break
                    target=struct.unpack_from('<Q',mem,table+slot*8)[0]-image_base
                    if not code(target):break
                    candidates.append({'pointer_index':slot,'rva':hex(target)})
                if candidates:
                    records.append({'complete_object_locator':hex(col),'vtable':hex(table),
                                    'first_virtual_entry':candidates[0]['rva'],
                                    'method_count':None,'boundary_verified':False,
                                    'contiguous_code_pointer_candidates':candidates})
        entries.append({'type':name,'typename_rva':hex(rva),'type_descriptor_rva':hex(descriptor),
                        'vtables':records,'evidence':'type_name_only','signature':None,
                        'call_relationships':None,'read_range':None,'write_range':None,'runtime_observed':False})
    return {'schema_version':2,'exe_sha256':hashlib.sha256(data).hexdigest(),'image_base':hex(image_base),
            'method':'MSVC PE64 RTTI / COL / executable-section pointer classification',
            'limitations':'RTTI establishes table starts, not method counts. Contiguous executable pointers may belong to adjacent callback tables. No semantics or safe callable signatures are inferred.',
            'entries':entries}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe',required=True,type=Path)
    parser.add_argument('--out',required=True,type=Path)
    args=parser.parse_args()
    if args.exe.resolve()==args.out.resolve():
        parser.error('Output must differ from executable')
    result=inventory(args.exe.read_bytes())
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps({'entries':len(result['entries']),'output':str(args.out),'exe_unchanged':True}))


if __name__=='__main__':
    main()
