# Pixel art authoring for MAZE. Produces preview PNGs and src/sprites.h
import random, sys
from PIL import Image

PAL = {
 # stone / ruins
 '1':(40,44,46),'2':(66,72,70),'3':(92,98,92),'4':(118,124,114),'5':(146,150,136),'6':(172,174,158),
 # moss
 'm':(46,82,40),'M':(70,116,52),'l':(112,160,72),'L':(150,196,96),
 # floor
 'd':(34,32,40),'f':(44,42,50),'F':(54,52,60),'c':(64,62,70),
 # character
 'k':(28,24,32),'H':(92,58,30),'h':(132,88,48),'n':(56,34,20),'y':(74,46,26),
 's':(240,198,152),'S':(204,154,110),'w':(250,250,250),
 'r':(204,60,50),'R':(148,38,34),'j':(160,140,84),'J':(122,104,60),
 'b':(72,44,24),'g':(232,192,72),'p':(68,62,92),'P':(50,46,70),
 'o':(104,64,36),'O':(66,40,22),'B':(128,84,44),'v':(84,54,28),
 # items
 'W':(245,245,245),'X':(208,208,208),'Y':(150,150,150),'Z':(36,36,40),
 'G':(236,192,64),'T':(178,138,40),'K':(16,14,22),'A':(150,140,165),
 'q':(96,214,136),'Q':(60,150,96),
 'u':(178,98,238),'U':(118,60,186),'i':(232,186,255),
 'x':(210,210,222),'z':(232,72,62),'a':(84,76,96),
}
S = 32
def blank(): return [['.']*S for _ in range(S)]
def put(g,x,y,c):
    if 0<=x<S and 0<=y<S: g[y][x]=c
def rect(g,x,y,w,h,c):
    for yy in range(y,y+h):
        for xx in range(x,x+w): put(g,xx,yy,c)

# ---------------------------------------------------------------- floor
def floor():
    rnd=random.Random(7); g=blank()
    for y in range(S):
        for x in range(S):
            g[y][x] = 'f'
            r=rnd.random()
            if r<0.10: g[y][x]='F'
            elif r<0.14: g[y][x]='d'
    # slab seams (2x2 slabs, offset)
    for x in range(S): g[15][x]='d'; g[31][x]='d'
    for y in range(0,15): g[y][10]='d'
    for y in range(16,31): g[y][24]='d'
    # slab top highlight
    for x in range(S):
        if g[0][x]!='d': g[0][x]='c'
        if g[16][x]!='d': g[16][x]='c'
    return g

# ---------------------------------------------------------------- walls
def stone_block(g,x0,y0,w,h,rnd):
    for y in range(y0,y0+h):
        for x in range(x0,x0+w):
            r=rnd.random()
            c='3' if r<0.55 else ('4' if r<0.85 else '2')
            put(g,x,y,c)
    for x in range(x0,x0+w): put(g,x,y0,'5'); put(g,x,y0+h-1,'2')
    for y in range(y0,y0+h): put(g,x0,y,'5' if y<y0+h-1 else '2'); put(g,x0+w-1,y,'2')
    put(g,x0,y0,'6')
def mortar(g):
    for y in range(S):
        for x in range(S): g[y][x]='1'
def moss(g,rnd,amount,top_bias=True):
    for _ in range(amount):
        cx=rnd.randrange(S); cy=rnd.randrange(0,12 if top_bias else S)
        r=rnd.choice([2,2,3,3,4])
        for y in range(cy-r,cy+r+1):
            for x in range(cx-r,cx+r+1):
                if (x-cx)**2+(y-cy)**2 <= r*r + rnd.randint(-2,1):
                    if 0<=x<S and 0<=y<S:
                        put(g,x,y, rnd.choice('mMMl'))
        # drip
        if rnd.random()<0.6:
            dx=cx+rnd.randint(-r,r); L=rnd.randint(3,8)
            for y in range(cy+r, min(S,cy+r+L)): put(g,dx,y,'m' if y%2 else 'M')
    # light tips
    for y in range(S):
        for x in range(S):
            if g[y][x] in 'mM' and y>0 and g[y-1][x] not in 'mMlL' and rnd.random()<0.5: g[y][x]='L'
def crack(g,rnd,x,y,n):
    for _ in range(n):
        put(g,x,y,'1'); x+=rnd.choice([-1,0,1]); y+=1
def wall(v):
    rnd=random.Random(100+v); g=blank(); mortar(g)
    if v==1:   # running bond 2 rows
        stone_block(g,0,0,15,15,rnd); stone_block(g,16,0,16,15,rnd)
        stone_block(g,0,16,7,16,rnd); stone_block(g,8,16,16,16,rnd); stone_block(g,25,16,7,16,rnd)
        moss(g,rnd,3)
    elif v==2: # one big cracked slab
        stone_block(g,0,0,32,31,rnd); crack(g,rnd,12,3,14); crack(g,rnd,20,14,12)
        moss(g,rnd,2)
    elif v==3: # small bricks
        for row in range(4):
            off = 0 if row%2==0 else -5
            x=off
            while x<S:
                stone_block(g,max(0,x),row*8,min(10,S-max(0,x)) if x>=0 else 10+x-1,7,rnd); x+=11
        moss(g,rnd,3)
    elif v==4: # heavy moss
        stone_block(g,0,0,20,15,rnd); stone_block(g,21,0,11,15,rnd)
        stone_block(g,0,16,11,16,rnd); stone_block(g,12,16,20,16,rnd)
        moss(g,rnd,9)
    elif v==5: # carved glyph block
        stone_block(g,0,0,32,15,rnd); stone_block(g,0,16,15,16,rnd); stone_block(g,16,16,16,16,rnd)
        # glyph: eye/spiral rune on top block
        for (x,y) in [(12,4),(13,4),(14,4),(15,4),(16,4),(17,4),(18,4),(19,4),(11,5),(20,5),(11,6),(20,6),
                      (11,7),(20,7),(12,8),(13,8),(14,8),(17,8),(18,8),(19,8),(15,6),(16,6),(15,7),(16,7),(15,10),(16,10)]:
            put(g,x,y,'2')
        moss(g,rnd,2)
    return g

# ---------------------------------------------------------------- items (drawn over floor)
def start():
    g=blank()
    for y in range(S):
        for x in range(S):
            d=((x-15.5)**2+(y-15.5)**2)**0.5
            if 10.5<=d<=12: g[y][x]='q'
            elif 12<d<=13: g[y][x]='Q'
            elif 5<=d<=6: g[y][x]='Q'
    for (x,y) in [(15,7),(16,7),(15,24),(16,24),(7,15),(7,16),(24,15),(24,16)]: put(g,x,y,'q')
    return g
def exit_():
    g=blank(); rect(g,3,3,26,26,'G'); rect(g,4,4,24,24,'T'); rect(g,5,5,22,22,'K')
    for i,(y,w) in enumerate([(9,6),(14,11),(19,16),(24,21)]):
        rect(g,6,y,w,2,'A'); rect(g,6,y+2,w,1,'a')
    for (x,y) in [(3,3),(28,3),(3,28),(28,28)]: put(g,x,y,'W')
    return g
def key():
    g=blank()
    for y in range(S):
        for x in range(S):
            d=((x-10)**2+(y-16)**2)**0.5
            if 3.2<=d<=6.2: g[y][x]='W' if d<5.2 else 'Y'
    rect(g,15,15,13,3,'W'); rect(g,15,18,13,1,'Y')
    rect(g,21,19,2,4,'W'); rect(g,25,19,2,6,'W'); put(g,21,23,'Y'); put(g,25,25,'Y')
    put(g,8,13,'X'); put(g,9,13,'X')
    return g
def door():
    g=blank(); rect(g,3,2,26,29,'Y'); rect(g,4,3,24,27,'X')
    for x in (10,16,22): rect(g,x,3,1,27,'Y')
    for y in (7,24): rect(g,4,y,24,2,'A')
    for (x,y) in [(6,8),(25,8),(6,25),(25,25)]: put(g,x,y,'W')
    rect(g,14,12,4,4,'Z'); rect(g,15,16,2,5,'Z'); rect(g,13,11,6,1,'Y')
    return g
def trap_reset():
    g=blank()
    for y in range(S):
        for x in range(S):
            d=((x-15.5)**2+(y-15.5)**2)**0.5
            if d<=2.5: g[y][x]='i'
            elif 5<=d<=6.3: g[y][x]='u'
            elif 9.5<=d<=10.7: g[y][x]='U'
            elif 13<=d<=14: g[y][x]='U' if (x+y)%3 else '.'
    for (x,y) in [(15,4),(16,4),(27,15),(27,16),(15,27),(16,27),(4,15),(4,16)]: put(g,x,y,'i')
    return g
def trap_penalty():
    g=blank(); rect(g,2,24,28,4,'a'); rect(g,2,24,28,1,'A')
    for cx in (6,13,20,27):
        for yy in range(12):
            half=yy//3
            rect(g,cx-half,12+yy,1+2*half,1,'x' if yy%4 else 'X')
        put(g,cx,12,'z'); put(g,cx,13,'z')
    return g
def rubble():
    rnd=random.Random(55); g=blank()
    for _ in range(14):
        x=rnd.randrange(3,27); y=rnd.randrange(6,28); w=rnd.randint(2,5); h=rnd.randint(2,3)
        rect(g,x,y,w,h,'3'); rect(g,x,y,w,1,'5'); rect(g,x,y+h-1,w,1,'2')
        if rnd.random()<0.3: put(g,x+1,y,'M')
    return g

TILES = [floor()] + [wall(v) for v in range(1,6)] + [start(), exit_(), key(), door(), trap_reset(), trap_penalty(), rubble()]
TILE_NAMES = ['FLOOR','WALL1','WALL2','WALL3','WALL4','WALL5','START','EXIT','KEY','DOOR','TRAP_RESET','TRAP_PENALTY','RUBBLE']

# ---------------------------------------------------------------- adventurer (front/back symmetric halves)
FRONT_HALF = [
"................",
"................",
"...........kkkkk",
"..........khhhhh",
".........khhhhhh",
".........khhhhhh",
".........knnnnnn",
"....kkkkkHHHHHHH",
"...kHHHHHHHHHHHH",
"....kkkkkkyyyyyy",
".........kyyssss",
".........kysssss",
".........ksskwss",
".........ksskkss",
".........kSsssss",
"..........kSssss",
"..........kRrrrr",
".......kkkrrrrrr",
"......kjjjjRrrrj",
".....kjjjjjjjRrj",
".....kjJjjjjjjjj",
".....kjJjjjjjjjj",
".....kssJjjjjjjj",
".....kssbbbbbbgg",
"......kkjjjjjjjj",
".......kpppppppp",
]
BACK_HALF = [
"................",
"................",
"...........kkkkk",
"..........khhhhh",
".........khhhhhh",
".........khhhhhh",
".........knnnnnn",
"....kkkkkHHHHHHH",
"...kHHHHHHHHHHHH",
"....kkkkkkyyyyyy",
".........kyyyyyy",
".........kyyyyyy",
".........kyyyyyy",
".........kyyyyyy",
".........kSyyyyy",
"..........kSssss",
"..........kRrrrr",
".......kkkrrrrrr",
"......kjjjvBBBBB",
".....kjjjjvBBBBB",
".....kjJjjvBhBBB",
".....kjJjjvBBBBB",
".....kssJjvBBBBB",
".....kssbbvbBBBB",
"......kkjjjkkkkk",
".......kpppppppp",
]
LEG_HALF = [  # rows 26..31 (left leg)
"........kpppppkk",
"........kpppppk.",
"........kOoook..",
".......kooooook.",
".......kkkkkkkk.",
"................",
]
def mirror(half): return half + half[::-1]
def legs_row(i, offL, offR):
    def leg(j):
        if j < 0: return LEG_HALF[0]
        return LEG_HALF[j] if j<len(LEG_HALF) else "................"
    L = leg(i - offL); R = leg(i - offR)
    # rows above legs when lifted: keep pants continuation
    return L + R[::-1]
def frame_sym(top, step):
    rows = [mirror(r) for r in top]
    offL, offR = {0:(0,0),1:(-2,0),2:(0,-2)}[step]
    for i in range(6): rows.append(legs_row(i, offL, offR))
    return rows

SIDE_TOP = [
"................................",
"................................",
"............kkkkkkkk............",
"...........khhhhhhhhk...........",
"..........khhhhhhhhhhk..........",
"..........khhhhhhhhhhk..........",
"..........knnnnnnnnnnk..........",
"........kkkHHHHHHHHHHHkkkkk.....",
".......kHHHHHHHHHHHHHHHHHHHk....",
"........kkkyyyyyyyssskkkkkk.....",
"..........kyyyyysssssk..........",
"..........kyyyysssssssk.........",
"..........kyyysssskwsk..........",
"..........kyyysssskksssk........",
"...........kyysssssssk..........",
"............kSsssssk............",
"...........kRrrrrrrk............",
".........kBkrrrrrrrrk...........",
"........kBBBkjjjjjjjk...........",
"........kBhBkjjjjjjjk...........",
"........kBBBkjjjjsjjk...........",
"........kBBBkjjjjsjjk...........",
"........kBBBkjjjjssjk...........",
"........kkBBkbbbbbgbk...........",
"..........kkkjjjjjjjk...........",
"............kppppppk............",
]
SIDE_LEGS = {
0:["............kpppppk.............",
   "............kpppppk.............",
   "............kOoook..............",
   "............kooooooook..........",
   "............kkkkkkkkkk..........",
   "................................"],
1:["...........kppk.kppk............",
   "..........kppk...kppk...........",
   ".........kOok.....kOok..........",
   ".........koooook...kooooook.....",
   ".........kkkkkkk...kkkkkkkk.....",
   "................................"],
2:["............kppkppk.............",
   "............kpppppk.............",
   "............kOokook.............",
   "...........kooooooook...........",
   "...........kkkkkkkkkk...........",
   "................................"],
}
def frame_side(step, flip):
    rows = SIDE_TOP + SIDE_LEGS[step]
    return [r[::-1] for r in rows] if flip else list(rows)

PLAYER = []  # rows: down, up, left, right; cols: idle, stepL, stepR
for step in range(3): PLAYER.append(('down',step,frame_sym(FRONT_HALF,step)))
for step in range(3): PLAYER.append(('up',step,frame_sym(BACK_HALF,step)))
for step in range(3): PLAYER.append(('left',step,frame_side(step,True)))
for step in range(3): PLAYER.append(('right',step,frame_side(step,False)))

def check(name,g):
    rows = [''.join(r) if isinstance(r,list) else r for r in g]
    assert len(rows)==S, (name,len(rows))
    for i,r in enumerate(rows):
        assert len(r)==S, (name,i,len(r),r)
        for ch in r: assert ch=='.' or ch in PAL, (name,i,ch)
    return rows

tiles=[check(TILE_NAMES[i],t) for i,t in enumerate(TILES)]
players=[check(f'{d}{s}',f) for d,s,f in PLAYER]

def img_from(rows):
    im=Image.new('RGBA',(S,S),(0,0,0,0))
    for y,r in enumerate(rows):
        for x,ch in enumerate(r):
            if ch!='.': im.putpixel((x,y),PAL[ch]+(255,))
    return im
# previews
sheet=Image.new('RGBA',(S*len(tiles),S),(0,0,0,0))
for i,t in enumerate(tiles): sheet.paste(img_from(t),(i*S,0))
psheet=Image.new('RGBA',(S*3,S*4),(0,0,0,0))
for i,p in enumerate(players): psheet.paste(img_from(p),((i%3)*S,(i//3)*S))
out=sys.argv[1]
sheet.save(out+'/tiles.png'); psheet.save(out+'/player.png')
# composite preview: items over floor, wall grid sample, player on floor
prev=Image.new('RGBA',(S*13,S*2),(20,18,26,255))
fl=img_from(tiles[0])
for i,t in enumerate(tiles):
    if i>=6 or i==0: prev.paste(fl,(i*S,0),fl)
    im=img_from(t); prev.paste(im,(i*S,0),im)
for i,p in enumerate(players):
    prev.paste(fl,(i*S,S),fl); im=img_from(p); prev.paste(im,(i*S,S),im)
prev.resize((prev.width*4,prev.height*4),Image.NEAREST).save(out+'/preview.png')

# C++ header
with open(sys.argv[2],'w') as f:
    f.write('// sprites.h  (자동 생성: tools/sprites.py)\n// 32x32 픽셀 아트 데이터. 문자 하나가 픽셀 하나이며 \'.\'은 투명이다.\n#pragma once\n\n')
    f.write('constexpr int SPRITE_PX = 32;\n')
    f.write('enum TileSprite { ' + ', '.join('T_'+n for n in TILE_NAMES) + ', T_COUNT };\n')
    f.write('constexpr int WALL_VARIANTS = 5;\n\n')
    f.write('struct PaletteEntry { char ch; unsigned char r, g, b; };\n')
    f.write('const PaletteEntry SPRITE_PALETTE[] = {\n')
    for ch,(r,g,b) in PAL.items(): f.write(f"    {{'{ch}', {r}, {g}, {b}}},\n")
    f.write('};\n\n')
    f.write(f'const char* const TILE_PIXELS[T_COUNT][{S}] = {{\n')
    for name,t in zip(TILE_NAMES,tiles):
        f.write(f'    {{ // {name}\n')
        for r in t: f.write(f'        "{r}",\n')
        f.write('    },\n')
    f.write('};\n\n')
    f.write('// 캐릭터: 행 = 방향(아래, 위, 왼쪽, 오른쪽), 열 = 동작(정지, 왼발, 오른발)\n')
    f.write(f'const char* const PLAYER_PIXELS[12][{S}] = {{\n')
    for (d,s,_),p in zip(PLAYER,players):
        f.write(f'    {{ // {d} step{s}\n')
        for r in p: f.write(f'        "{r}",\n')
        f.write('    },\n')
    f.write('};\n')
print('ok', len(tiles), len(players))
