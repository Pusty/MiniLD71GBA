#include <stdio.h>
#include <tonc.h>
#include <string.h>


#include <maxmod.h> 
#include "soundbank.h"

#include "soundbank_bin.h"

#include "sprites.4obj.h"
#include "screen.8bmp.h"
#include "tilemap.tm.h"
#include "map01.map.h"
#include "map02.map.h"
#include "map03.map.h"
#include "loading.map.h"


#define INITIAL_SCREEN_OFFSET_X 0
#define INITIAL_SCREEN_OFFSET_Y 8*18

#define WORLD_SIZE_X 64
#define WORLD_SIZE_Y 32

#define AMOUNT_ENTITIES 16
#define AMOUNT_MAPS 3


#define ENTITY_NONE 0
#define ENTITY_ENEMY 1
#define ENTITY_PLATFORM 2
#define ENTITY_COIN 3
#define ENTITY_MP_PLAYER 4

#define SPRITE_PLAYER_STANDING_LEFT 0x00
#define SPRITE_PLAYER_MOVING0_LEFT  0x02
#define SPRITE_PLAYER_MOVING1_LEFT  0x04
#define SPRITE_PLAYER_INAIR_LEFT    0x06

#define SPRITE_PLAYER_STANDING_RIGHT 0x08
#define SPRITE_PLAYER_MOVING0_RIGHT  0x0A
#define SPRITE_PLAYER_MOVING1_RIGHT  0x0C
#define SPRITE_PLAYER_INAIR_RIGHT    0x0E

#define SPRITE_ENEMY_0 0x40
#define SPRITE_ENEMY_1 0x41
#define SPRITE_PLATFORM_0 0x42
#define SPRITE_PLATFORM_1 0x44
#define SPRITE_COIN_0 0x46
#define SPRITE_COIN_1 0x47


#define TILE_PLATFORM 0xB
#define TILE_PLATFORM2 0xC
#define TILE_COIN 0xD
#define TILE_ENEMY 0xE
#define TILE_ENEMY2 0xF



typedef struct ENTITY {
    int id;
    int x;
    int y;
    union {
       int start;
       int animation_tick;
       int mp_player_map;
    };
    int end;
    int velocity;
    int sprite;
    OBJ_ATTR * sprite_obj;
} ENTITY;


typedef struct PLAYER {
    int x; // x position
    int y; // y position
    int velocityX; // x velocity
    int velocityY; // y velocity
    int look_direction; // direction player is looking (for render)
    int is_moving; // is the player moving
    int in_moving; // how many ticks into moving
    int is_jumping; // is the player jumping currently
    int in_jumping; // how many ticks into jumping
    int is_on_ground; // is the player on the ground
    int sprite; // sprite of the player
    OBJ_ATTR * sprite_obj; // a pointer to the render object (GBA specific)
} PLAYER;


int scrollx; // moving the background X 
int scrolly; // moving the background Y
int playercamX; // moving the player on the screen

int freeze = true; //freeze for first input
int coins[AMOUNT_MAPS] = {0}; // coins collected
int animTick = 0; // global animation tick

int mapIndex = 0; // current map index
int nextMap = 0; // next map to load
const unsigned short* map; // pointer to current loaded flat map
int loading = 0; // is loading


int coinEntitiesIndex = 0;
ENTITY* coinEntities[7];

PLAYER player;
ENTITY mp_players[4];
ENTITY entities[AMOUNT_ENTITIES];



// map
OBJ_ATTR obj_buffer[128];
OBJ_AFFINE *obj_aff_buffer= (OBJ_AFFINE*)obj_buffer;



void initData() {
    memset32(entities, 0, (AMOUNT_ENTITIES*sizeof(ENTITY))/4);
    memset32(mp_players, 0, (4*sizeof(ENTITY))/4);
    coinEntitiesIndex = 0;
    player.sprite = SPRITE_PLAYER_STANDING_RIGHT;
    player.sprite_obj = &obj_buffer[0];
    player.velocityX = 0;
    player.velocityY = 0;
    player.look_direction = 1;
    player.is_moving = false;
    player.in_moving = 0;
    player.is_jumping = false;
    player.in_jumping = 0;
    player.is_on_ground = false;
    player.x = 16;
    player.y = 8*30;
    obj_set_attr(player.sprite_obj, ATTR0_SQUARE, ATTR1_SIZE_16, ATTR2_PALBANK(0) | player.sprite | ATTR2_PRIO(1)); 
}


void drawWorld() {
    
    (player.sprite_obj)->attr2= ((player.sprite_obj)->attr2&~ATTR2_ID_MASK) | ATTR2_ID(player.sprite&0xFF);
    obj_set_pos(player.sprite_obj, 112+playercamX, 96);

    int screenOffsetX = (INITIAL_SCREEN_OFFSET_X - playercamX - 96);
    screenOffsetX = (player.x+screenOffsetX-16);
    int screenOffsetY =  (player.y+INITIAL_SCREEN_OFFSET_Y+16);
    
    for(int e=0;e<AMOUNT_ENTITIES;e++) {
        if(entities[e].id == ENTITY_ENEMY) {
            if(entities[e].velocity == 1) 
                if((entities[e].x/5)%2==0)
                    entities[e].sprite = SPRITE_ENEMY_0 | 0x100;
                else
                    entities[e].sprite = SPRITE_ENEMY_1 | 0x100;
            else 
                if((entities[e].x/5)%2==0)
                    entities[e].sprite = SPRITE_ENEMY_0;
                else
                    entities[e].sprite = SPRITE_ENEMY_1;
        }else if(entities[e].id == ENTITY_PLATFORM) {
            if(animTick%40 >= 20)
                entities[e].sprite = SPRITE_PLATFORM_0;
            else
                entities[e].sprite = SPRITE_PLATFORM_1;
        }else if(entities[e].id == ENTITY_COIN) {
                if(entities[e].animation_tick>=50)
                    entities[e].sprite = SPRITE_COIN_0;
                else
                    entities[e].sprite = SPRITE_COIN_1;
        }
        
        if(entities[e].id != 0) {
           (entities[e].sprite_obj)->attr2= ((entities[e].sprite_obj)->attr2&~ATTR2_ID_MASK) | ATTR2_ID(entities[e].sprite&0xFF);
           
           // If 9th bit is set, flip the sprite on the x axis
           (entities[e].sprite_obj)->attr1 &= ~ATTR1_HFLIP;
           if((entities[e].sprite>>8)&1) (entities[e].sprite_obj)->attr1 |= ATTR1_HFLIP;
           
           obj_set_pos(entities[e].sprite_obj, entities[e].x - screenOffsetX, entities[e].y - screenOffsetY);
        }
    }
    
    for(int e=0;e<4;e++) {
        if(mp_players[e].id != 0) {
           (mp_players[e].sprite_obj)->attr2= ((mp_players[e].sprite_obj)->attr2&~ATTR2_ID_MASK) | ATTR2_ID(mp_players[e].sprite&0xFF);
           obj_set_pos(mp_players[e].sprite_obj, mp_players[e].x - screenOffsetX, mp_players[e].y - screenOffsetY);
        }
    }
}

void addEntity(int id,int x,int y,int st,int en) {
    for(int e=0;e<AMOUNT_ENTITIES;e++) {
        if(entities[e].id != ENTITY_NONE)continue;
        entities[e].id= id;
        entities[e].x = x;
        entities[e].y = y; //248-y;
        entities[e].start = st;
        entities[e].end = en;
        entities[e].velocity = 1;
        entities[e].sprite_obj = &obj_buffer[e+5]; // player + 4 mp players
        if(id == ENTITY_ENEMY) {
            entities[e].sprite = SPRITE_ENEMY_0;
            obj_set_attr(entities[e].sprite_obj, ATTR0_SQUARE, ATTR1_SIZE_8, ATTR2_PALBANK(0) | (entities[e].sprite&0xFF) | ATTR2_PRIO(1)); // enemy
        }else if(id == ENTITY_PLATFORM) {
            entities[e].sprite = SPRITE_PLATFORM_0;
            obj_set_attr(entities[e].sprite_obj, ATTR0_WIDE, ATTR1_SIZE_8, ATTR2_PALBANK(0) | (entities[e].sprite&0xFF) | ATTR2_PRIO(1));  // platform
        }else if(id == ENTITY_COIN) {
            entities[e].sprite = SPRITE_COIN_0;
            obj_set_attr(entities[e].sprite_obj, ATTR0_SQUARE, ATTR1_SIZE_8, ATTR2_PALBANK(0) | (entities[e].sprite&0xFF) | ATTR2_PRIO(1));  // coin
            coinEntities[coinEntitiesIndex++] = &entities[e];
        }
        
        return;
    }
}

void removeEntity(ENTITY* entity) {
    entity->id = ENTITY_NONE; 
    obj_hide(entity->sprite_obj);
}

void initWorld() {
    
    scrollx = INITIAL_SCREEN_OFFSET_X; // moving the background X 
    scrolly = INITIAL_SCREEN_OFFSET_Y; // moving the background Y
    playercamX = -96; // moving the player on the screen
    
    coins[mapIndex] = 0;
    
    animTick = 0;

    for(int x=0;x<WORLD_SIZE_X;x++) 
        for(int y=0;y<WORLD_SIZE_Y;y++) {
            int id = map[y*WORLD_SIZE_X+x];
            if(id== TILE_COIN) {
                addEntity(ENTITY_COIN ,x*8,(y) * 8 - 7  , 0, 0);
            }
            if(id == TILE_PLATFORM) {
                for(int x2=x+1;x2<WORLD_SIZE_X;x2++)  {
                    if(map[y*WORLD_SIZE_X+x2] == TILE_PLATFORM2) {
                        addEntity(ENTITY_PLATFORM, x*8,(y) * 8, x*8, (x2-1)*8);
                        break;
                    }
                }
            }
            if(id == TILE_ENEMY) {
                for(int x2=x+1;x2<WORLD_SIZE_X;x2++)  {
                    if(map[y*WORLD_SIZE_X+x2] == TILE_ENEMY2) {
                        addEntity(ENTITY_ENEMY, x*8,(y) * 8, x*8, x2*8);
                        break;
                    }
                }
            }
        }
}


int collisionWorld(int tempX, int tempY) {
    for(int x=0;x<WORLD_SIZE_X;x++) 
        for(int y=0;y<WORLD_SIZE_Y;y++) {
            if(map[y*WORLD_SIZE_X+x]==0)continue;
            if(map[y*WORLD_SIZE_X+x]==TILE_PLATFORM)continue;
            if(map[y*WORLD_SIZE_X+x]==TILE_COIN)continue;
            if(map[y*WORLD_SIZE_X+x]==TILE_ENEMY)continue;
            if(map[y*WORLD_SIZE_X+x]==TILE_PLATFORM2)continue;
            if(map[y*WORLD_SIZE_X+x]==TILE_ENEMY2)continue;
            if(tempX+9>=x*8 && tempX<=x*8+7 && tempY+15>=(y)*8 && tempY<=(y)*8+7) {
                //int id = map[y*WORLD_SIZE_X+x];
                //if(id == 4) {
                //    map[y*WORLD_SIZE_X+x] = 5;
                //}
                return true;
            }
            
            
        }
    return false;
    
}

int collisionEntities(int tempX, int tempY) {
    
    int returnValue = 0;
    
    for(int e=0;e<AMOUNT_ENTITIES;e++)
        if(entities[e].id == ENTITY_ENEMY) {
            if(tempX+9>=entities[e].x && tempX<=entities[e].x+7 && player.y+16==entities[e].y && player.in_jumping<=0) {
                player.is_jumping = true;
                mmEffect(SFX_JUMP2);
                player.in_jumping = 15;
                returnValue = true; // don't return early so you can pickup coins when on an enemy
            }  			
        }else if(entities[e].id == ENTITY_PLATFORM) {
            if(tempX+9>=entities[e].x && tempX<=entities[e].x+15 && player.y+16==entities[e].y && player.in_jumping<=0) {
                if((animTick&1) == 0) {
                    if(player.velocityX == 0)	{
                        player.velocityX = entities[e].velocity;
                    }else if(entities[e].velocity != 0) {
                        if(player.velocityX == entities[e].velocity)
                            player.velocityX += entities[e].velocity;
                        else
                            player.velocityX = player.velocityX;
                    }
                }
                returnValue = true; // don't return early so you can pickup coins when on a platform
            }
        }else if(entities[e].id == ENTITY_COIN) {
            if(tempX+9>=entities[e].x && tempX<=entities[e].x +7 && tempY+15>=entities[e].y && tempY<=entities[e].y+7) {
                removeEntity(&entities[e]); // pickup coin!
                for(int i=0;i<7;i++) {
                    if(&entities[e] == coinEntities[i])
                        coins[mapIndex] |= (1<<i);
                }
                mmEffect(SFX_COIN);
            }
        
        }
    return returnValue;
}

void handleVel() {
    int tempY = player.y + player.velocityY;
    
    //char str[32];
    //siprintf(str, "p %3d,%3d", tempY, 0);
    //nocash_puts(str);
    
    if(!collisionEntities(player.x, tempY) && tempY <= 240  && !collisionWorld(player.x,tempY)) {
        player.y=tempY;
        scrolly+=player.velocityY;
        player.is_on_ground = false;
    }else if(player.velocityY > 0 && !player.is_on_ground) {
        player.is_on_ground = true;
        mmEffect(SFX_LANDING);
    }
    
    int tempX = player.x + player.velocityX;
    if(tempX>=0 && !collisionWorld(tempX,player.y)) {
        player.x=tempX;
        if(player.x <= 8*14 || player.x >= (WORLD_SIZE_X*8)-(8*16))
            playercamX+=player.velocityX;
        else
            scrollx+=player.velocityX;
        if(player.is_moving && !player.is_jumping && player.is_on_ground) {
            player.in_moving++;
        }else
            player.in_moving = 0;
    }
    
    if(player.in_moving>=20) {
        player.in_moving=0;
    }
}
    

void loadMap(SCR_ENTRY* sbleft, SCR_ENTRY* sbright, const unsigned short* map) {
    
    for(int y=0;y<WORLD_SIZE_Y;y++) {
        for(int x=0;x<WORLD_SIZE_X;x++) {
            unsigned short id = map[x+y*WORLD_SIZE_X];
            
            if(id == TILE_COIN) id = 0;
            else if(id == TILE_ENEMY) id = 0;
            else if(id == TILE_PLATFORM) id = 0;
            else if(id == TILE_PLATFORM2) id = 0;
            else if(id == TILE_ENEMY2) id = 0;
            
            
            if(x < 32) {
                sbleft[0] = id;
                sbleft++;     
            }else {
                sbright[0] =  id;
                sbright++;      
            }
        }
    }
}


void setWorld(int index) {
    if(index == 0) {
        map = map01_mapMap;	
    }else if(index == 1) {
        map = map02_mapMap;	
    }else if(index == 2) {
        map = map03_mapMap;	
    }
    loadMap(&se_mem[30][0], &se_mem[31][0], map);
    initData();
    initWorld();
}


void handleEntities() {
    for(int e=0;e<AMOUNT_ENTITIES;e++)
        if(entities[e].id == ENTITY_ENEMY || entities[e].id == ENTITY_PLATFORM) {
            
            int distance = (entities[e].end-entities[e].start);
            int toveral  = (animTick/2) % (distance*2);
            if(toveral >= distance) {
                entities[e].x = entities[e].end - (toveral-distance);
                entities[e].velocity = -1;
            } else {
                entities[e].x = entities[e].start + (toveral);
                entities[e].velocity = 1;
            }
            
            //entities[e].x=entities[e].x+entities[e].velocity;
            //if(entities[e].x<=entities[e].start || entities[e].x >= entities[e].end) 
            //    entities[e].velocity = -entities[e].velocity;
        }else if(entities[e].id == ENTITY_COIN) {
            entities[e].animation_tick = (entities[e].animation_tick+1) % 100;
        }
}


#define PACKET_CONNECT   0x00
#define PACKET_PLAYER_X0 0x01
#define PACKET_PLAYER_X1 0x02
#define PACKET_PLAYER_Y0 0x03
#define PACKET_PLAYER_Y1 0x04
#define PACKET_PLAYER_SPRITE 0x05
#define PACKET_WORLD 0x06

#define PACKET_COINS0 0x07
#define PACKET_COINS1 0x08
#define PACKET_COINS2 0x09

#define PACKET_ANIMTICK0 0x0A
#define PACKET_ANIMTICK1 0x0B

int should_send_next_index = 0;

int link_is_master() {
    return !(REG_SIOCNT&SIOM_SLAVE);
}
int link_is_ready() {
    return (REG_SIOCNT&SIOM_CONNECTED);
}
int link_has_error() {
    return (REG_SIOCNT&SIOM_ERROR);
}
int link_is_sending() {
    return (REG_SIOCNT&SION_ENABLE);
}
int link_get_id() {
    return (REG_SIOCNT&SIOM_ID_MASK)>>SIOM_ID_SHIFT;
}

void link_init_player(int id) {
     mp_players[id].x = 16;
     mp_players[id].y = 8*30;
     mp_players[id].id = ENTITY_MP_PLAYER;
     mp_players[id].sprite_obj = &obj_buffer[1+id];
     mp_players[id].sprite = SPRITE_PLAYER_STANDING_RIGHT;
     obj_set_attr(mp_players[id].sprite_obj, ATTR0_SQUARE, ATTR1_SIZE_16, ATTR2_PALBANK((id%3)+1) | (mp_players[id].sprite&0xFF) | ATTR2_PRIO(1));  // multiplayer-player
}


void handle_packet(unsigned int id, unsigned short packet) {
    int packetData = packet&0xFF;
    int packetID   = (packet>>8)&0xFF;
    
    if(id > 4) return;
    if(packetID == 0xFF) return; // default data = 0xFFFF -> ERROR
    if(id == link_get_id()) return;
    
    if(mp_players[id].id == ENTITY_NONE)
        link_init_player(id);
    
    int coinWorldID = -1;
    
    switch(packetID) {
        case PACKET_PLAYER_X0:
            mp_players[id].x = (mp_players[id].x&(~0xFF)) | packetData;
            break;
        case PACKET_PLAYER_X1:
            mp_players[id].x = (mp_players[id].x&(~0xFF00)) | (packetData<<8);
            break;
        case PACKET_PLAYER_Y0:
            mp_players[id].y = (mp_players[id].y&(~0xFF)) | packetData;
            break;
        case PACKET_PLAYER_Y1:
            mp_players[id].y = (mp_players[id].y&(~0xFF00)) | (packetData<<8);
            break;
        case PACKET_PLAYER_SPRITE:
            mp_players[id].sprite = packetData;
            break;
        case PACKET_ANIMTICK0:
            if(id != 0) break; // only sync to master
            animTick = (animTick&(~0xFF)) | packetData; 
            break;
        case PACKET_ANIMTICK1:
            if(id != 0) break; // only sync to master
            animTick = (animTick&(~0xFF00)) | (packetData<<8); 
            break; 
        case PACKET_WORLD:
            mp_players[id].mp_player_map = packetData; // dd
            break; 
        case PACKET_COINS0:
            coinWorldID = 0;
            goto PACKET_COINS_PROCESS;
        case PACKET_COINS1:
            coinWorldID = 1;
            goto PACKET_COINS_PROCESS;
        case PACKET_COINS2:
            coinWorldID = 2;
            goto PACKET_COINS_PROCESS;
        PACKET_COINS_PROCESS:
            //if(mp_players[id].mp_player_map != mapIndex) break;
            if(mapIndex != coinWorldID) break;
            if(coins[coinWorldID] >= packetData) break;
            for(int i=0;i < 7;i++) {
                if(((packetData>>i)&1) && coinEntities[i]->id != ENTITY_NONE) {
                    removeEntity(coinEntities[i]); // pickup coin!
                    coins[coinWorldID] |= (1<<i);
                    mmEffect(SFX_COIN);
                }
            }
            break;
        default:
            break;
    }
        
    
        

        
}



void link_init() {
    
    REG_RCNT = R_MODE_GPIO;
    REG_RCNT = R_MODE_MULTI;
    REG_SIOCNT       = SIOU_38400;
    REG_SIOMLT_SEND  = 0;
    
    REG_SIOCNT |= SIO_MODE_MULTI;
    REG_SIOCNT |= SIO_IRQ;
}

void link_send(unsigned short data) {
    REG_SIOMLT_SEND = data;
    if(link_is_master())
        REG_SIOCNT |= SION_ENABLE;
}



void link_send_player() {
    if(loading > 0 || freeze) return;
    
    switch(should_send_next_index%7) {
        case 0:
            link_send((player.x&0xFF) | (PACKET_PLAYER_X0<<8)); 
            break;
        case 1:
            link_send(((player.x>>8)&0xFF) | (PACKET_PLAYER_X1<<8)); 
            break;
        case 2:
            link_send((player.y&0xFF) | (PACKET_PLAYER_Y0<<8)); 
            break;
        case 3:
            link_send(((player.y>>8)&0xFF) | (PACKET_PLAYER_Y1<<8)); 
            break;
        case 4:
            link_send(((player.sprite)&0xFF) | (PACKET_PLAYER_SPRITE<<8)); 
            break;
        case 5:
            if(should_send_next_index/7 == 0)
                link_send(((animTick)&0xFF) | (PACKET_ANIMTICK0<<8)); 
            else
                link_send(((mapIndex)&0xFF) | (PACKET_WORLD<<8)); 
            break;
        case 6:
            if(should_send_next_index/7 == 0)
                link_send(((animTick>>8)&0xFF) | (PACKET_ANIMTICK1<<8)); 
            else {
                if(should_send_next_index%3 == 0)
                    link_send(((coins[0])&0xFF) | (PACKET_COINS0<<8)); 
                else if(should_send_next_index%3 == 1)
                    link_send(((coins[1])&0xFF) | (PACKET_COINS1<<8)); 
                else if(should_send_next_index%3 == 2)
                    link_send(((coins[2])&0xFF) | (PACKET_COINS2<<8)); 
            }
            break;
        default:
            break;
    }
    
    should_send_next_index = (should_send_next_index + 1) % (7*128);
   // should_send_next = false;
    //should_send_next_timer = 0;
}


void link_irq_serial() {
    
    if (!link_is_ready() || link_has_error()) {
      link_init();
      return;
    }
    
    for(int i=0;i<4;i++)
        handle_packet(i, REG_SIOMULTI[i]);
    
    if(!link_is_master())
        link_send_player();
}

void link_irq_timer() {
    if(link_is_master())
        link_send_player();
}

int main()
{
    vid_page= vid_mem;
    freeze = true;

 
	irq_init(NULL);
	irq_add(II_VBLANK, mmVBlank);
    irq_add(II_SERIAL, link_irq_serial);
    irq_add(II_TIMER3, link_irq_timer);
    

    link_init();
    
    REG_TM[3].start = -50;
    REG_TM[3].cnt = TM_ENABLE | TM_IRQ | TM_FREQ_1024;
    
    
    mmInitDefault((mm_addr)soundbank_bin, 8);
    
    mmSetModuleVolume(1024);
    mmSetJingleVolume(1024);
    
	
	pal_bg_mem[255]= CLR_WHITE;

    // Set display options for the main background
    REG_BG0CNT= BG_PRIO(2) | BG_CBB(0) | BG_SBB(30) | BG_4BPP | BG_REG_64x32;
    REG_BG3CNT= BG_PRIO(0) | BG_CBB(0) | BG_SBB(28) | BG_4BPP | BG_REG_32x32;
    
    // Load sprite pallette, and tiles into CB 4
    memcpy32(pal_obj_mem, sprites_4objPal, sprites_4objPalLen/4);
    
    
    memcpy32(&tile_mem[4][0], sprites_4objTiles, sprites_4objTilesLen/4);
    
    
	// init video mode
	REG_DISPCNT= DCNT_MODE4 | DCNT_BG2 | DCNT_OBJ;
    
    memcpy32(vid_mem, screen_8bmpBitmap, screen_8bmpBitmapLen/4);
    memcpy32(pal_bg_mem, screen_8bmpPal, screen_8bmpPalLen/4);

	while(freeze)
	{
        mmFrame();
		VBlankIntrWait();
        
        key_poll();
        if(key_hit(KI_A) ||
           key_hit(KI_B)||
           key_hit(KI_SELECT)||
           key_hit(KI_START)||
           key_hit(KI_RIGHT)||
           key_hit(KI_LEFT)||
           key_hit(KI_UP)||
           key_hit(KI_DOWN)||
           key_hit(KI_R)||
           key_hit(KI_L) ||
           key_tri_horz() != 0 ||
           key_tri_vert() != 0) {
            loading = 10*3;
            freeze = false;
        }

		
	}
    
    
    // Load charset into CB 1
    tte_init_se_default(1, BG_PRIO(0) | BG_CBB(1)|BG_SBB(29)); // overlay into screenblock 29
    oam_init(obj_buffer, 128);

        
    // Load background pallette, and tiles into CB 0
    memcpy32(pal_bg_mem, SharedPal, SharedPalLen/4);
    memcpy32(&tile_mem[0][0], SharedTiles, SharedTilesLen/4);
    
    
    memcpy32(&se_mem[28][0], loading_mapMap, loading_mapMapLen/4);

    // show bg 0 (map), bg 1 (overlay), objects (player, movables, coins, mobs), sprites are 2d
    REG_DISPCNT = DCNT_MODE0;    
    

    nextMap = 0;
    

    // Scroll around some
    
    while(1)
    {
        VBlankIntrWait();
        key_poll();
        
        
        if(loading > 0) {
            
            REG_DISPCNT = DCNT_MODE0 | DCNT_BG3;
            //mmEffectCancelAll();
            
            // Animate Loading screen tiles
            tile_mem[0][0x14] = tile_mem[0][0x15+((animTick%(2<<3))>>3)];
            tile_mem[0][0x17] = tile_mem[0][0x18+((animTick%(2<<3))>>3)];
        
            loading--;
            if(loading == 0) {
                REG_DISPCNT = DCNT_MODE0 | DCNT_BG0  | DCNT_BG1  | DCNT_OBJ | DCNT_OBJ_2D;
            }
        }
        
        if(nextMap != -1) {
            setWorld(nextMap);
            nextMap = -1;
        }
        
        

        int otrih = key_tri_horz();
        int otriv = key_tri_vert();
        
        if(loading > 0) {
            otrih = 0;
            otriv = 0;
        }
        
        int velXP = 0;
        if(otrih != 0) {
            velXP=otrih;
            player.is_moving = true;
        }else {
            player.is_moving = false;
        }
        
		if(otriv == -1 && !player.is_jumping && player.is_on_ground) {
            player.is_jumping=true;
            player.is_on_ground=false;
            player.in_jumping=15;
            mmEffect(SFX_JUMP);
        }
        
        if(player.is_moving && !player.is_jumping && player.is_on_ground && player.in_moving == 1 && otriv != -1) {
             mmEffect(SFX_STEP);
        }

        /*if(otriv == 1) {
            mmEffect(SFX_SAMPLE);
            loading = 10*3;
            nextMap = 2;
            continue;
        }*/
        
        if(coins[mapIndex] == 0x7F) {
            mapIndex++;
            if(mapIndex > 2) {
                mapIndex = 0;
                nextMap = mapIndex;
            }else
                nextMap = mapIndex;
            loading = 10*3;
            continue;
        }
        
    	
 
        //se_mem[30][(player.x/8) + (player.y/8)*32] = 5;
    
        if(player.is_jumping) {
            player.velocityY=-2;
            if(player.in_jumping<5)
                player.velocityY=-1;
            player.in_jumping--;
            if(player.in_jumping<=0)
                player.is_jumping = false;
            player.is_on_ground=false;
        }else
            player.velocityY=1;
            
   

        player.velocityX = velXP;
        handleVel();
        if((animTick&1) == 0) handleEntities();
        

        if(player.velocityX!=0 && player.is_moving)
            player.look_direction=player.velocityX;
        
        if(player.look_direction==1) {
            if(!player.is_on_ground || otriv == -1) {
                player.sprite = SPRITE_PLAYER_INAIR_RIGHT;
            }else if(player.is_moving) {
                if(player.in_moving >= 10)
                    player.sprite = SPRITE_PLAYER_MOVING0_RIGHT;
                else
                    player.sprite = SPRITE_PLAYER_MOVING1_RIGHT;
            }else {
                player.sprite = SPRITE_PLAYER_STANDING_RIGHT;
            }
        }else if(player.look_direction == -1) {
            if(!player.is_on_ground || otriv == -1){
                player.sprite = SPRITE_PLAYER_INAIR_LEFT;
            }else if(player.is_moving) {
                if(player.in_moving >= 10)
                    player.sprite = SPRITE_PLAYER_MOVING0_LEFT;
                else
                    player.sprite = SPRITE_PLAYER_MOVING1_LEFT;
            }else {
                player.sprite = SPRITE_PLAYER_STANDING_LEFT;
            }
        }

        

        REG_BG0HOFS= scrollx;
        REG_BG0VOFS= scrolly;
        
        drawWorld();

        
        tile_mem[0][2] = tile_mem[0][3+((animTick% (4<<3))>>3)];
        animTick = (animTick + 1) % 0x10000;
        
        
        
        tte_write("#{P:2,0}");
        char str[32];
        
        int coin_count = 0;
        for(int i=0;i<7;i++)
            if(((coins[mapIndex]>>i)&1) == 1) 
                coin_count++;
        
        siprintf(str, "%1d/%1d Chipsets Level: %1d", coin_count, 7, mapIndex+1);
        tte_write(str);
            
        mmFrame();
        oam_copy(oam_mem, obj_buffer, AMOUNT_ENTITIES+1+4);
        
    }

    return 0;
}
