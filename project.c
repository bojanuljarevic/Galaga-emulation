
///////////////////////////////////////////////////////////////////////////////
// Headers.

#include <stdint.h>
#include "system.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "sprites_rgb333.h"

///////////////////////////////////////////////////////////////////////////////
// HW stuff.

#define WAIT_UNITL_0(x) while(x != 0){}
#define WAIT_UNITL_1(x) while(x != 1){}

#define SCREEN_IDX1_W 640
#define SCREEN_IDX1_H 480
#define SCREEN_IDX4_W 320
#define SCREEN_IDX4_H 240
#define SCREEN_RGB333_W 160
#define SCREEN_RGB333_H 120

#define SCREEN_IDX4_W8 (SCREEN_IDX4_W/8)

#define gpu_p32 ((volatile uint32_t*)LPRS2_GPU_BASE)
#define palette_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x1000))
#define unpack_idx1_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x400000))
#define pack_idx1_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x600000))
#define unpack_idx4_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x800000))
#define pack_idx4_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0xa00000))
#define unpack_rgb333_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0xc00000))
#define joypad_p32 ((volatile uint32_t*)LPRS2_JOYPAD_BASE)

typedef struct {
	unsigned a      : 1;
	unsigned b      : 1;
	unsigned z      : 1;
	unsigned start  : 1;
	unsigned up     : 1;
	unsigned down   : 1;
	unsigned left   : 1;
	unsigned right  : 1;
} bf_joypad;
#define joypad (*((volatile bf_joypad*)LPRS2_JOYPAD_BASE))

typedef struct {
	uint32_t m[SCREEN_IDX1_H][SCREEN_IDX1_W];
} bf_unpack_idx1;
#define unpack_idx1 (*((volatile bf_unpack_idx1*)unpack_idx1_p32))



///////////////////////////////////////////////////////////////////////////////
// Game config.

#define STEP 1


#define ANIM_DELAY 3

#define BLUE_NUM 8
#define RED_NUM 6
#define GREEN_NUM 4


///////////////////////////////////////////////////////////////////////////////
// Game data structures.

typedef struct {
	uint16_t x;
	uint16_t y;
} point_t;

typedef enum {
	SHIP_ALIVE,
	SHIP_DEAD_1,
	SHIP_DEAD_2,
	SHIP_DEAD_3,
	SHIP_DEAD_4,
	NO_SHIP
} ship_anim_states_t;

typedef struct {
	ship_anim_states_t state;
	uint8_t delay_cnt;
} ship_anim_t;

typedef struct {
	point_t pos;
	ship_anim_t anim;
	int alive;
} ship_t;


typedef struct {
	ship_t ship;
} game_state_t;


typedef struct Laser {
	int x;
	int y;
	int fired;
} Laser;

static int lives = 3;


void draw_sprite_from_atlas(
	uint16_t src_x,
	uint16_t src_y,
	uint16_t w,
	uint16_t h,
	uint16_t dst_x,
	uint16_t dst_y
) {
	
	
	for(uint16_t y = 0; y < h; y++){
		for(uint16_t x = 0; x < w; x++){
			uint32_t src_idx = 
				(src_y+y)*galaga_sprites__w +
				(src_x+x);
			uint32_t dst_idx = 
				(dst_y+y)*SCREEN_RGB333_W +
				(dst_x+x);
			uint16_t pixel = galaga_sprites__p[src_idx];
			unpack_rgb333_p32[dst_idx] = pixel;
		}
	}
	
	
}


void draw_black_square(ship_t ship);
void draw_white_square(int width, int height);

///////////////////////////////////////////////////////////////////////////////
// Game code.

int main(void) {
	
	// Setup.
	gpu_p32[0] = 3; // Color bar.
	gpu_p32[0x800] = 0x00ff00ff; // Magenta for HUD.


	// Game state.
	game_state_t gs;
	gs.ship.pos.x = 80;
	gs.ship.pos.y = 100;
	gs.ship.anim.state = SHIP_ALIVE;
	gs.ship.anim.delay_cnt = 0;
	gs.ship.alive = 1;
	
	ship_t blue[BLUE_NUM];
	ship_t red[RED_NUM];
	ship_t green[GREEN_NUM];
	
	int i;
	for(i = 0; i < BLUE_NUM; i++) {
		blue[i].pos.x = 20 + 15*i;
		blue[i].pos.y = 30;
		blue[i].anim.state = SHIP_ALIVE;
		blue[i].anim.delay_cnt = 0;
		blue[i].alive = 1;
		if(i < RED_NUM) {
		red[i].pos.x = 35 + 15*i;
		red[i].pos.y = 15;
		red[i].anim.state = SHIP_ALIVE;
		red[i].anim.delay_cnt = 0;
		red[i].alive = 1;
		}
		if(i < GREEN_NUM) {
		green[i].pos.x = 45 + 20*i;
		green[i].pos.y = 0;
		green[i].anim.state = SHIP_ALIVE;
		green[i].anim.delay_cnt = 0;
		green[i].alive = 1;
		}
	}
	
	char move = 'r';
	
	Laser newLaser;
	newLaser.fired = 0;
	Laser blueLaser[BLUE_NUM];
	for(i = 0; i < BLUE_NUM; i++) {
		blueLaser[i].fired = 0;
		blueLaser[i].x = blue[i].pos.x;
		blueLaser[i].y = blue[i].pos.y + 16;
	}
	Laser redLaser[RED_NUM];
	for(i = 0; i < RED_NUM; i++) {
		redLaser[i].fired = 0;
		redLaser[i].x = red[i].pos.x;
		redLaser[i].y = red[i].pos.y;
	}
	Laser greenLaser[GREEN_NUM];
	for(i = 0; i < GREEN_NUM; i++) {
		greenLaser[i].fired = 0;
		greenLaser[i].x = green[i].pos.x;
		greenLaser[i].y = green[i].pos.y;
	}
	
	
	// Za nasumicno pucanje
	srand(time(0));
	
	
	// Orijentacija za kretanje brodica
	int moveLimit = blue[7].pos.x;
	
	
	while(1){
		
		
		/////////////////////////////////////
		// Poll controls.
		
		int mov_x = 0;
		int mov_y = 0;
		if(joypad.right){
			mov_x = +1;
		}
		
		if(joypad.left){
			mov_x = -1;
		}
		
		if(joypad.z){
			// SUICIDE
			if (gs.ship.anim.state == SHIP_ALIVE) gs.ship.anim.state = SHIP_DEAD_1;
		}
		
		if(joypad.a) {
			// SHOOT
			if(newLaser.fired != 1 && gs.ship.alive == 1) {
				newLaser.x = gs.ship.pos.x;
				newLaser.y = gs.ship.pos.y;
				newLaser.fired = 1;
			}
		}
		

		/////////////////////////////////////
		// Gameplay.
		
		
		// PLAYER MOVEMENTS
		if(gs.ship.pos.x >= 1  && gs.ship.pos.x <= 144) gs.ship.pos.x += mov_x*STEP;
		else if(gs.ship.pos.x < 1) gs.ship.pos.x++;
		else gs.ship.pos.x--;
		gs.ship.pos.y += mov_y*STEP;
		
		// LASER FIRING
		if(newLaser.fired == 1) {
			if(newLaser.y > 2) newLaser.y -= 3;
			else newLaser.y = 0;
			if(newLaser.y == 0) {
				newLaser.fired = 0;
			}
		}
		
		//PLAYER DEATH ANIMATION STATES
		switch(gs.ship.anim.state){
		case SHIP_ALIVE:
			break;
		case SHIP_DEAD_1:
			if(gs.ship.anim.delay_cnt != 0){
					gs.ship.anim.delay_cnt--;
			}else{
				gs.ship.anim.delay_cnt = ANIM_DELAY;
				gs.ship.anim.state = SHIP_DEAD_2;
			}
			break;
		case SHIP_DEAD_2:
			if(gs.ship.anim.delay_cnt != 0){
					gs.ship.anim.delay_cnt--;
			}else{
				gs.ship.anim.delay_cnt = ANIM_DELAY;
				gs.ship.anim.state = SHIP_DEAD_3;
			}
			break;
		case SHIP_DEAD_3:
			if(gs.ship.anim.delay_cnt != 0){
					gs.ship.anim.delay_cnt--;
			}else{
				gs.ship.anim.delay_cnt = ANIM_DELAY;
				gs.ship.anim.state = SHIP_DEAD_4;
			}
			break;
		case SHIP_DEAD_4:
			if(gs.ship.anim.delay_cnt != 0){
					gs.ship.anim.delay_cnt--;
			}else{
				gs.ship.anim.delay_cnt = ANIM_DELAY;
				gs.ship.anim.state = NO_SHIP;
			}
			break;
		}
		
		
		// ENEMY MOVEMENTS
		if(moveLimit== 145) move = 'l';
		else if(moveLimit == 105) move = 'r';
		
		if(move == 'r') {
			moveLimit++;
			for(i = 0; i < 8; i++) {
				blue[i].pos.x += STEP;
				if(i < 6) red[i].pos.x += STEP;
				if(i < 4) green[i].pos.x += STEP;
			}
		} else {
			moveLimit--;
			for(i = 0; i < 8; i++) {
				blue[i].pos.x -= STEP;
				if(i < 6) red[i].pos.x -= STEP;
				if(i < 4) green[i].pos.x -= STEP;
			}
		}
		
		
		// ENEMY LASERS
		for(i = 0; i < BLUE_NUM; i++) {
			int x = rand() % 1000;
			if(x > 995 && blueLaser[i].fired == 0 && blue[i].alive == 1) blueLaser[i].fired = 1;
			if(blueLaser[i].fired == 1) {
				if(blueLaser[i].y == 120) { blueLaser[i].y = blue[i].pos.y + 16;	blueLaser[i].x = blue[i].pos.x; blueLaser[i].fired = 0; }
				else blueLaser[i].y += 2;
			}
			if((gs.ship.pos.y == blueLaser[i].y) && (blueLaser[i].x > gs.ship.pos.x) && (blueLaser[i].x < gs.ship.pos.x+16) && gs.ship.alive == 1) {
				gs.ship.alive = 0;
				gs.ship.anim.state = SHIP_DEAD_1;
			}
			if(i < RED_NUM) {
				int x = rand() % 1000;
				if(x > 995 && redLaser[i].fired == 0 && red[i].alive == 1) redLaser[i].fired = 1;
				if(redLaser[i].fired == 1) {
					if(redLaser[i].y == 120) { redLaser[i].y = red[i].pos.y + 16;	redLaser[i].x = red[i].pos.x; redLaser[i].fired = 0; }
					else redLaser[i].y += 2;
				}
				if((gs.ship.pos.y == redLaser[i].y) && (redLaser[i].x > gs.ship.pos.x) && (redLaser[i].x < gs.ship.pos.x+16) && gs.ship.alive == 1) {
					gs.ship.alive = 0;
					gs.ship.anim.state = SHIP_DEAD_1;
				}
			}
			if(i < GREEN_NUM) {
				int x = rand() % 1000;
				if(x > 995 && greenLaser[i].fired == 0 && green[i].alive == 1) greenLaser[i].fired = 1;
				if(greenLaser[i].fired == 1) {
					if(greenLaser[i].y == 120) { greenLaser[i].y = green[i].pos.y + 16;	greenLaser[i].x = green[i].pos.x; greenLaser[i].fired = 0; }
					else greenLaser[i].y += 2;
				}
				if((gs.ship.pos.y == greenLaser[i].y) && (greenLaser[i].x > gs.ship.pos.x) && (greenLaser[i].x < gs.ship.pos.x+16) && gs.ship.alive == 1) {
					gs.ship.alive = 0;
					gs.ship.anim.state = SHIP_DEAD_1;
				}
			}
		}
		
		
		// KILL ENEMIES
		for(i = 0; i < BLUE_NUM; i++) {
			if((newLaser.y <= (blue[i].pos.y+16)) && (newLaser.x > blue[i].pos.x) && (newLaser.x < blue[i].pos.x+16) && blue[i].alive == 1) {
				newLaser.fired = 0;
				blue[i].alive = 0;
				newLaser.x = 160;
				newLaser.y = 120;
				blue[i].pos.x = 160;
				blue[i].pos.y = 120;
			}
		}
		for(i = 0; i < RED_NUM; i++) {
			if((newLaser.y <= (red[i].pos.y+16)) && (newLaser.x > red[i].pos.x) && (newLaser.x < red[i].pos.x+16) && red[i].alive == 1) {
				newLaser.fired = 0;
				red[i].alive = 0;
				newLaser.x = 160;
				newLaser.y = 120;
				red[i].pos.x = 160;
				red[i].pos.y = 120;
			}
		}
		for(i = 0; i < GREEN_NUM; i++) {
			if((newLaser.y <= (green[i].pos.y+16)) &&  (newLaser.x > green[i].pos.x) && (newLaser.x < green[i].pos.x+16) && green[i].alive == 1) {
				newLaser.fired = 0;
				green[i].alive = 0;
				newLaser.x = 160;
				newLaser.y = 120;
				green[i].pos.x = 160;
				green[i].pos.y = 120;
			}
		}
				
		
		/////////////////////////////////////
		// Drawing.

		
		// DRAW FIRED LASERS
		if(newLaser.fired == 1) draw_sprite_from_atlas(307, 136, 16, 16, newLaser.x, newLaser.y);
		
		
		for(i = 0; i < BLUE_NUM; i++) {
			if(blueLaser[i].fired == 1) draw_sprite_from_atlas(307, 118, 16, 16, blueLaser[i].x, blueLaser[i].y);
			if(i < RED_NUM) {
				if(redLaser[i].fired == 1) draw_sprite_from_atlas(307, 118, 16, 16, redLaser[i].x, redLaser[i].y);
			}
			if(i < GREEN_NUM) {
				if(greenLaser[i].fired == 1) draw_sprite_from_atlas(307, 118, 16, 16, greenLaser[i].x, greenLaser[i].y);
			}
		}
		
		// DRAW ENEMIES
		for(i = 0; i < BLUE_NUM; i++) {
			if(blue[i].alive == 1) draw_sprite_from_atlas(127, 91, 16, 16, blue[i].pos.x, blue[i].pos.y);
			else draw_black_square(blue[i]);
			if(i < RED_NUM) {
				if(red[i].alive == 1) draw_sprite_from_atlas(127, 73, 16, 16, red[i].pos.x, red[i].pos.y);
				else draw_black_square(red[i]);
			}
			if(i < GREEN_NUM) {
				if(green[i].alive == 1) draw_sprite_from_atlas(109, 73, 16, 16, green[i].pos.x, green[i].pos.y);
				else draw_black_square(green[i]);
			}
		}
		
		
		// PLAYER DEATH ANIMATION DRAWING
		switch(gs.ship.anim.state){
		case SHIP_ALIVE:
			draw_sprite_from_atlas(
				109, 1, 16, 16, gs.ship.pos.x, gs.ship.pos.y
			);
			break;
		case SHIP_DEAD_1:
			if(gs.ship.alive != 0) {
				gs.ship.pos.x -= 8;
				gs.ship.pos.y -= 8;
				gs.ship.alive = 0;
			}
			draw_sprite_from_atlas(
				145, 1, 32, 32, gs.ship.pos.x, gs.ship.pos.y
			);
			break;
		case SHIP_DEAD_2:
			if(gs.ship.alive != 0) {
				gs.ship.pos.x -= 8;
				gs.ship.pos.y -= 8;
				gs.ship.alive = 0;
			}
			draw_sprite_from_atlas(
				179, 1, 32, 32, gs.ship.pos.x, gs.ship.pos.y
			);
			break;
		case SHIP_DEAD_3:
			if(gs.ship.alive != 0) {
				gs.ship.pos.x -= 8;
				gs.ship.pos.y -= 8;
				gs.ship.alive = 0;
			}
			draw_sprite_from_atlas(
				213, 1, 32, 32, gs.ship.pos.x, gs.ship.pos.y
			);
			break;
		case SHIP_DEAD_4:
			if(gs.ship.alive != 0) {
				gs.ship.pos.x -= 8;
				gs.ship.pos.y -= 8;
				gs.ship.alive = 0;
			}
			draw_sprite_from_atlas(
				247, 1, 32, 32, gs.ship.pos.x, gs.ship.pos.y
			);
			break;
		case NO_SHIP:
			draw_black_square(gs.ship);
			lives--;
			if(lives > 0) {
				gs.ship.anim.state = SHIP_ALIVE;
				gs.ship.alive = 1;
			}
			break;
		}
		
		if(lives > 0) draw_white_square(4, 2);
		if(lives > 1) draw_white_square(4, 7);
		if(lives > 2) draw_white_square(4, 12);
		
		
		// Detecting rising edge of VSync.
		WAIT_UNITL_0(gpu_p32[2]);
		WAIT_UNITL_1(gpu_p32[2]);
		// Draw in buffer while it is in VSync.
		
		
		
		// Black background.
		for(uint16_t r = 0; r < SCREEN_RGB333_H; r++){
			for(uint16_t c = 0; c < SCREEN_RGB333_W; c++){
				unpack_rgb333_p32[r*SCREEN_RGB333_W + c] = 0000;
			}
		}
		
		
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////


void draw_black_square(ship_t ship) {
	for(uint16_t y = 0; y < 16; y++){
		for(uint16_t x = 0; x < 16; x++){
			uint32_t dst_idx = 
			(ship.pos.y+y)*SCREEN_RGB333_W +
			(ship.pos.x+x);
			unpack_rgb333_p32[dst_idx] = 0000;
		}
	}
}

void draw_white_square(int width, int height) {
	for(uint16_t y = 0; y < 4; y++){
		for(uint16_t x = 0; x < 4; x++){
			uint32_t dst_idx = 
			(height + y)*SCREEN_RGB333_W +
			(width + x);
			unpack_rgb333_p32[dst_idx] = 0xffff;
		}
	}
}
