import struct, sys
src, dst = sys.argv[1], sys.argv[2]
d = open(src,'rb').read()
le = struct.unpack_from('<I', d, 0x3c)[0]
assert d[le:le+2] == b'LE'
H = lambda o: struct.unpack_from('<I', d, le+o)[0]
npages, eip_obj, eip, esp_obj, esp, pagesz, lastpg = H(0x14),H(0x18),H(0x1c),H(0x20),H(0x24),H(0x28),H(0x2c)
objtab, nobj, pagemap = H(0x40),H(0x44),H(0x48)
fpt, frt, datapages = H(0x68),H(0x6c),H(0x80)
objs=[]
for i in range(nobj):
    vs, base, fl, pidx, pcnt, _ = struct.unpack_from('<6I', d, le+objtab+i*24)
    objs.append(dict(vs=vs,base=base,fl=fl,pidx=pidx,pcnt=pcnt))
def pageinfo(p):  # 1-based
    b = d[le+pagemap+(p-1)*4: le+pagemap+(p-1)*4+4]
    return (b[0]<<16)|(b[1]<<8)|b[2], b[3]
img = {}
for i,o in enumerate(objs):
    buf = bytearray(max(o['vs'], o['pcnt']*pagesz))
    for k in range(o['pcnt']):
        p = o['pidx']+k
        num, fl = pageinfo(p)
        if num == 0: continue
        size = lastpg if p == npages else pagesz
        off = datapages + (num-1)*pagesz
        buf[k*pagesz:k*pagesz+size] = d[off:off+size]
    img[i+1] = buf
nfix = 0; kinds = {}
for i,o in enumerate(objs):
    for k in range(o['pcnt']):
        p = o['pidx']+k
        a = struct.unpack_from('<I', d, le+fpt+(p-1)*4)[0]
        b = struct.unpack_from('<I', d, le+fpt+p*4)[0]
        pos = le+frt+a; end = le+frt+b
        while pos < end:
            st, fl = d[pos], d[pos+1]; pos += 2
            if st & 0x20:
                cnt = d[pos]; pos += 1
            else:
                srcs = [struct.unpack_from('<h', d, pos)[0]]; pos += 2
            assert fl & 3 == 0, 'non-internal fixup'
            if fl & 0x40: tobj = struct.unpack_from('<H', d, pos)[0]; pos += 2
            else: tobj = d[pos]; pos += 1
            stype = st & 0xf
            if stype == 2: toff = 0
            elif fl & 0x10: toff = struct.unpack_from('<I', d, pos)[0]; pos += 4
            else: toff = struct.unpack_from('<H', d, pos)[0]; pos += 2
            if st & 0x20:
                srcs = list(struct.unpack_from('<%dh'%cnt, d, pos)); pos += 2*cnt
            tgt = objs[tobj-1]['base'] + toff
            buf = img[i+1]
            for s in srcs:
                at = k*pagesz + s
                if at < 0 or at+4 > len(buf): continue
                kinds[stype] = kinds.get(stype,0)+1; nfix += 1
                if stype == 7: struct.pack_into('<I', buf, at, tgt)
                elif stype == 8: struct.pack_into('<i', buf, at, tgt - (o['base']+at+4))
                elif stype == 5: struct.pack_into('<H', buf, at, tgt & 0xffff)
                # selectors (2) / far ptrs left as-is: flat model under DOS4GW
for i,o in enumerate(objs):
    print('obj %d base %08x vsize %08x flags %04x pages %d' % (i+1,o['base'],o['vs'],o['fl'],o['pcnt']))
entry = objs[eip_obj-1]['base'] + eip
print('entry %08x fixups %d kinds %s' % (entry, nfix, kinds))
# write ELF32
phs=[]; data=b''; hdr=52; phsz=32*len(objs); cur=hdr+phsz
cur = (cur+0xfff)&~0xfff
blobs=[]
for i,o in enumerate(objs):
    buf = bytes(img[i+1][:o['vs']]) if o['vs'] else bytes(img[i+1])
    flags = (4 if o['fl']&1 else 0)|(2 if o['fl']&2 else 0)|(1 if o['fl']&4 else 0)
    phs.append(struct.pack('<8I',1,cur,o['base'],o['base'],len(buf),max(o['vs'],len(buf)),flags,0x1000))
    blobs.append((cur,buf)); cur = (cur+len(buf)+0xfff)&~0xfff
eh = b'\x7fELF\x01\x01\x01'+b'\0'*9+struct.pack('<HHIIIIIHHHHHH',2,3,1,entry,hdr,0,0,52,32,len(objs),40,0,0)
out = bytearray(cur); out[:52]=eh; out[52:52+phsz]=b''.join(phs)
for off,buf in blobs: out[off:off+len(buf)]=buf
open(dst,'wb').write(out)
