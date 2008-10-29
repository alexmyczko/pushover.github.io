#include "leveldisplay.h"

#include "textsections.h"
#include "sha1.h"
#include "graphics.h"
#include "soundsys.h"
#include "screen.h"
#include "ant.h"

#include <stdio.h>

#include <sstream>
#include <iomanip>

void levelDisplay_c::load(const textsections_c & sections) {

  levelData_c::load(sections);

  // attention don't use many levels on the same graphics object
  gr.setTheme(getTheme());
  background.markAllDirty();
  target.markAllDirty();

  Min = Sec = -1;
}

levelDisplay_c::levelDisplay_c(surface_c & t, graphics_c & g) : background(t), target(t), gr(g) {
}

levelDisplay_c::~levelDisplay_c(void) { }

void levelDisplay_c::updateBackground(void)
{
  for (unsigned int y = 0; y < 13; y++)
  {
    for (unsigned int x = 0; x < 20; x++)
    {
      // when the current block is dirty, recreate it
      if (background.isDirty(x, y))
      {
        for (unsigned char b = 0; b < numBg; b++)
          background.blitBlock(gr.getBgTile(level[y][x].bg[b]), x*gr.blockX(), y*gr.blockY());
        background.blitBlock(gr.getFgTile(level[y][x].fg), x*gr.blockX(), y*gr.blockY());

        background.gradient(gr.blockX()*x, gr.blockY()*y, gr.blockX(), gr.blockY());
      }
    }

  }
  background.clearDirty();
}

void levelDisplay_c::drawDominos(void) {

  int timeLeft = getTimeLeft();

  // the dirty marks for the clock
  {

    // calculate the second left
    int tm = timeLeft/18;

    // if negative make positive again
    if (timeLeft < 0)
      tm = -tm+1;

    int newSec = tm%60;
    int newMin = tm/60;

    if (newSec != Sec || timeLeft == -1)
    {
      target.markDirty(3, 11);
      target.markDirty(3, 12);
    }

    if (newSec != Sec || newMin != Min || timeLeft % 18 == 17 || timeLeft % 18 == 8 || timeLeft == -1)
    {
      target.markDirty(2, 11);
      target.markDirty(2, 12);
    }

    if (newMin != Min || timeLeft == -1)
    {
      target.markDirty(1, 11);
      target.markDirty(1, 12);
    }

    Min = newMin;
    Sec = newSec;
  }

  // copy background, where necessary
  for (unsigned int y = 0; y < 13; y++)
    for (unsigned int x = 0; x < 20; x++)
      if (target.isDirty(x, y))
        target.copy(background, x*gr.blockX(), y*gr.blockY(), gr.blockX(), gr.blockY());

  static int XposOffset[] = {-16, -16,  0,-16,  0,  0, 0, 0, 0,  0, 0, 16,  0, 16, 16, 0};
  static int YposOffset[] = { -8,  -6,  0, -4,  0, -2, 0, 0, 0, -2, 0, -4,  0, -6, -8, 0};
  static int StoneImageOffset[] = {  7, 6, 0, 5, 0, 4, 0, 0, 0, 3, 0, 2, 0, 1, 0, 0};

  // the idea behind this code is to repaint the dirty blocks. Dominos that are actually
  // within neighbor block must be repaint, too, when they might reach into the actual
  // block. But painting the neighbors is only necessary, when they are not drawn on
  // their own anyway, so always check for !dirty of the "homeblock" of each domino

  int SpriteYPos = gr.getDominoYStart();

  for (int y = 0; y < 13; y++, SpriteYPos += gr.blockY()) {

    int SpriteXPos = -2*gr.blockX();

    for (int x = 0; x < 20; x++, SpriteXPos += gr.blockX()) {

      if (!target.isDirty(x, y)) continue;

      // paint the left neighbor domino, if it leans in our direction and is not painted on its own
      if (y < 12 && x > 0 && !target.isDirty(x-1, y+1) && level[y+1][x-1].dominoType != DominoTypeEmpty &&
          (level[y+1][x-1].dominoState > 8 ||
           level[y+1][x-1].dominoType == DominoTypeSplitter && level[y+1][x-1].dominoState != 8 ||
           level[y+1][x-1].dominoState >= DominoTypeCrash0))
      {
        target.blit(gr.getDomino(level[y+1][x-1].dominoType-1, level[y+1][x-1].dominoState-1),
            SpriteXPos-gr.blockX(),
            SpriteYPos+gr.convertDominoY(level[y+1][x-1].dominoYOffset)+gr.blockY());
      }

      if (x > 0 && !target.isDirty(x-1, y) && level[y][x-1].dominoType != DominoTypeEmpty &&
          (level[y][x-1].dominoState > 8 ||
           level[y][x-1].dominoType == DominoTypeSplitter && level[y][x-1].dominoState != 8 ||
           level[y][x-1].dominoType >= DominoTypeCrash0))
      {
        target.blit(gr.getDomino(level[y][x-1].dominoType-1, level[y][x-1].dominoState-1),
            SpriteXPos-gr.blockX(),
            SpriteYPos+gr.convertDominoY(level[y][x-1].dominoYOffset));
      }

      if (y < 12 && !target.isDirty(x, y+1) && level[y+1][x].dominoType != DominoTypeEmpty)
      {
        target.blit(gr.getDomino(level[y+1][x].dominoType-1, level[y+1][x].dominoState-1),
            SpriteXPos,
            SpriteYPos+gr.convertDominoY(level[y+1][x].dominoYOffset)+gr.blockY());
      }

      // paint the splitting domino for the splitter
      if (level[y][x].dominoType == DominoTypeSplitter &&
          level[y][x].dominoState == 6 &&
          level[y][x].dominoExtra != 0)
      {
        target.blit(gr.getDomino(level[y][x].dominoExtra-1, level[y][x].dominoExtra>=DominoTypeCrash0?0:7),
            SpriteXPos,
            SpriteYPos-gr.splitterY());
        level[y][x].dominoExtra = 0;
      }

      // paint the actual domino but take care of the special cases of the ascender domino
      if (level[y][x].dominoType == DominoTypeAscender && level[y][x].dominoExtra == 0x60 &&
          level[y][x].dominoState < 16 && level[y][x].dominoState != 8)
      {
        target.blit(gr.getDomino(DominoTypeRiserCont-1, StoneImageOffset[level[y][x].dominoState-1]),
            SpriteXPos+gr.convertDominoX(XposOffset[level[y][x].dominoState-1]),
            SpriteYPos+gr.convertDominoY(YposOffset[level[y][x].dominoState-1]+level[y][x].dominoYOffset));
      }
      else if (level[y][x].dominoType == DominoTypeAscender && level[y][x].dominoState == 1 && level[y][x].dominoExtra == 0 &&
          level[y-2][x-1].fg == 0)
      { // this is the case of the ascender domino completely horizontal and with the plank it is below not existing
        // so we see the above face of the domino. Normally there is a wall above us so we only see
        // the front face of the domino
        target.blit(gr.getDomino(DominoTypeRiserCont-1, StoneImageOffset[level[y][x].dominoState-1]),
            SpriteXPos+gr.convertDominoX(XposOffset[level[y][x].dominoState-1]+6),
            SpriteYPos+gr.convertDominoY(YposOffset[level[y][x].dominoState-1]+level[y][x].dominoYOffset));
      }
      else if (level[y][x].dominoType == DominoTypeAscender && level[y][x].dominoState == 15 && level[y][x].dominoExtra == 0 &&
          level[y-2][x+1].fg == 0)
      {
        target.blit(gr.getDomino(DominoTypeRiserCont-1, StoneImageOffset[level[y][x].dominoState-1]),
            SpriteXPos+gr.convertDominoX(XposOffset[level[y][x].dominoState-1]-2),
            SpriteYPos+gr.convertDominoY(YposOffset[level[y][x].dominoState-1]+level[y][x].dominoYOffset));
      }
      else if (level[y][x].dominoType != DominoTypeEmpty)
      {
        target.blit(gr.getDomino(level[y][x].dominoType-1, level[y][x].dominoState-1),
            SpriteXPos,
            SpriteYPos+gr.convertDominoY(level[y][x].dominoYOffset));
      }

      // paint the right neighor if it is leaning in our direction
      if (x < 19 && y < 12 && !target.isDirty(x+1, y+1) && level[y+1][x+1].dominoType != DominoTypeEmpty &&
          (level[y+1][x+1].dominoState < 8 ||
           level[y+1][x+1].dominoType == DominoTypeSplitter && level[y+1][x+1].dominoState != 8 ||
           level[y+1][x+1].dominoType >= DominoTypeCrash0))
      {
        target.blit(gr.getDomino(level[y+1][x+1].dominoType-1, level[y+1][x+1].dominoState-1),
            SpriteXPos+gr.blockX(),
            SpriteYPos+gr.convertDominoY(level[y+1][x+1].dominoYOffset)+gr.blockY());
      }

      if (x < 19 && !target.isDirty(x+1, y) && level[y][x+1].dominoType != DominoTypeEmpty &&
          (level[y][x+1].dominoState < 8 ||
           level[y][x+1].dominoType == DominoTypeSplitter && level[y][x+1].dominoState != 8 ||
           level[y][x+1].dominoType >= DominoTypeCrash0))
      {
        target.blit(gr.getDomino(level[y][x+1].dominoType-1, level[y][x+1].dominoState-1),
            SpriteXPos+gr.blockX(),
            SpriteYPos+gr.convertDominoY(level[y][x+1].dominoYOffset));
      }

      if (y >= 11) continue;

      if (!target.isDirty(x, y+2) && level[y+2][x].dominoType == DominoTypeAscender)
      {
        target.blit(gr.getDomino(level[y+2][x].dominoType-1, level[y+2][x].dominoState-1),
            SpriteXPos,
            SpriteYPos+gr.convertDominoY(level[y+2][x].dominoYOffset)+2*gr.blockY());
      }

      if (x > 0 && !target.isDirty(x-1, y+2) && level[y+2][x-1].dominoType == DominoTypeAscender)
      {
        target.blit(gr.getDomino(level[y+2][x-1].dominoType-1, level[y+2][x-1].dominoState-1),
            SpriteXPos-gr.blockX(),
            SpriteYPos+gr.convertDominoY(level[y+2][x-1].dominoYOffset)+2*gr.blockY());
      }

      if (x < 19 && !target.isDirty(x+1, y+2) && level[y+2][x+1].dominoType == DominoTypeAscender)
      {
        target.blit(gr.getDomino(level[y+2][x+1].dominoType-1, level[y+2][x+1].dominoState-1),
            SpriteXPos+gr.blockX(),
            SpriteYPos+gr.convertDominoY(level[y+2][x+1].dominoYOffset)+2*gr.blockY());
      }

      if (level[y][x].dominoType != DominoTypeAscender) continue;

      if (!target.isDirty(x, y+2) && level[y+2][x].dominoType != DominoTypeEmpty)
      {
        target.blit(gr.getDomino(level[y+2][x].dominoType-1, level[y+2][x].dominoState-1),
            SpriteXPos,
            SpriteYPos+gr.convertDominoY(level[y+2][x].dominoYOffset)+2*gr.blockY());
      }

      if (x > 0 && !target.isDirty(x-1, y+2) && level[y+2][x-1].dominoType != DominoTypeEmpty)
      {
        target.blit(gr.getDomino(level[y+2][x-1].dominoType-1, level[y+2][x-1].dominoState-1),
            SpriteXPos-gr.blockX(),
            SpriteYPos+gr.convertDominoY(level[y+2][x-1].dominoYOffset)+2*gr.blockY());
      }

      if (x >= 19) continue;

      if (!target.isDirty(x+1, y+2)) continue;

      if (level[y+2][x+1].dominoType == DominoTypeEmpty) continue;

      target.blit(gr.getDomino(level[y+2][x+1].dominoType-1, level[y+2][x+1].dominoState-1),
          SpriteXPos+gr.blockX(),
          SpriteYPos+gr.convertDominoY(level[y+2][x+1].dominoYOffset)+2*gr.blockY());
    }
  }

  // repaint the ladders in front of dominos
  for (unsigned int y = 0; y < 13; y++)
    for (unsigned int x = 0; x < 20; x++) {
      if (target.isDirty(x, y)) {
        if (getFg(x, y) == FgElementPlatformLadderDown || getFg(x, y) == FgElementLadder) {
          SDL_Rect dst;
          dst.x = x*gr.blockX();
          dst.y = y*gr.blockY();
          dst.w = gr.blockX();
          dst.h = gr.blockY();
          target.blitBlock(gr.getFgTile(FgElementLadder2), dst.x, dst.y);
        }
        else if (getFg(x, y) == FgElementPlatformLadderUp)
        {
          SDL_Rect dst;
          dst.x = x*gr.blockX();
          dst.y = y*gr.blockY();
          dst.w = gr.blockX();
          dst.h = gr.blockY();
          target.blitBlock(gr.getFgTile(FgElementLadderMiddle), dst.x, dst.y);
        }
      }
    }

  if (timeLeft < 60*60*18)
  { // output the time
    char time[6];

    // care for the : between the minutes and seconds and
    // make a string out of the time
    // in the new font ':' and ' ' have different width, so keep it
    // just a colon for now, we will make it blink later on again
    // TODO
//    if (timeLeft % 18 < 9)
      snprintf(time, 6, "%02i:%02i", Min, Sec);
//    else
//      snprintf(time, 6, "%02i %02i", Min, Sec);

    fontParams_s pars;
    if (timeLeft >= 0)
    {
      pars.color.r = pars.color.g = 255; pars.color.b = 0;
    }
    else
    {
      pars.color.r = 255; pars.color.g = pars.color.b = 0;
    }
    pars.font = FNT_BIG;
    pars.alignment = ALN_TEXT;
    pars.box.x = gr.timeXPos();
    pars.box.y = gr.timeYPos();
    pars.box.w = 50;
    pars.box.h = 50;
    pars.shadow = true;

    target.renderText(&pars, time);
  }
}

