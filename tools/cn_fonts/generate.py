#!/usr/bin/env python3
"""Deterministic offline GX12 Chinese fixed-cell font generator."""
from __future__ import annotations
import argparse, ast, hashlib, io, json, re, sys, tarfile, zipfile
from dataclasses import dataclass
from pathlib import Path, PurePosixPath

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = Path(__file__).with_name("manifest.json")
FALLBACK = 0x25A1
IDS = ["STR_ABOUTUS","STR_ALARMSWARN","STR_BATTERY","STR_EMERGENCY_MODE",
       "STR_FAILSAFEWARN","STR_MODEL","STR_NO_SDCARD","STR_SD_CARD",
       "STR_STORAGE_WARNING","STR_SWITCHWARN","STR_TEST_WARNING",
       "STR_THROTTLE_UPPERCASE","STR_WARNING","STR_WRONG_PCBREV"]
FONT_CONTRACT = {
 "CN_8":  {"source":"fusion8","policy":"all-cn-h","expected_glyphs":625,"expected_translation_codepoints":624,"body":[8,8],"cell":[8,8],"top_offset":0},
 "CN_DEFAULT_10": {"source":"fusion10","policy":"all-cn-h-ascii","expected_glyphs":720,"expected_translation_codepoints":624,"body":[10,10],"cell":[10,10],"top_offset":0},
 "CN_10": {"source":"fusion10","policy":"fallback-only","expected_glyphs":1,"expected_translation_codepoints":0,"body":[10,10],"cell":[10,12],"top_offset":1},
 "CN_12": {"source":"fusion12","policy":"identifiers","expected_glyphs":34,"expected_translation_codepoints":33,"body":[12,12],"cell":[12,16],"top_offset":2},
 "CN_16": {"source":"wqy16b","policy":"identifiers","expected_glyphs":34,"expected_translation_codepoints":33,"body":[16,16],"cell":[16,20],"top_offset":2,"source_advance":17},
 "CN_32": {"source":"ark16","policy":"fallback-only","expected_glyphs":1,"expected_translation_codepoints":0,"body":[32,32],"cell":[32,38],"top_offset":3,"scale":2}}
SOURCE_KEYS = ["fusion8","fusion10","fusion12","wqy16b","ark16"]
WQY_BOLD_SOURCE_CONTRACT = {
 "kind":"bdf-tar-gz", "version":"0.7.0-4",
 "url":"https://sourceforge.net/projects/wqy/files/wqy-bitmapfont/0.7.0/wqy-bitmapfont-bdf-0.7.0-4.tar.gz/download",
 "archive_size":5203506, "archive_sha256":"E1A9BF2D4E608EAADA9822F58F33626204B665A4A60A353DCEB0C5FC09A75D40",
 "member":"wqy-bitmapfont/wenquanyi_12ptb.bdf", "member_size":5462992, "member_sha256":"26F493DC492BF64EB974E81C174BBCEDD2FF7FBD116428865992388B04A09042",
 "subset":"radio/src/fonts/cn/source/wqy-16px-bold-zh.subset.bdf", "subset_sha256":"D8047A55DC1F456A41D87FEDDED4C9B8F83B408F12357EF1180F592E7AE804C3"}

def fail(s): raise ValueError(s)
def sha256(b): return hashlib.sha256(b).hexdigest().upper()
def cjk(c): return 0x3400<=c<=0x4DBF or 0x4E00<=c<=0x9FFF or 0xF900<=c<=0xFAFF
def strings(s): return [ast.literal_eval(x) for x in re.findall(r'"(?:[^"\\]|\\.)*"',s)]

def validate_manifest(m):
    if m.get("schema") != 7 or m.get("translation") != "radio/src/translations/i18n/cn.h" or m.get("fallback") != "U+25A1": fail("immutable manifest schema/fallback/translation contract changed")
    if m.get("cn12_identifiers") != IDS or len(set(m["cn12_identifiers"])) != 14: fail("CN_12 ordered identifier contract changed")
    if m.get("profiles") != FONT_CONTRACT: fail("immutable font profile geometry/policy/count/source contract changed")
    if list(m.get("sources",{})) != SOURCE_KEYS: fail("source contract changed")
    common={"kind","version","url","member","member_sha256","subset","subset_sha256"}
    for key,s in m["sources"].items():
        if s.get("kind")=="bdf-zip":
            if set(s)!=common|{"zip_sha256"} or s["version"]!="2026.07.01": fail(f"{key}: ZIP source schema changed")
            archive_hash=s["zip_sha256"]
        elif s.get("kind")=="bdf-tar-gz":
            if set(s)!=common|{"archive_size","archive_sha256","member_size"}: fail(f"{key}: tar.gz source schema changed")
            if not isinstance(s["archive_size"],int) or s["archive_size"]<=0 or not isinstance(s["member_size"],int) or s["member_size"]<=0: fail(f"{key}: invalid archive/member size")
            archive_hash=s["archive_sha256"]
        else: fail(f"{key}: unsupported source kind")
        if not re.fullmatch(r"[0-9A-F]{64}",archive_hash) or not re.fullmatch(r"[0-9A-F]{64}",s["member_sha256"]) or not re.fullmatch(r"[0-9A-F]{64}",s["subset_sha256"]): fail(f"{key}: invalid source hash")
    if m["sources"].get("wqy16b") != WQY_BOLD_SOURCE_CONTRACT: fail("wqy16b source/hash contract changed")

def resolve(expr):
    old=None
    while expr!=old:
        old=expr
        for name,arg in (("TR_BW_COL",0),("TR_SFC_AIR",1),("TR3",0),("TR",0)):
            p=expr.find(name+"(")
            if p<0: continue
            start=p+len(name)+1; depth=1; quote=esc=False; parts=[]; last=start; end=None
            for i,ch in enumerate(expr[start:],start):
                if quote:
                    if esc: esc=False
                    elif ch=="\\": esc=True
                    elif ch=='"': quote=False
                elif ch=='"': quote=True
                elif ch=='(': depth+=1
                elif ch==')':
                    depth-=1
                    if depth==0: parts.append(expr[last:i]); end=i; break
                elif ch==',' and depth==1: parts.append(expr[last:i]); last=i+1
            if end is None or arg>=len(parts): fail("unresolved selector")
            expr=expr[:p]+parts[arg].strip()+expr[end+1:]; break
    if re.search(r"\b(?:TR|TR3|TR_BW_COL|TR_SFC_AIR)\s*\(",expr): fail("unresolved selector")
    return expr

def codepoints(m):
    validate_manifest(m)
    text=(ROOT/m["translation"]).read_text(encoding="utf-8")
    allcp=sorted({ord(ch) for s in strings(text) for ch in s if cjk(ord(ch)) or ord(ch) in (0x3001,0xFF0C)})
    defs={x.group(1):x.group(2) for x in re.finditer(r"^#define\s+(TR_\w+)\s+(.+)$",text,re.M)}; selected=[]
    for ident in IDS:
        key="TR_"+ident[4:]
        if key not in defs: fail(f"unresolved {ident}")
        vals=strings(resolve(defs[key]))
        if not vals: fail(f"non-string {ident}")
        selected += [ord(ch) for s in vals for ch in s if cjk(ord(ch))]
    c12=sorted(set(selected))
    if len(allcp)!=624 or len(c12)!=33: fail(f"translation coverage changed ({len(allcp)}/{len(c12)})")
    out={"CN_8":[FALLBACK]+allcp,
         "CN_DEFAULT_10":[FALLBACK]+list(range(0x20,0x7F))+allcp,
         "CN_10":[FALLBACK],
         "CN_12":[FALLBACK]+c12,"CN_16":[FALLBACK]+c12,
         "CN_32":[FALLBACK]}
    for n,v in out.items():
        if v[0]!=FALLBACK or v[1:]!=sorted(set(v[1:])) or len(v)!=FONT_CONTRACT[n]["expected_glyphs"]: fail(f"{n}: codepoint contract failed")
    return out

@dataclass
class Glyph:
    cp:int; dwidth:tuple[int,int]; bbx:tuple[int,int,int,int]; rows:list[int]; record:bytes

def parse_glyph_record(rec):
    """Parse one BDF glyph using the ordered BDF 2.1 line grammar."""
    lines=rec.splitlines()
    if not lines or not re.fullmatch(r"STARTCHAR\s+\S+",lines[0]): fail("glyph must start with STARTCHAR name")
    if lines[-1]!="ENDCHAR": fail("glyph must end with ENDCHAR")
    fields={}; bitmap_at=None
    grammar=[("ENCODING",r"-?\d+(?:\s+\d+)?",True),("SWIDTH",r"-?\d+\s+-?\d+",False),
             ("DWIDTH",r"-?\d+\s+-?\d+",True),("SWIDTH1",r"-?\d+\s+-?\d+",False),
             ("DWIDTH1",r"-?\d+\s+-?\d+",False),("VVECTOR",r"-?\d+\s+-?\d+",False),
             ("BBX",r"\d+\s+\d+\s+-?\d+\s+-?\d+",True),("ATTRIBUTES",r"[0-9A-Fa-f]+",False),
             ("BITMAP",r"",True)]
    rank={name:i for i,(name,_,_) in enumerate(grammar)}; patterns={name:pat for name,pat,_ in grammar}; previous=-1
    for i,line in enumerate(lines[1:-1],1):
        name=line.split(None,1)[0] if line else ""
        if bitmap_at is not None:
            continue
        if name not in rank: fail(f"unknown glyph field: {line}")
        if name in fields: fail(f"duplicate glyph field: {name}")
        if rank[name]<previous: fail(f"glyph field out of order: {name}")
        value=line[len(name):].strip()
        if not re.fullmatch(patterns[name],value): fail(f"invalid {name} field")
        fields[name]=value; previous=rank[name]
        if name=="BITMAP": bitmap_at=i
    for name,_,required in grammar:
        if required and name not in fields: fail(f"missing glyph field: {name}")
    if bitmap_at is None: fail("missing glyph field: BITMAP")
    # Once BITMAP starts, every line before ENDCHAR is bitmap data; field-like
    # or otherwise non-hex content is rejected as a row, never silently parsed.
    w,h,xo,yo=map(int,fields["BBX"].split()); bitmap=lines[bitmap_at+1:-1]
    if len(bitmap)!=h: fail(f"BITMAP row count {len(bitmap)} does not match BBX height {h}")
    row_chars=2*((w+7)//8); rows=[]; unused=(8-w%8)%8
    for row in bitmap:
        if not re.fullmatch(rf"[0-9A-Fa-f]{{{row_chars}}}",row): fail(f"invalid BITMAP row width/hex: {row}")
        value=int(row,16)
        if unused and value&((1<<unused)-1): fail("nonzero BITMAP row padding bits")
        rows.append(value)
    encoding=int(fields["ENCODING"].split()[0]); dwidth=tuple(map(int,fields["DWIDTH"].split()))
    return encoding,dwidth,(w,h,xo,yo),rows

def parse_bdf(data):
    try: text=data.decode("ascii")
    except UnicodeDecodeError: fail("BDF is not ASCII")
    if not text.startswith("STARTFONT ") or not text.endswith("ENDFONT\n"): fail("BDF must have STARTFONT and final ENDFONT")
    cm=re.search(r"^CHARS\s+(\d+)\r?$",text,re.M); ma=re.search(r"^FONT_ASCENT\s+(\d+)\r?$",text,re.M); md=re.search(r"^FONT_DESCENT\s+(\d+)\r?$",text,re.M)
    if not (cm and ma and md): fail("BDF missing CHARS/ascent/descent")
    first=text.find("STARTCHAR "); endfont=text.rfind("ENDFONT")
    if first<0: records=[]; middle=""
    else:
        middle=text[first:endfont]; records=list(re.finditer(r"^STARTCHAR .*?^ENDCHAR\r?\n",middle,re.M|re.S))
        if not records or "".join(x.group(0) for x in records)!=middle: fail("BDF contains trailing/unparsed glyph content")
    if len(records)!=int(cm.group(1)): fail(f"BDF CHARS says {cm.group(1)}, found {len(records)}")
    glyphs={}
    for x in records:
        rec=x.group(0); cp,dwidth,bbx,rows=parse_glyph_record(rec)
        if cp>=0:
            if cp in glyphs: fail("duplicate BDF encoding")
            glyphs[cp]=Glyph(cp,dwidth,bbx,rows,rec.encode("ascii"))
    return int(ma.group(1)),int(md.group(1)),glyphs

def bdf_body(g,ascent,descent,size,source_advance=None):
    expected_width=source_advance if source_advance is not None else (size//2 if 0x20<=g.cp<=0x7E else size)
    if ascent+descent<size or g.dwidth!=(expected_width,0): fail(f"U+{g.cp:04X}: source metric mismatch")
    w,h,xo,yo=g.bbx; top=ascent-(yo+h); bits=((w+7)//8)*8; out=[[0]*size for _ in range(size)]
    for sy,row in enumerate(g.rows):
        for sx in range(w):
            x,y=xo+sx,top+sy
            if 0<=x<size and 0<=y<size and row&(1<<(bits-1-sx)): out[y][x]=1
    return out

def scale_body(body,scale):
    if scale<=0 or not body or any(len(r)!=len(body) for r in body): fail("source body must be square and scale positive")
    size=len(body)*scale
    return [[body[y//scale][x//scale] for x in range(size)] for y in range(size)]

def scale2(body):
    return scale_body(body,2)

def validate_logical_width(name,cp,body,logical_width):
    body_width=len(body)
    if not body or any(len(row)!=body_width for row in body): fail(f"{name} U+{cp:04X}: invalid body dimensions")
    if logical_width<=0 or logical_width>body_width: fail(f"{name} U+{cp:04X}: invalid logical width {logical_width}")
    if any(any(row[logical_width:]) for row in body): fail(f"{name} U+{cp:04X}: pixels beyond logical width {logical_width}")

def pack(body,width,height,top):
    if len(body)!=width or any(len(r)!=width for r in body): fail("body dimensions changed")
    out=bytearray(); blocks=(height+7)//8
    for x in range(width):
        for block in range(blocks):
            out.append(sum((1<<bit) for bit in range(8) if 0<=block*8+bit-top<len(body) and body[block*8+bit-top][x]))
    return bytes(out)

def index_bdf_records(data):
    try: text=data.decode("ascii")
    except UnicodeDecodeError: fail("BDF is not ASCII")
    if not text.startswith("STARTFONT ") or not text.endswith("ENDFONT\n"): fail("BDF must have STARTFONT and final ENDFONT")
    cm=re.search(r"^CHARS\s+(\d+)\r?$",text,re.M)
    if not cm: fail("BDF missing CHARS")
    first=text.find("STARTCHAR "); end=text.rfind("ENDFONT")
    if first<0: records=[]; middle=""
    else:
        middle=text[first:end]; records=list(re.finditer(r"^STARTCHAR .*?^ENDCHAR\r?\n",middle,re.M|re.S))
        if not records or "".join(x.group(0) for x in records)!=middle: fail("BDF contains trailing/unparsed glyph content")
    if len(records)!=int(cm.group(1)): fail(f"BDF CHARS says {cm.group(1)}, found {len(records)}")
    indexed={}
    for match in records:
        rec=match.group(0); encoding=re.search(r"^ENCODING\s+(-?\d+)(?:\s+-?\d+)?\r?$",rec,re.M)
        if not encoding: fail("glyph missing valid ENCODING")
        cp=int(encoding.group(1))
        if cp>=0:
            if cp in indexed: fail("duplicate BDF encoding")
            indexed[cp]=rec.encode("ascii")
    return text,first,end,indexed

def subset_bdf(full,wanted):
    text,first,end,glyphs=index_bdf_records(full); missing=sorted(set(wanted)-set(glyphs))
    if missing: fail("missing BDF glyphs: "+", ".join(f"U+{x:04X}" for x in missing[:10]))
    header=re.sub(r"^CHARS\s+\d+\r?$",f"CHARS {len(wanted)}",text[:first],flags=re.M)
    result=header.encode("ascii")+b"".join(glyphs[x] for x in wanted)+text[end:].encode("ascii")
    parse_bdf(result)
    return result

def zip_member(blob,source):
    if sha256(blob)!=source["zip_sha256"]: fail("release ZIP hash mismatch")
    try:
        with zipfile.ZipFile(io.BytesIO(blob)) as z:
            matches=[n for n in z.namelist() if n==source["member"]]
            if len(matches)!=1: fail("ZIP does not contain exactly one specified member")
            data=z.read(matches[0])
    except zipfile.BadZipFile: fail("invalid release ZIP")
    if sha256(data)!=source["member_sha256"]: fail("ZIP member hash mismatch")
    return data

def safe_tar_name(name):
    path=PurePosixPath(name)
    return bool(name) and "\\" not in name and not path.is_absolute() and ".." not in path.parts

def tar_member(blob,source):
    if len(blob)!=source["archive_size"]: fail("release tar.gz size mismatch")
    if sha256(blob)!=source["archive_sha256"]: fail("release tar.gz hash mismatch")
    try:
        with tarfile.open(fileobj=io.BytesIO(blob),mode="r:gz") as archive:
            members=archive.getmembers()
            if any(not safe_tar_name(member.name) for member in members): fail("unsafe tar member path")
            matches=[member for member in members if member.name==source["member"]]
            if len(matches)!=1: fail("tar.gz does not contain exactly one specified member")
            if not matches[0].isfile(): fail("specified tar.gz member is not a regular file")
            if "member_size" in source and matches[0].size!=source["member_size"]: fail("tar.gz member size mismatch")
            stream=archive.extractfile(matches[0])
            if stream is None: fail("specified tar.gz member is unreadable")
            data=stream.read()
    except tarfile.TarError: fail("invalid release tar.gz")
    if sha256(data)!=source["member_sha256"]: fail("tar.gz member hash mismatch")
    return data

def archive_member(blob,source):
    if source.get("kind")=="bdf-zip": return zip_member(blob,source)
    if source.get("kind")=="bdf-tar-gz": return tar_member(blob,source)
    fail("unsupported source kind")

def extract_one(blob,source,wanted): return subset_bdf(archive_member(blob,source),wanted)

def extract(args,m,cps):
    selected=[key for key in SOURCE_KEYS if getattr(args,key)]
    if not selected: fail("--extract-subset requires at least one source archive")
    for key in selected:
        path=getattr(args,key)
        wanted=sorted(set().union(*(cps[name] for name,spec in FONT_CONTRACT.items() if spec["source"]==key)))
        subset=extract_one(Path(path).read_bytes(),m["sources"][key],wanted); target=ROOT/m["sources"][key]["subset"]
        if m["sources"][key]["subset_sha256"] and sha256(subset)!=m["sources"][key]["subset_sha256"]: fail(f"{key}: extracted subset differs from manifest")
        target.parent.mkdir(parents=True,exist_ok=True); target.write_bytes(subset); print(f"{key}={sha256(subset)}")

def generated(m,cps):
    parsed={}
    for key in SOURCE_KEYS:
        s=m["sources"][key]; raw=(ROOT/s["subset"]).read_bytes()
        if sha256(raw)!=s["subset_sha256"]: fail(f"{key}: subset hash mismatch")
        asc,desc,glyphs=parse_bdf(raw)
        wanted=sorted(set().union(*(cps[name] for name,spec in FONT_CONTRACT.items() if spec["source"]==key)))
        if list(glyphs)!=wanted: fail(f"{key}: subset order/coverage mismatch")
        parsed[key]=(asc,desc,glyphs)
    bodies={}; widths={}
    for name,spec in FONT_CONTRACT.items():
        asc,desc,glyphs=parsed[spec["source"]]
        scale=spec.get("scale",1)
        body_width=spec["body"][0]
        if body_width%scale: fail(f"{name}: body width is not divisible by scale")
        source_size=body_width//scale
        logical_widths=[]
        for cp in cps[name]:
            logical_source_width=source_size//2 if 0x20<=cp<=0x7E else source_size
            expected=spec.get("source_advance",logical_source_width)
            width=glyphs[cp].dwidth[0]
            if glyphs[cp].dwidth!=(expected,0) or width<=0: fail(f"U+{cp:04X}: invalid source advance")
            gw,_,gx,_=glyphs[cp].bbx
            if "source_advance" in spec and (gx<0 or gx+gw>source_size): fail(f"U+{cp:04X}: horizontal bitmap clipping")
            logical_widths.append(logical_source_width*scale)
        vals=[bdf_body(glyphs[x],asc,desc,source_size,spec.get("source_advance")) for x in cps[name]]
        rendered=[scale_body(x,scale) for x in vals]
        for cp,body,logical_width in zip(cps[name],rendered,logical_widths):
            validate_logical_width(name,cp,body,logical_width)
        bodies[name]=rendered
        widths[name]=logical_widths
    outputs={}; outdir=ROOT/"radio/src/fonts/cn/generated"; command="python tools/cn_fonts/generate.py"
    for name,spec in FONT_CONTRACT.items():
        source=m["sources"][spec["source"]]; width,height=spec["cell"]; bw,bh=spec["body"]; top=spec["top_offset"]; bpg=width*((height+7)//8)
        glyphs=[pack(x,width,height,top) for x in bodies[name]]; guard="EDGETX_CN_FONT_"+name+"_H"
        if source["kind"]=="bdf-zip": archive_provenance=f"ZIP SHA256 {source['zip_sha256']}"
        else: archive_provenance=f"tar.gz SHA256 {source['archive_sha256']}"
        provenance=f"// Source: {source['url']} version {source['version']} {archive_provenance} member {source['member']} SHA256 {source['member_sha256']}\n// Subset SHA256: {source['subset_sha256']}\n// Generator command: {command}\n"
        constants=f"#define {name}_WIDTH {width}\n#define {name}_BODY_HEIGHT {bh}\n#define {name}_STORAGE_HEIGHT {height}\n#define {name}_TOP_OFFSET {top}\n#define {name}_BYTES_PER_GLYPH {bpg}\n#define {name}_GLYPH_COUNT {len(cps[name])}\n"
        h=f"// Generated; do not edit.\n{provenance}#ifndef {guard}\n#define {guard}\n#include <stdint.h>\n{constants}extern const uint16_t {name}_codepoints[{name}_GLYPH_COUNT];\nextern const uint8_t {name}_widths[{name}_GLYPH_COUNT];\nextern const uint8_t {name}_glyphs[{name}_GLYPH_COUNT][{name}_BYTES_PER_GLYPH];\n#endif\n"
        cptext=",\n  ".join(", ".join(f"0x{x:04X}" for x in cps[name][i:i+12]) for i in range(0,len(cps[name]),12)); rowtext=",\n".join("  {"+", ".join(f"0x{x:02X}" for x in g)+"}" for g in glyphs)
        widthtext=",\n  ".join(", ".join(str(x) for x in widths[name][i:i+24]) for i in range(0,len(widths[name]),24))
        cpp=f"// Generated; do not edit.\n{provenance}#include \"{name.lower()}.h\"\nconst uint16_t {name}_codepoints[{name}_GLYPH_COUNT] = {{\n  {cptext}\n}};\nconst uint8_t {name}_widths[{name}_GLYPH_COUNT] = {{\n  {widthtext}\n}};\nconst uint8_t {name}_glyphs[{name}_GLYPH_COUNT][{name}_BYTES_PER_GLYPH] = {{\n{rowtext}\n}};\n"
        outputs[outdir/f"{name.lower()}.h"]=h.encode(); outputs[outdir/f"{name.lower()}.cpp"]=cpp.encode()
    samples=[bodies[n][0] for n in FONT_CONTRACT]; scale=2; gap=4; pw=sum(len(x)*scale+gap for x in samples)-gap; ph=64; pix=[[0]*pw for _ in range(ph)]; ox=0
    for body in samples:
        for y,row in enumerate(body):
            for x,v in enumerate(row):
                if v:
                    for yy in range(2):
                        for xx in range(2): pix[y*2+yy][ox+x*2+xx]=1
        ox+=len(body)*2+gap
    outputs[outdir/"preview.pbm"]=(f"P1\n# {' '.join(FONT_CONTRACT)} U+25A1\n{pw} {ph}\n"+"\n".join(" ".join(map(str,r)) for r in pix)+"\n").encode()
    return outputs

def main(argv=None):
    p=argparse.ArgumentParser(); p.add_argument("--check",action="store_true"); p.add_argument("--extract-subset",action="store_true")
    for x in SOURCE_KEYS: p.add_argument("--"+x)
    a=p.parse_args(argv); m=json.loads(MANIFEST.read_text(encoding="utf-8")); cps=codepoints(m)
    if a.extract_subset: extract(a,m,cps); return
    outputs=generated(m,cps); changed=[]
    for path,data in outputs.items():
        if a.check:
            if not path.exists() or path.read_bytes()!=data: changed.append(str(path.relative_to(ROOT)))
        else: path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(data)
    if changed: fail("generated files differ: "+", ".join(changed))
    print(("checked" if a.check else "generated")+" "+", ".join(f"{n}={len(cps[n])}" for n in cps))
if __name__=="__main__":
    try: main()
    except (OSError,ValueError,KeyError,json.JSONDecodeError) as e: print("error:",e,file=sys.stderr); sys.exit(1)
