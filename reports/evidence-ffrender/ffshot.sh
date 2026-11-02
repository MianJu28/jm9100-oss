#!/bin/bash
# 用法: ffshot.sh <profile> <page> <out-prefix> [extra env...]
PROF="$1"; PAGE="$2"; OUT="$3"; shift 3
python3 -c "
import os
print(' '.join(p for p in filter(str.isdigit,os.listdir('/proc')) if os.path.exists('/proc/'+p+'/exe') and 'firefox' in os.readlink('/proc/'+p+'/exe')))
" > ${OUT}.before
env "$@" DISPLAY=:0 setsid /usr/lib/firefox/firefox --no-remote -profile "$PROF" -allow-downgrade "$PAGE" > ${OUT}.log 2>&1 &
sleep 11
DISPLAY=:0 ffmpeg -loglevel error -f x11grab -video_size 1920x1080 -i :0 -frames:v 1 -y ${OUT}.png 2>/dev/null
read W H X Y <<< "$(DISPLAY=:0 /tmp/findwin2 2>/dev/null | grep -m1 'DIAG' | python3 -c "
import sys,re
t=sys.stdin.read()
m=re.search(r'(\d+)x(\d+)\s+@\s*(\d+),\s*(\d+)', t)
print(m.group(1), m.group(2), m.group(3), m.group(4)) if m else print('0 0 0 0')
")"
python3 - "$OUT" "$W" "$H" "$X" "$Y" <<'PY'
import subprocess, collections, sys
out,w,h,x,y=sys.argv[1],int(sys.argv[2]),int(sys.argv[3]),int(sys.argv[4]),int(sys.argv[5])
raw=subprocess.run(['ffmpeg','-loglevel','error','-i',out+'.png','-vf',f'crop={w}:{h}:{x}:{y}','-f','rawvideo','-pix_fmt','rgb24','-'],capture_output=True).stdout
px=[raw[i:i+3] for i in range(0,len(raw),3)]
def cls(q):
    r,g,b=q
    if r<16 and g<16 and b<16: return '黑'
    if r>200 and g<60 and b<60: return '红'
    if g>200 and r<60 and b<60: return '绿方块'
    if b>200 and r<60 and g<60: return '蓝'
    if r>200 and g>200 and b<60: return '黄'
    if r>200 and g>200 and b>200: return '白'
    if abs(r-32)<48 and abs(g-128)<48 and abs(b-32)<48: return '页面绿#208020'
    return '其它'
c=collections.Counter(cls(q) for q in px); tot=len(px)
print('  ' + '  '.join(f"{k}:{v}({v/tot*100:.1f}%)" for k,v in c.most_common(9)))
PY
python3 -c "
import os,signal
before=set(open('${OUT}.before').read().split())
for pid in filter(str.isdigit,os.listdir('/proc')):
    if pid in before: continue
    try: exe=os.readlink('/proc/'+pid+'/exe')
    except Exception: continue
    if 'firefox' in exe:
        try: os.kill(int(pid),signal.SIGTERM)
        except Exception: pass
"
