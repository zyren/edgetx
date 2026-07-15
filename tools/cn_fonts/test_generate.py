import copy, io, json, os, re, tempfile, unittest, warnings, zipfile
from pathlib import Path
import generate

ZIP_FILES={
 "fusion8":"fusion-pixel-font-8px-monospaced-bdf-v2026.07.01.zip",
 "fusion10":"fusion-pixel-font-10px-monospaced-bdf-v2026.07.01.zip",
 "fusion12":"fusion-pixel-font-12px-monospaced-bdf-v2026.07.01.zip",
 "ark16":"ark-pixel-font-16px-monospaced-bdf-v2026.07.01.zip"}

def parse_generated(name):
    header=(generate.ROOT/f"radio/src/fonts/cn/generated/{name.lower()}.h").read_text()
    cpp=(generate.ROOT/f"radio/src/fonts/cn/generated/{name.lower()}.cpp").read_text()
    constants={k:int(v) for k,v in re.findall(rf"#define {name}_(WIDTH|BODY_HEIGHT|STORAGE_HEIGHT|TOP_OFFSET|BYTES_PER_GLYPH|GLYPH_COUNT) (\d+)",header)}
    cm=re.search(rf"{name}_codepoints\[{name}_GLYPH_COUNT\] = \{{(.*?)\}};",cpp,re.S)
    wm=re.search(rf"{name}_widths\[{name}_GLYPH_COUNT\] = \{{(.*?)\}};",cpp,re.S)
    gm=re.search(rf"{name}_glyphs\[{name}_GLYPH_COUNT\]\[{name}_BYTES_PER_GLYPH\] = \{{(.*?)\n\}};",cpp,re.S)
    if not(cm and wm and gm): raise AssertionError("generated declarations not parseable")
    cps=[int(x,16) for x in re.findall(r"0x([0-9A-F]{4})",cm.group(1))]
    widths=[int(x) for x in re.findall(r"\d+",wm.group(1))]
    glyphs=[[int(x,16) for x in re.findall(r"0x([0-9A-F]{2})",row)] for row in re.findall(r"\{([^{}]*)\}",gm.group(1))]
    return constants,cps,widths,glyphs

def decode_cell(data,width,height):
    blocks=(height+7)//8
    return [[(data[x*blocks+y//8]>>(y%8))&1 for x in range(width)] for y in range(height)]

def reference_bdf(g,ascent,size):
    w,h,xo,yo=g.bbx; top=ascent-yo-h; bits=((w+7)//8)*8; out=[[0]*size for _ in range(size)]
    for sy,row in enumerate(g.rows):
        for sx in range(w):
            x,y=xo+sx,top+sy
            if 0<=x<size and 0<=y<size and row&(1<<(bits-1-sx)): out[y][x]=1
    return out

class GeneratorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.manifest=json.loads(generate.MANIFEST.read_text(encoding="utf-8")); cls.cps=generate.codepoints(cls.manifest)

    def test_manifest_contract_is_immutable(self):
        self.assertEqual(self.manifest["cn12_identifiers"],generate.IDS)
        for mutate in (
            lambda m:m.update(fallback="U+FFFD"),
            lambda m:m["cn12_identifiers"].pop(1), # duplicate-union identifier still cannot disappear
            lambda m:m["cn12_identifiers"].__setitem__(1,m["cn12_identifiers"][0]),
            lambda m:m["fonts"]["CN_10"].update(cell=[10,13],top_offset=0),
            lambda m:m["fonts"]["CN_12"].update(policy="fallback-only")):
            bad=copy.deepcopy(self.manifest); mutate(bad)
            with self.assertRaises(ValueError): generate.codepoints(bad)

    def test_counts_order_and_fallback(self):
        self.assertEqual({k:len(v) for k,v in self.cps.items()},{"CN_8":625,"CN_10":1,"CN_12":34,"CN_32":1})
        for v in self.cps.values():
            self.assertEqual(v[0],0x25A1); self.assertEqual(v[1:],sorted(set(v[1:])))
            self.assertFalse(set(v)&set(range(0x20,0x7F)))

    def test_real_release_zip_to_checked_in_subset(self):
        archive_dir=os.environ.get("EDGETX_CN_FONT_ARCHIVE_DIR")
        if not archive_dir: self.skipTest("EDGETX_CN_FONT_ARCHIVE_DIR is not set")
        zip_dir=Path(archive_dir)
        for key,name in zip(generate.SOURCE_KEYS,generate.FONT_CONTRACT):
            path=zip_dir/ZIP_FILES[key]; self.assertTrue(path.is_file(),path)
            source=self.manifest["sources"][key]
            subset=generate.extract_one(path.read_bytes(),source,self.cps[name])
            checked=(generate.ROOT/source["subset"]).read_bytes()
            self.assertEqual(subset,checked); self.assertEqual(generate.sha256(subset),source["subset_sha256"])

    def test_synthetic_zip_archive_member_and_subset_chain(self):
        record="STARTCHAR box\nENCODING 9633\nSWIDTH 1000 0\nDWIDTH 8 0\nBBX 8 1 0 0\nBITMAP\n80\nENDCHAR\n"
        bdf=("STARTFONT 2.1\nSTARTPROPERTIES 2\nFONT_ASCENT 7\nFONT_DESCENT 1\nENDPROPERTIES\nCHARS 1\n"+record+"ENDFONT\n").encode()
        stream=io.BytesIO()
        with zipfile.ZipFile(stream,"w",zipfile.ZIP_DEFLATED) as archive: archive.writestr("font.bdf",bdf)
        blob=stream.getvalue(); source={"zip_sha256":generate.sha256(blob),"member":"font.bdf","member_sha256":generate.sha256(bdf)}
        self.assertEqual(generate.extract_one(blob,source,[0x25A1]),bdf)
        for mutation in (lambda s:s.update(zip_sha256="0"*64),lambda s:s.update(member="missing.bdf"),lambda s:s.update(member_sha256="0"*64)):
            bad=copy.deepcopy(source); mutation(bad)
            with self.assertRaises(ValueError): generate.extract_one(blob,bad,[0x25A1])
        duplicate=io.BytesIO()
        with warnings.catch_warnings():
            warnings.simplefilter("ignore",UserWarning)
            with zipfile.ZipFile(duplicate,"w") as archive:
                archive.writestr("font.bdf",bdf); archive.writestr("font.bdf",bdf)
        db=duplicate.getvalue(); bad={**source,"zip_sha256":generate.sha256(db)}
        with self.assertRaisesRegex(ValueError,"exactly one"): generate.extract_one(db,bad,[0x25A1])

    def test_strict_glyph_line_grammar_negative_cases(self):
        base=["STARTCHAR x","ENCODING 1","SWIDTH 1000 0","DWIDTH 9 0","BBX 9 2 0 0","BITMAP","8000","0000","ENDCHAR"]
        cases={
          "unknown":base[:2]+["BOGUS 1"]+base[2:],
          "duplicate encoding":base[:2]+["ENCODING 2"]+base[2:],
          "duplicate dwidth":base[:4]+["DWIDTH 9 0"]+base[4:],
          "duplicate bbx":base[:5]+["BBX 9 2 0 0"]+base[5:],
          "duplicate bitmap":base[:-1]+["BITMAP"]+base[-1:],
          "short row":base[:6]+["80","0000"]+base[8:],
          "long row":base[:6]+["800000","0000"]+base[8:],
          "invalid hex":base[:6]+["ZZZZ","0000"]+base[8:],
          "nonzero padding":base[:6]+["8001","0000"]+base[8:],
          "row count":base[:7]+base[8:],
          "missing endchar":base[:-1],
          "missing encoding":base[:1]+base[2:],
          "missing dwidth":base[:3]+base[4:],
          "missing bbx":base[:4]+base[5:],
          "missing bitmap":base[:5]+base[6:]}
        for label,lines in cases.items():
            with self.subTest(label=label),self.assertRaises(ValueError): generate.parse_glyph_record("\n".join(lines)+"\n")

    def test_valid_complete_bdf_subsets_are_idempotent(self):
        for key,name in zip(generate.SOURCE_KEYS,generate.FONT_CONTRACT):
            raw=(generate.ROOT/self.manifest["sources"][key]["subset"]).read_bytes()
            self.assertTrue(raw.startswith(b"STARTFONT ")); self.assertTrue(raw.endswith(b"ENDFONT\n"))
            self.assertEqual(generate.subset_bdf(raw,self.cps[name]),raw)
            self.assertEqual(len(generate.parse_bdf(raw)[2]),len(self.cps[name]))

    def test_strict_bdf_rejects_count_missing_end_and_garbage(self):
        raw=(generate.ROOT/self.manifest["sources"]["fusion10"]["subset"]).read_bytes()
        for bad in (re.sub(br"CHARS \d+",b"CHARS 999",raw,count=1),raw[:-8],raw+b"garbage\n"):
            with self.assertRaises(ValueError): generate.parse_bdf(bad)
        empty=b"STARTFONT 2.1\nSTARTPROPERTIES 2\nFONT_ASCENT 7\nFONT_DESCENT 1\nENDPROPERTIES\nCHARS 0\nENDFONT\n"
        with self.assertRaisesRegex(ValueError,"missing BDF"): generate.subset_bdf(empty,[0x25A1])

    def test_generated_arrays_independently(self):
        for name,spec in generate.FONT_CONTRACT.items():
            c,cps,widths,glyphs=parse_generated(name); width,height=spec["cell"]; top=spec["top_offset"]; bh=spec["body"][1]; bpg=width*((height+7)//8)
            self.assertEqual(c,{"WIDTH":width,"BODY_HEIGHT":bh,"STORAGE_HEIGHT":height,"TOP_OFFSET":top,"BYTES_PER_GLYPH":bpg,"GLYPH_COUNT":len(self.cps[name])})
            self.assertEqual(cps,self.cps[name]); self.assertEqual(cps[0],0x25A1); self.assertEqual(cps[1:],sorted(set(cps[1:])))
            self.assertEqual(len(widths),len(cps)); self.assertEqual(widths[0],width)
            self.assertTrue(all(x==width for x in widths))
            self.assertTrue(all(0<x<=width for x in widths))
            self.assertEqual(len(glyphs),len(cps)); self.assertTrue(all(len(g)==bpg for g in glyphs))
            for g in glyphs:
                cell=decode_cell(g,width,height)
                self.assertFalse(any(any(r) for r in cell[:top])); self.assertFalse(any(any(r) for r in cell[top+bh:]))
                if height%8:
                    mask=~((1<<(height%8))-1)&0xff; blocks=(height+7)//8
                    self.assertTrue(all((g[x*blocks+blocks-1]&mask)==0 for x in range(width)))

    def test_arrays_match_independent_bdf_reference_and_ark_exact_2x(self):
        for key,name in zip(generate.SOURCE_KEYS,generate.FONT_CONTRACT):
            raw=(generate.ROOT/self.manifest["sources"][key]["subset"]).read_bytes(); asc,_,glyphs=generate.parse_bdf(raw)
            constants,cps,widths,arrays=parse_generated(name); source_size=16 if name=="CN_32" else constants["BODY_HEIGHT"]
            for i,cp in enumerate(cps):
                expected=reference_bdf(glyphs[cp],asc,source_size)
                if name=="CN_32": expected=[[expected[y//2][x//2] for x in range(32)] for y in range(32)]
                cell=decode_cell(arrays[i],constants["WIDTH"],constants["STORAGE_HEIGHT"])
                self.assertEqual([r[:] for r in cell[constants["TOP_OFFSET"]:constants["TOP_OFFSET"]+constants["BODY_HEIGHT"]]],expected)

    def test_subset_glyph_metrics_are_full_cell(self):
        for key,name in zip(generate.SOURCE_KEYS,generate.FONT_CONTRACT):
            raw=(generate.ROOT/self.manifest["sources"][key]["subset"]).read_bytes()
            _,_,glyphs=generate.parse_bdf(raw)
            source_width=16 if name=="CN_32" else generate.FONT_CONTRACT[name]["body"][0]
            self.assertEqual(list(glyphs),self.cps[name])
            self.assertEqual({glyph.dwidth for glyph in glyphs.values()},{(source_width,0)})

    def test_logical_width_accepts_pixels_within_width(self):
        body=[[0]*8 for _ in range(8)]; body[3][3]=1
        generate.validate_logical_width("CN_8",0x41,body,4)
        scaled=generate.scale2([[1 if x==7 and y==4 else 0 for x in range(16)] for y in range(16)])
        generate.validate_logical_width("CN_32",0x41,scaled,16)

    def test_logical_width_rejects_pixels_beyond_width_after_scaling(self):
        body=[[0]*8 for _ in range(8)]; body[3][4]=1
        with self.assertRaisesRegex(ValueError,r"CN_8 U\+0041: pixels beyond logical width 4"):
            generate.validate_logical_width("CN_8",0x41,body,4)
        scaled=generate.scale2([[1 if x==8 and y==4 else 0 for x in range(16)] for y in range(16)])
        with self.assertRaisesRegex(ValueError,r"CN_32 U\+0041: pixels beyond logical width 16"):
            generate.validate_logical_width("CN_32",0x41,scaled,16)

    def test_offsets_negative_x_clipping_and_real_12px_14row(self):
        g=generate.Glyph(1,(4,0),(3,4,-1,-1),[0xE0]*4,b"")
        self.assertEqual(generate.bdf_body(g,3,1,4),reference_bdf(g,3,4)); self.assertEqual(sum(map(sum,reference_bdf(g,3,4))),8)
        g2=generate.Glyph(2,(4,0),(2,4,1,1),[0xC0]*4,b"") # top=-2, clips upper two rows
        self.assertEqual(sum(map(sum,generate.bdf_body(g2,3,1,4))),4)
        raw=(generate.ROOT/self.manifest["sources"]["fusion12"]["subset"]).read_bytes(); asc,desc,glyphs=generate.parse_bdf(raw)
        real=next(g for g in glyphs.values() if g.bbx[1]==14)
        self.assertEqual(generate.bdf_body(real,asc,desc,12),reference_bdf(real,asc,12))

    def test_full_ff_column_is_valid_data(self):
        body=[[0]*8 for _ in range(8)]
        for y in range(8): body[y][3]=1
        packed=generate.pack(body,8,8,0); self.assertEqual(packed[3],0xFF); self.assertEqual(sum(decode_cell(packed,8,8)[y][3] for y in range(8)),8)

    def test_byte_identical_generation_and_hash_failure(self):
        self.assertEqual(generate.generated(self.manifest,self.cps),generate.generated(self.manifest,self.cps))
        bad=copy.deepcopy(self.manifest); bad["sources"]["fusion8"]["subset_sha256"]="0"*64
        with self.assertRaisesRegex(ValueError,"hash mismatch"): generate.generated(bad,self.cps)

if __name__=="__main__": unittest.main()
