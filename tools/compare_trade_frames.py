"""Make private, labeled original/native comparison images; not a fidelity score."""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('native',type=Path)
    p.add_argument('original',type=Path)
    p.add_argument('--crop',type=int,nargs=4,required=True,metavar=('L','T','R','B'))
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    private=Path(__file__).resolve().parents[1]/'.local'
    if not a.output.resolve().is_relative_to(private.resolve()):p.error('output must remain private')
    original=Image.open(a.original).convert('RGB').crop(a.crop).resize((768,576),Image.Resampling.NEAREST)
    native=Image.open(a.native).convert('RGB').resize((768,576),Image.Resampling.NEAREST)
    pair=Image.new('RGB',(1536,600),'#242424');pair.paste(original,(0,24));pair.paste(native,(768,24))
    draw=ImageDraw.Draw(pair);draw.text((8,6),'Original no$psx (manual client crop)',fill='white')
    draw.text((776,6),'Native C controller / C++ host (same teams, independent animation phase)',fill='white')
    a.output.parent.mkdir(parents=True,exist_ok=True);pair.save(a.output)
    for name in a.native.parent.glob('*.ppm'):
        Image.open(name).resize((768,576),Image.Resampling.NEAREST).save(name.with_suffix('.png'))
    print(a.output)
if __name__=='__main__':main()
