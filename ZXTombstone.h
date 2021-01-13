/*
 *  Copyright (C) 2008-2019 Florent Bedoiseau (electronique.fb@free.fr)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef TOMBSTONE_H
#define TOMBSTONE_H

 struct Object {
    unsigned char _valid; 
    unsigned char _x, _y;
	unsigned char _old, _tileNumber;
};

#define MAX_OBJECTS_IN_LIST 32

struct Point {
    unsigned char _x, _y;
};

#define MAX_POINTS_IN_LIST 8

void NoObject_Place(unsigned char x, unsigned char y, unsigned char tile);  
void Object_Copy(struct Object* dst, struct Object* src);

void Object_Place(struct Object *o,  unsigned char tile, unsigned char x, unsigned char y);
void Object_Erase(struct Object *o);
unsigned char Object_IncXY (struct Object* o, signed char ix, signed char iy);
void Object_SetXY (struct Object* o,  unsigned char xx, unsigned char yy);

void ObjectList_Init(struct Object* liste); // Init structure
void ObjectList_PlaceObject(struct Object* liste, unsigned char tileNum, unsigned char x, unsigned char y);
unsigned char ObjectList_isEmpty(struct Object* liste);
void ObjectList_Erase(struct Object* liste); // Clear all the list

void PointList_Copy(struct Point* dst, struct Point* src, unsigned char n);

#define S_INIT  0
#define S_RUN   1
#define S_PAUSE 2
#define S_END   3
#define S_OVER  4

#define SI_NONE    0
#define SI_MONSTER 1
#define SI_BUSH    2

void Game_Process ();
void Game_State ();

void Game_PrepareGame ();
void Game_ChangeDay ();
void Game_ChangeLife ();

void Game_Create ();
void Game_CreatePlayer ();
void Game_CreateAllCactus ();
void Game_CreateAllBuissons ();
void Game_CreateAllCages ( unsigned char tileCage);

void Game_Delete ();
void Game_DeleteAllObjects ();
void Game_DeletePlayer ();

void Game_TimeCreateMonsters ();
void Game_TimeCreateBuissons ();
void Game_TimeMovePlayer ();
void Game_TimeMoveBuissons ();
void Game_TimeMoveMonsters ();
void Game_TimeFirePlayer ();
void Game_TimeMoveFire();
void Game_TimeSplash();

void Game_DisplayBoardTiles();
void Game_DisplayFire(unsigned char draw);

unsigned int Game_getVideoAddr(unsigned char x, unsigned char y);

void Game_IntroScreen();

void Game_DrawTile(unsigned char x, unsigned char y, unsigned char* t, unsigned char color);
void Game_DrawLowerTile();
void Game_DrawLowerTile();
void Game_DrawBoardTile(unsigned char cx, unsigned char cy);
void Game_DrawNumberTile(unsigned char x, unsigned char y, unsigned int v, unsigned char nd);
void Game_DrawString(unsigned char x, unsigned char y, char *str, unsigned char color); // Spectrum 
void Game_DrawCharacter(unsigned char x, unsigned char y, unsigned char c, unsigned char color);

unsigned char Game_ComputeNewMonsterPosition ();
unsigned char Game_ComputeThreeCactusList ( unsigned char x, unsigned char y, struct Point*);
void Game_MoveMonsters ();
unsigned char Game_MoveMonsterX ( struct Object *aObject, signed char dx);
unsigned char Game_MoveMonsterY ( struct Object *aObject, signed char dy);
void Game_ReplaceCactusByMonster ( unsigned char x, unsigned char y);
unsigned char Game_RemoveXYObject ( struct Object* aListOfObject, unsigned char x, unsigned char y);
unsigned char Game_RemoveXYNoObject (unsigned char x, unsigned char y);

unsigned char Game_CheckIfBlocked ();
void Game_FindEmptySpace ( unsigned char* x, unsigned char* y);
unsigned char Game_CheckBounces ( unsigned char x, unsigned char y);
unsigned char Game_CheckObstacles ( unsigned char x, unsigned char y);
unsigned char Game_IsOut (unsigned char x, unsigned char y);

void Game_ShakeCactusList();
void ShakeDirection (unsigned char *tab_dir);
signed char sign (signed char val);
void sleepNFrames(unsigned int nFrames);
unsigned char rand_mod(unsigned char modulo);

#endif
