import copy, io, json, os, re, tarfile, tempfile, unittest, warnings, zipfile
from pathlib import Path
try:
    from . import generate
except ImportError:
    import generate

ARCHIVE_FILES={
 "fusion10":"fusion-pixel-font-10px-monospaced-bdf-v2026.07.01.zip",
 "fusion12":"fusion-pixel-font-12px-monospaced-bdf-v2026.07.01.zip",
 "wqy16b":"wqy-bitmapfont-bdf-0.7.0-4.tar.gz"}

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
            lambda m:m["profiles"]["CN_10"].update(cell=[10,13],top_offset=0),
            lambda m:m["profiles"]["CN_DEFAULT_10"].update(top_offset=1),
            lambda m:m["profiles"]["CN_12"].update(policy="fallback-only"),
            lambda m:m["profiles"]["CN_16"].update(source_advance=16)):
            bad=copy.deepcopy(self.manifest); mutate(bad)
            with self.assertRaises(ValueError): generate.codepoints(bad)

    def test_counts_order_and_fallback(self):
        self.assertEqual({k:len(v) for k,v in self.cps.items()},{"CN_DEFAULT_10":720,"CN_10":1,"CN_12":34,"CN_16":34})
        for name,v in self.cps.items():
            self.assertEqual(v[0],0x25A1); self.assertEqual(v[1:],sorted(set(v[1:])))
            self.assertEqual(set(v)&set(range(0x20,0x7F)),set(range(0x20,0x7F)) if name=="CN_DEFAULT_10" else set())

    def test_real_release_archives_to_checked_in_subsets(self):
        archive_dir=os.environ.get("EDGETX_CN_FONT_ARCHIVE_DIR")
        if not archive_dir: self.skipTest("EDGETX_CN_FONT_ARCHIVE_DIR is not set")
        zip_dir=Path(archive_dir)
        for key in generate.SOURCE_KEYS:
            path=zip_dir/ARCHIVE_FILES[key]; self.assertTrue(path.is_file(),path)
            source=self.manifest["sources"][key]
            wanted=sorted(set().union(*(self.cps[name] for name,spec in generate.FONT_CONTRACT.items() if spec["source"]==key)))
            subset=generate.extract_one(path.read_bytes(),source,wanted)
            checked=(generate.ROOT/source["subset"]).read_bytes()
            self.assertEqual(subset,checked); self.assertEqual(generate.sha256(subset),source["subset_sha256"])

    def test_synthetic_zip_archive_member_and_subset_chain(self):
        record="STARTCHAR box\nENCODING 9633\nSWIDTH 1000 0\nDWIDTH 8 0\nBBX 8 1 0 0\nBITMAP\n80\nENDCHAR\n"
        bdf=("STARTFONT 2.1\nSTARTPROPERTIES 2\nFONT_ASCENT 7\nFONT_DESCENT 1\nENDPROPERTIES\nCHARS 1\n"+record+"ENDFONT\n").encode()
        stream=io.BytesIO()
        with zipfile.ZipFile(stream,"w",zipfile.ZIP_DEFLATED) as archive: archive.writestr("font.bdf",bdf)
        blob=stream.getvalue(); source={"kind":"bdf-zip","zip_sha256":generate.sha256(blob),"member":"font.bdf","member_sha256":generate.sha256(bdf)}
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

    def test_synthetic_tar_archive_is_safe_and_hash_checked(self):
        record="STARTCHAR box\nENCODING 9633\nSWIDTH 1000 0\nDWIDTH 8 0\nBBX 8 1 0 0\nBITMAP\n80\nENDCHAR\n"
        bdf=("STARTFONT 2.1\nSTARTPROPERTIES 2\nFONT_ASCENT 7\nFONT_DESCENT 1\nENDPROPERTIES\nCHARS 1\n"+record+"ENDFONT\n").encode()
        def make_tar(extra_name=None):
            stream=io.BytesIO()
            with tarfile.open(fileobj=stream,mode="w:gz") as archive:
                info=tarfile.TarInfo("font.bdf"); info.size=len(bdf); archive.addfile(info,io.BytesIO(bdf))
                if extra_name:
                    bad=tarfile.TarInfo(extra_name); bad.size=1; archive.addfile(bad,io.BytesIO(b"x"))
            return stream.getvalue()
        blob=make_tar(); source={"kind":"bdf-tar-gz","archive_size":len(blob),"archive_sha256":generate.sha256(blob),"member":"font.bdf","member_sha256":generate.sha256(bdf)}
        self.assertEqual(generate.extract_one(blob,source,[0x25A1]),bdf)
        for mutation in (lambda s:s.update(archive_size=s["archive_size"]+1),lambda s:s.update(archive_sha256="0"*64),lambda s:s.update(member="missing.bdf"),lambda s:s.update(member_sha256="0"*64)):
            bad=copy.deepcopy(source); mutation(bad)
            with self.assertRaises(ValueError): generate.extract_one(blob,bad,[0x25A1])
        traversal=make_tar("../escape.bdf"); unsafe={**source,"archive_size":len(traversal),"archive_sha256":generate.sha256(traversal)}
        with self.assertRaisesRegex(ValueError,"unsafe tar member path"): generate.extract_one(traversal,unsafe,[0x25A1])
        invalid=b"not a tar.gz archive"; corrupt={**source,"archive_size":len(invalid),"archive_sha256":generate.sha256(invalid)}
        with self.assertRaisesRegex(ValueError,"invalid release tar.gz"): generate.extract_one(invalid,corrupt,[0x25A1])

    def test_wqy_bold_source_hash_and_license_contract(self):
        source=self.manifest["sources"]["wqy16b"]
        self.assertEqual(source["archive_sha256"],"E1A9BF2D4E608EAADA9822F58F33626204B665A4A60A353DCEB0C5FC09A75D40")
        self.assertEqual(source["member_sha256"],"26F493DC492BF64EB974E81C174BBCEDD2FF7FBD116428865992388B04A09042")
        subset=(generate.ROOT/self.manifest["sources"]["wqy16b"]["subset"]).read_bytes()
        for notice in (b"Developer: The WenQuanYi Project Contributors",b"Copyright: (C)2004-2006, The WenQuanYi Project",b"License  : GPL v2.0 (with font embedding exception)"):
            self.assertIn(notice,subset)

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
        for key in generate.SOURCE_KEYS:
            raw=(generate.ROOT/self.manifest["sources"][key]["subset"]).read_bytes()
            self.assertTrue(raw.startswith(b"STARTFONT ")); self.assertTrue(raw.endswith(b"ENDFONT\n"))
            wanted=sorted(set().union(*(self.cps[name] for name,spec in generate.FONT_CONTRACT.items() if spec["source"]==key)))
            self.assertEqual(generate.subset_bdf(raw,wanted),raw)
            self.assertEqual(len(generate.parse_bdf(raw)[2]),len(wanted))

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
            if name=="CN_DEFAULT_10":
                self.assertTrue(all(widths[i]==5 for i,cp in enumerate(cps) if 0x20<=cp<=0x7E))
                self.assertTrue(all(widths[i]==10 for i,cp in enumerate(cps) if not 0x20<=cp<=0x7E))
            else: self.assertTrue(all(x==width for x in widths))
            self.assertTrue(all(0<x<=width for x in widths))
            self.assertEqual(len(glyphs),len(cps)); self.assertTrue(all(len(g)==bpg for g in glyphs))
            for g in glyphs:
                cell=decode_cell(g,width,height)
                self.assertFalse(any(any(r) for r in cell[:top])); self.assertFalse(any(any(r) for r in cell[top+bh:]))
                if height%8:
                    mask=~((1<<(height%8))-1)&0xff; blocks=(height+7)//8
                    self.assertTrue(all((g[x*blocks+blocks-1]&mask)==0 for x in range(width)))

    def test_arrays_match_independent_bdf_reference_and_exact_scaling(self):
        parsed={key:generate.parse_bdf((generate.ROOT/self.manifest["sources"][key]["subset"]).read_bytes()) for key in generate.SOURCE_KEYS}
        for name,spec in generate.FONT_CONTRACT.items():
            asc,_,glyphs=parsed[spec["source"]]
            constants,cps,widths,arrays=parse_generated(name); scale=spec.get("scale",1); source_size=constants["BODY_HEIGHT"]//scale
            for i,cp in enumerate(cps):
                expected=reference_bdf(glyphs[cp],asc,source_size)
                expected=generate.scale_body(expected,scale)
                cell=decode_cell(arrays[i],constants["WIDTH"],constants["STORAGE_HEIGHT"])
                self.assertEqual([r[:] for r in cell[constants["TOP_OFFSET"]:constants["TOP_OFFSET"]+constants["BODY_HEIGHT"]]],expected)

    def test_subset_glyph_metrics_are_full_cell(self):
        for key in generate.SOURCE_KEYS:
            raw=(generate.ROOT/self.manifest["sources"][key]["subset"]).read_bytes()
            _,_,glyphs=generate.parse_bdf(raw)
            wanted=sorted(set().union(*(self.cps[name] for name,spec in generate.FONT_CONTRACT.items() if spec["source"]==key)))
            self.assertEqual(list(glyphs),wanted)
            source_width={"fusion10":10,"fusion12":12,"wqy16b":17}[key]
            expected={(source_width,0)} | ({(5,0)} if key=="fusion10" else set())
            self.assertEqual({glyph.dwidth for glyph in glyphs.values()},expected)

    def test_wqy_bold_glyphs_fit_sixteen_pixel_body(self):
        raw=(generate.ROOT/self.manifest["sources"]["wqy16b"]["subset"]).read_bytes()
        asc,desc,glyphs=generate.parse_bdf(raw)
        target={0x6CB9,0x95E8,0x5F00,0x5173,0x8B66,0x544A}
        self.assertTrue(target.issubset(glyphs))
        for cp,glyph in glyphs.items():
            width,_,xoff,_=glyph.bbx
            self.assertEqual((17,0),glyph.dwidth)
            self.assertGreaterEqual(xoff,0,hex(cp))
            self.assertLessEqual(xoff+width,16,hex(cp))
            body=generate.bdf_body(glyph,asc,desc,16,17)
            generate.validate_logical_width("CN_16",cp,body,16)

    def test_logical_width_accepts_pixels_within_width(self):
        body=[[0]*10 for _ in range(10)]; body[3][4]=1
        generate.validate_logical_width("CN_10",0x41,body,5)

    def test_logical_width_rejects_pixels_beyond_width_after_scaling(self):
        body=[[0]*10 for _ in range(10)]; body[3][5]=1
        with self.assertRaisesRegex(ValueError,r"CN_10 U\+0041: pixels beyond logical width 5"):
            generate.validate_logical_width("CN_10",0x41,body,5)

    def test_offsets_negative_x_clipping_and_real_12px_14row(self):
        g=generate.Glyph(1,(4,0),(3,4,-1,-1),[0xE0]*4,b"")
        self.assertEqual(generate.bdf_body(g,3,1,4),reference_bdf(g,3,4)); self.assertEqual(sum(map(sum,reference_bdf(g,3,4))),8)
        g2=generate.Glyph(2,(4,0),(2,4,1,1),[0xC0]*4,b"") # top=-2, clips upper two rows
        self.assertEqual(sum(map(sum,generate.bdf_body(g2,3,1,4))),4)
        raw=(generate.ROOT/self.manifest["sources"]["fusion12"]["subset"]).read_bytes(); asc,desc,glyphs=generate.parse_bdf(raw)
        real=next(g for g in glyphs.values() if g.bbx[1]==14)
        self.assertEqual(generate.bdf_body(real,asc,desc,12),reference_bdf(real,asc,12))

    def test_bdf_body_accepts_taller_global_cell(self):
        g=generate.Glyph(0x4E2D,(4,0),(4,4,0,0),[0xF0]*4,b"")
        self.assertEqual(generate.bdf_body(g,4,1,4),reference_bdf(g,4,4))
        with self.assertRaisesRegex(ValueError,"source metric mismatch"): generate.bdf_body(g,3,0,4)

    def test_bdf_body_source_advance_is_explicit_and_strict(self):
        g=generate.Glyph(0x4E2D,(17,0),(16,16,0,-2),[0xFFFF]*16,b"")
        self.assertEqual(256,sum(map(sum,generate.bdf_body(g,14,4,16,17))))
        with self.assertRaisesRegex(ValueError,"source metric mismatch"): generate.bdf_body(g,14,4,16)

    def test_full_ff_column_is_valid_data(self):
        body=[[0]*8 for _ in range(8)]
        for y in range(8): body[y][3]=1
        packed=generate.pack(body,8,8,0); self.assertEqual(packed[3],0xFF); self.assertEqual(sum(decode_cell(packed,8,8)[y][3] for y in range(8)),8)

    def test_byte_identical_generation_and_hash_failure(self):
        self.assertEqual(generate.generated(self.manifest,self.cps),generate.generated(self.manifest,self.cps))
        bad=copy.deepcopy(self.manifest); bad["sources"]["fusion10"]["subset_sha256"]="0"*64
        with self.assertRaisesRegex(ValueError,"hash mismatch"): generate.generated(bad,self.cps)

if __name__=="__main__": unittest.main()
