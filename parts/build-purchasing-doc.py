"""Regenerate the purchasing HTML, CSV and downloaded-resource manifest."""
from pathlib import Path
import csv
import hashlib
import json
import re
import markdown

ROOT = Path(__file__).resolve().parent
md = (ROOT / 'purchasing-kit.md').read_text()

# Preserve the distinction between candidate listings and qualified purchases.
core = [
    ('K01', 'ESP32-S3 SuperMini ESP32-S3FH4R2', '1-10', '1005012191736844', 'Exact module drawing needed', 'https://docs.waveshare.com/ESP32-S3-Zero/Resources-And-Documents', 'Waveshare reference only; exact SuperMini library not verified', 'Core'),
    ('K02', 'INMP441 microphone breakout', '1/5/10', '1005009006830769', 'Chip datasheet only; module drawing needed', 'https://invensense.tdk.com/wp-content/uploads/2015/02/INMP441.pdf', 'Not verified', 'Core'),
    ('K03', 'PCM5102A DAC module', '1-10', '1005010369195534', 'Chip datasheet only; module drawing needed', 'https://www.ti.com/lit/ds/symlink/pcm5102a.pdf', 'Chip CAD on TI; module CAD not verified', 'Core'),
    ('K04', 'MAX98357A I2S amplifier module', '1/10', '1005009759175798', 'Chip datasheet only; confirm A suffix and module drawing', 'https://www.analog.com/en/products/MAX98357A.html', 'Chip CAD on ADI; Adafruit module CAD is reference only', 'Core'),
    ('K05', 'SSD1306 0.96 inch 128x64 4-pin I2C OLED', '10', '1005008169257099', 'Exact module drawing needed', '', 'Not verified', 'Core'),
    ('K06', 'Bare EC11 encoder with push switch', '10', '1005005239756119', 'Exact MPN and seller drawing needed', '', 'Generic EC11 cannot establish exact footprint', 'Core'),
    ('K07', 'Bare ALPS RKJXV1224005 joystick', '10-100', '1005011995304794', 'Exact MPN drawings available; seller identity and part match unverified', 'https://tech.alpsalpine.com/e/products/detail/RKJXV1224005/', 'Drawings saved; no verified native DipTrace or exact STEP', 'Core'),
    ('K08', 'MPU6050 GY-521 IMU breakout', '10', '1005011896641210', 'Chip datasheet only; module drawing needed', 'https://product.tdk.com/system/files/dam/doc/product/sensor/mortion-inertial/imu/data_sheet/mpu-6000-datasheet1.pdf', 'Not verified for GY-521 lot', 'Motion option included'),
    ('K09', 'VL53L0X direct I2C sensor breakout', '1/10', '1005010777247203', 'Mixed variants; exact module drawing and interface needed', 'https://www.st.com/resource/en/datasheet/vl53l0x.pdf', 'Not verified for candidate module', 'Distance option included'),
    ('K10', '40mm round speaker 4 ohm 3W', '1/2/10', '1005009533204416', 'Full depth and mounting drawing needed', '', 'Not verified', 'Core'),
    ('K11', 'Stereo headphone amp; TDA1308 candidate', '1/10', '1005009791615188', 'Schematic and dimensions needed; headphone suitability unverified', 'https://www.nxp.com/products/TDA1308', 'Not verified', 'Headphones; design hold'),
]
fields = ['id','part','scope','qty_per_kit','qty_for_10_kits','advertised_pack_options','target_pieces_per_pack','target_number_of_packs','purchase_url','link_type','listing_evidence_url','documentation_status','documentation_url','cad_status','choice_status','shipping_status','verified_lot_price','currency','verified_shipping_cost','verification_date','notes']
rows = []
for ident, part, packs, item, docstatus, docurl, cad, scope in core:
    rows.append(dict(zip(fields, [ident,part,scope,1,10,packs,10,1,f'https://www.aliexpress.com/item/{item}.html','Candidate product listing',f'https://www.pricearchive.org/aliexpress.com/item/{item}',docstatus,docurl,cad,'Unverified','Unverified','','','','2026-09-09','No purchase qualification. Select 10-piece option. Read purchasing-kit.md for compatibility and holds.'])))

supplementary = [
    ('S01','Encoder knob',1,10,10,1,'https://www.aliexpress.com/item/33003636792.html','Candidate product listing','https://alitools.io/en/showcase/10pcs-6mm-shaft-hole-amplifier-knob-for-encoder-potentiometer-knobs-33003636792','Match exact shaft profile and bore depth'),
    ('S02','Joystick cap',1,10,10,1,'https://www.aliexpress.com/w/wholesale-10pcs-joystick-thumbstick-cap.html','Search only','','Only if not included; match ALPS lever'),
    ('S03','Male header strip 1x40 2.54mm',2,20,10,2,'https://www.aliexpress.com/w/wholesale-10pcs-1x40-2.54-male-header.html','Search only','','Allowance; final cuts/included headers unknown'),
    ('S04','Female socket strip 1x40 2.54mm',2,20,10,2,'https://www.aliexpress.com/item/1005013054690432.html','Candidate product listing','https://www.pricearchive.org/aliexpress.com/item/1005013054690432','Allowance; verify height and cutting losses'),
    ('S05','USB-A to USB-C data cable',1,10,10,1,'https://www.aliexpress.com/w/wholesale-10pcs-usb-a-usb-c-data-cable.html','Search only','','Require data conductors; choose cable length'),
    ('S06','Regulated 5V USB supply',1,10,10,1,'https://www.aliexpress.com/w/wholesale-10pcs-5v-2a-usb-power-supply.html','Search only','','Conditional accessory; 2A provisional budget; plug/destination TBD'),
    ('S07','Speaker connector mating pair with leads',1,10,10,1,'https://www.aliexpress.com/w/wholesale-10sets-jst-ph-2.0-2pin-wire.html','Search only','','Conditional; match pitch and current; may use amp terminal block instead'),
    ('S08','3.5mm stereo headphone socket',1,10,10,1,'https://www.aliexpress.com/w/wholesale-10pcs-3.5mm-stereo-pcb-jack.html','Search only','','Only if absent from headphone amp; exact MPN/footprint TBD'),
    ('S09','Internal wiring set',1,10,10,1,'https://www.aliexpress.com/w/wholesale-10pcs-dupont-jumper-wire-set.html','Search only','','Signal and power wiring lengths/gauges TBD'),
    ('S10','Assembled carrier PCB',1,10,10,1,'','Custom manufacture','','Design hold; no production files or final assembly BOM'),
    ('S11','4.7k I2C pull-up resistors',2,20,100,1,'https://www.aliexpress.com/w/wholesale-100pcs-4.7k-resistor.html','Search only','','Conditional allowance; check onboard pull-ups; package TBD'),
    ('S12','Supply/ADC/filter passives','TBD','TBD','','','','Design hold','','Values/packages/counts depend on carrier schematic'),
    ('S13','Printed enclosure and lid pair',1,10,10,1,'','Custom manufacture','','Select body and revise fit before printing'),
    ('S14','Lid screws',4,40,100,1,'https://www.aliexpress.com/w/wholesale-100pcs-m2-self-tapping-screws.html','Search only','','Handheld minimum only; length/type and additional module fixings TBD'),
    ('S15','Mic/speaker gasket set',1,10,10,1,'','Custom cut','','Dimensions depend on final parts and enclosure'),
    ('B01','Protected 1S battery',1,10,10,1,'https://www.aliexpress.com/w/wholesale-10pcs-3.7v-lipo-battery-protected.html','Search only','','Battery design hold; capacity, size, charge current and polarity TBD'),
    ('B02','Charger with appropriate power-path management',1,10,10,1,'https://www.aliexpress.com/w/wholesale-lipo-power-path-charger-5v-boost.html','Search only','','Battery design hold; TP4056 alone is not complete system power'),
    ('B03','5V boost converter',1,10,10,1,'https://www.aliexpress.com/w/wholesale-lipo-power-path-charger-5v-boost.html','Search only','','Battery design hold; omit if integrated into qualified power assembly'),
    ('B04','Power switch',1,10,10,1,'https://www.aliexpress.com/w/wholesale-10pcs-slide-switch.html','Search only','','Battery design hold; rating and wiring depend on power path'),
    ('B05','Battery mating harness',1,10,10,1,'','Design hold','','Exact battery connector and polarity TBD'),
]
for ident,part,per,total,pack,npack,url,kind,evidence,note in supplementary:
    rows.append(dict(zip(fields,[ident,part,'Portable power' if ident.startswith('B') else 'Assembly / conditional accessory',per,total,'Unverified',pack,npack,url,kind,evidence,'Exact item and dimension documentation needed','','Not verified','Unverified','Unverified','','','','2026-09-09',note])))
with (ROOT/'purchasing-kit.csv').open('w', newline='') as f:
    w=csv.DictWriter(f,fieldnames=fields)
    w.writeheader()
    w.writerows(rows)

css='''
@page { size: A4; margin: 16mm 14mm; }
* { box-sizing: border-box; }
body { font: 15px/1.55 system-ui, sans-serif; color: #182b37; background: #eef2f4; margin: 0; }
main { max-width: 1150px; margin: 36px auto; background: white; padding: 44px 50px; border-top: 7px solid #007f78; }
h1 { font-size: 32px; line-height: 1.15; letter-spacing: -.8px; margin-top: 0; }
h2 { font-size: 22px; margin-top: 36px; border-bottom: 1px solid #d7e1e4; padding-bottom: 8px; }
a { color: #006c77; text-decoration-thickness: 1px; text-underline-offset: 2px; overflow-wrap: anywhere; }
table { border-collapse: collapse; width: 100%; font-size: 13px; line-height: 1.45; margin: 18px 0; }
th { background: #e6f0f0; text-align: left; }
th, td { border: 1px solid #d6e0e4; padding: 9px; vertical-align: top; }
tr:nth-child(even) td { background: #f6f8fa; }
blockquote { margin: 20px 0; padding: 8px 20px; border-left: 4px solid #00857b; background: #f1f7f6; }
code { font-size: .9em; }
li { margin: 7px 0; }
.nav { font-size: 13px; margin-bottom: 24px; }
@media print {
  body { background: white; font-size: 10pt; line-height: 1.42; }
  main { margin: 0; padding: 0; max-width: none; border: 0; }
  h1 { font-size: 23pt; } h2 { font-size: 15pt; break-after: avoid; margin-top: 20pt; }
  table { font-size: 8.1pt; } th, td { padding: 5pt; }
  tr, blockquote { break-inside: avoid; } thead { display: table-header-group; }
  .nav { display: none; }
}
'''
body=markdown.markdown(md,extensions=['tables','fenced_code','sane_lists'])
html=f'''<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ESP32-S3 audio kit — purchasing shortlist</title><style>{css}</style></head><body><main><div class="nav"><a href="purchasing-kit.pdf">PDF</a> · <a href="purchasing-kit.csv">Editable CSV</a> · <a href="purchasing-kit.md">Markdown source</a></div>{body}</main></body></html>'''
(ROOT/'purchasing-kit.html').write_text(html)

sources={
'PCM5102A-chip-datasheet.pdf':('https://www.ti.com/lit/ds/symlink/pcm5102a.pdf','Chip datasheet; does not dimension candidate DAC module'),
'Waveshare-ESP32-S3-Zero-dimensions.jpg':('https://files.waveshare.com/wiki/ESP32-S3-Zero/ESP32-S3-Zero-2D-size.jpg','Waveshare board only; generic SuperMini match not established'),
'Waveshare-ESP32-S3-Zero-v2.stp':('https://files.waveshare.com/wiki/ESP32-S3-Zero/ESP32-S3-Zero_v2-138eb3753cf14afa3252058c380ca416.stp','Waveshare v2 manufacturer model; format/header checked, no mechanical fit validation'),
'ALPS-EC11E15244G1-dimensions.gif':('https://tech.alpsalpine.com/cms.media/product_detail_fig_ec11_d_82_en_2cca8e7a94.gif','Exact ALPS part only; not generic EC11 lot'),
'ALPS-EC11E15244G1-mounting.gif':('https://tech.alpsalpine.com/cms.media/product_detail_fig_ec11_d_83_en_1f3da99620.gif','Exact ALPS part only; mounting-side view'),
'ALPS-RKJXV1224005-dimensions.gif':('https://tech.alpsalpine.com/cms.media/product_detail_fig_rkjxk_d_52_en_91144bd9bb.gif','RKJXV1224005; supplier must confirm exact part'),
'ALPS-RKJXV1224005-mounting.gif':('https://tech.alpsalpine.com/cms.media/product_detail_fig_rkjxk_d_53_en_9117e0a2e7.gif','RKJXV1224005; mounting-side view'),
'ALPS-RKJXV1224005-lever.gif':('https://tech.alpsalpine.com/cms.media/product_detail_fig_rkjxk_d_54_en_8c918778f3.gif','RKJXV1224005 cap-interface drawing'),
}
manifest=[]
for name,(url,note) in sources.items():
    p=ROOT/'research-2026-09-09'/name
    if p.exists():
        manifest.append(dict(file=name,source_url=url,retrieved='2026-09-09',bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest(),applicability=note))
(ROOT/'research-2026-09-09'/'sources.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(f'Generated HTML, {len(rows)} CSV rows and {len(manifest)} resource records.')

# The current seven-part kit is a separate, compact document.
minimal_md = ROOT / 'minimal-kit.md'
if minimal_md.exists():
    minimal_body = markdown.markdown(minimal_md.read_text(), extensions=['tables', 'sane_lists'])
    compact_css = css + '\n@media print { body { font-size: 9pt; } h1 { font-size: 21pt; } table { font-size: 8.3pt; } }'
    (ROOT / 'minimal-kit.html').write_text(f'<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Minimal audio kit — 7 parts</title><style>{compact_css}</style></head><body><main>{minimal_body}</main></body></html>')
    core_by_id = {r['id']: r for r in rows}
    minimal_rows = []
    for i, key in enumerate(['K01', 'K03', 'K07', 'K06', 'K02', 'K05', 'K08'], 1):
        row = dict(core_by_id[key])
        row.update(id=f'M{i:02d}', scope='Current seven-part kit', notes='One per kit; target one lot of 10. Exact selected variant, headers, shipping and drawing match remain unverified.')
        if key == 'K03':
            row.update(part='PCM5102A I2S DAC PURPLE board with audio jack', documentation_status='Chip datasheet available; exact purple board revision/drawing unverified')
        elif key == 'K07':
            row.update(part='KY-023-style joystick BOARD with fitted pins and cap', advertised_pack_options='1/5/10', purchase_url='https://www.aliexpress.com/item/1005008298413198.html', listing_evidence_url='https://ms.pricearchive.org/aliexpress.com/item/1005008298413198', documentation_status='Joy-IT module documentation is reference only; exact seller PCB drawing and fitted pins unverified', documentation_url='https://www.joy-it.net/en/products/COM-KY023JM', cad_status='Exact module footprint/STEP not verified; bare ALPS drawings do not apply')
        elif key == 'K02':
            row.update(part='Round SIX-PIN I2S microphone board; INMP441 assumed', documentation_status='Chip datasheet only; round six-pin variant and board dimensions unverified')
        minimal_rows.append(row)
    with (ROOT / 'minimal-kit.csv').open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerows(minimal_rows)
    print('Generated minimal HTML and seven-part CSV.')
