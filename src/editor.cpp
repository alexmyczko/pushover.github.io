/* Pushover
 *
 * Pushover is the legal property of its developers, whose
 * names are listed in the COPYRIGHT file, which is included
 * within the source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335 USA
 */

#include "editor.h"

#include "window.h"
#include "screen.h"
#include "graphicsn.h"
#include "tools.h"
#include "recorder.h"

#include <fstream>
#include <sys/stat.h>

// states of the level Editor
typedef enum {
  ST_INIT,
  ST_CHOOSE_LEVEL,     // initial transition state
  ST_EXIT,
  ST_NEW_LEVEL,
  ST_DOUBLE_NAME,
  ST_DELETE_LEVEL,
  ST_EDIT_HOME,        // edit the foreground of the level (all level related elements
  ST_EDIT_DOMINOS_SELECTOR,
  ST_EDIT_BACKGROUND,  // edit the background imagery of the level
  ST_EDIT_BG_SELECTOR,
  ST_EDIT_MENU,
  ST_EDIT_HELP,
  ST_EDIT_PLAY,
} states_e;

static states_e currentState, nextState, messageContinue;
static window_c * window;

static graphicsN_c * gr;
static screen_c * screen;
static levelset_c * levels;
static levelPlayer_c * l;
static std::string userName;
static std::string levelFileName;
static std::string levelName;
static recorder_c rec;
static ant_c * a;
static DominoType dt;
static surface_c * dominoOverlay = 0;
static surface_c * backgroundOverlay = 0;
static uint8_t bgLayer = 0;

static void loadLevels(void)
{
  if (levels) delete levels;

  try {
    levels = new levelset_c(getHome() + "levels/", "", true);
  }
  catch (format_error e)
  {
    printf("format error %s\n", e.msg.c_str());
    levels = 0;
  }
}

#define D_OVL_W 50
#define D_OVL_H 70
#define D_OVL_COL 5
#define D_OVL_ROW 3
#define D_OVL_FRAME 3

void updateDominoOverlay(void)
{
  if (!dominoOverlay)
  {
    dominoOverlay = new surface_c (2*D_OVL_FRAME+D_OVL_W*D_OVL_COL, 2*D_OVL_FRAME+D_OVL_H*D_OVL_ROW, false);
  }

  dominoOverlay->fillRect(0, 0, dominoOverlay->getX(), dominoOverlay->getY(), 0, 0, 0);

  for (int i = 0; i < DominoTypeLastNormal; i++)
  {
    if (dt == (DominoType(i+1)))
    {
      dominoOverlay->fillRect(D_OVL_FRAME+D_OVL_W*(i % D_OVL_COL), D_OVL_FRAME+D_OVL_H*(i / D_OVL_COL), D_OVL_W, D_OVL_H, 100, 100, 100);
    }
    else
    {
      dominoOverlay->fillRect(D_OVL_FRAME+D_OVL_W*(i % D_OVL_COL), D_OVL_FRAME+D_OVL_H*(i / D_OVL_COL), D_OVL_W, D_OVL_H, 0, 0, 0);
    }

    dominoOverlay->blit(*gr->getHelpDominoImage((DominoType)(i+1)), D_OVL_W*(i%D_OVL_COL)-75, 62+D_OVL_H*(i/D_OVL_COL));
  }

  gr->setOverlay(dominoOverlay);
}

#define B_OVL_W gr->blockX()
#define B_OVL_H gr->blockY()
#define B_OVL_COL 18
#define B_OVL_ROW 11
#define B_OVL_FRAME 3

void updateBackgroundOverlay(void)
{
  if (!backgroundOverlay)
  {
    backgroundOverlay = new surface_c (2*B_OVL_FRAME+B_OVL_W*B_OVL_COL, 2*B_OVL_FRAME+B_OVL_H*B_OVL_ROW, false);
  }

  backgroundOverlay->fillRect(0, 0, backgroundOverlay->getX(), backgroundOverlay->getY(), 0, 0, 0);

  const std::vector<surface_c *> bgTiles = gr->getBgTiles();

  int x = 0;
  int y = 0;
  int b = 0;

  for (size_t t = 0; t < bgTiles.size(); t++)
  {
    for (int yc = 0; yc < 3; yc++)
      for (int xc = 0; xc < 5; xc++)
      {
        if ((xc+5*x+yc+6*y+3*b)%2)
          backgroundOverlay->fillRect(B_OVL_FRAME+x*B_OVL_W+xc*8, B_OVL_FRAME+y*B_OVL_H+b*B_OVL_H/2+yc*8, 8, 8, 85, 85, 85);
        else
          backgroundOverlay->fillRect(B_OVL_FRAME+x*B_OVL_W+xc*8, B_OVL_FRAME+y*B_OVL_H+b*B_OVL_H/2+yc*8, 8, 8, 2*85, 2*85, 2*85);
      }

    backgroundOverlay->blitBlock(*(bgTiles[t]), B_OVL_FRAME+x*B_OVL_W, B_OVL_FRAME+y*B_OVL_H+b*B_OVL_H/2);

    b++;
    if (b == 2)
    {
      b = 0;
      x++;
      if (x == B_OVL_COL)
      {
        x = 0;
        y++;

        if (y == B_OVL_ROW)
          break;
      }
    }
  }

  gr->setOverlay(backgroundOverlay);
}

static void changeState(void)
{
  if (currentState != nextState)
  {
    // leave old state
    switch (currentState)
    {
      case ST_INIT:
      case ST_EXIT:
      case ST_EDIT_HOME:
      case ST_EDIT_BACKGROUND:
        break;

      case ST_EDIT_PLAY:
        gr->setPaintData(l, 0, screen);
        break;

      case ST_CHOOSE_LEVEL:
      case ST_NEW_LEVEL:
      case ST_DELETE_LEVEL:
      case ST_DOUBLE_NAME:
      case ST_EDIT_MENU:
      case ST_EDIT_HELP:
        delete window;
        window = 0;
        break;

      case ST_EDIT_DOMINOS_SELECTOR:
      case ST_EDIT_BG_SELECTOR:
        gr->setOverlay(0);
        break;
    }

    currentState = nextState;

    // enter new state
    switch (currentState)
    {
      case ST_INIT:
      case ST_EXIT:
      case ST_EDIT_HOME:
      case ST_EDIT_BACKGROUND:
        break;

      case ST_CHOOSE_LEVEL:
        loadLevels();
        window = getEditorLevelChooserWindow(*screen, *gr, *levels);
        break;

      case ST_NEW_LEVEL:
        window = getNewLevelWindow(*screen, *gr);
        break;

      case ST_DELETE_LEVEL:
        window = getDeleteLevelWindow(*screen, *gr, *levels);
        break;

      case ST_DOUBLE_NAME:
        window = getMessageWindow(*screen, *gr, _("The given level name already exists"));
        break;

      case ST_EDIT_MENU:
        window = getEditorMenu(*screen, *gr);
        break;

      case ST_EDIT_HELP:
        window = getEditorHelp(*screen, *gr);
        break;

      case ST_EDIT_PLAY:
        gr->setPaintData(l, a, screen);
        break;

      case ST_EDIT_DOMINOS_SELECTOR:
        updateDominoOverlay();
        break;

      case ST_EDIT_BG_SELECTOR:
        updateBackgroundOverlay();
        break;
    }
  }
}

void leaveEditor(void)
{
  if (window) delete window;
  window = 0;

  delete levels;
  levels = 0;

  if (currentState != ST_EXIT)
    printf("oops editor left without reason\n");

  gr->setEditorMode(false);
  gr->setShowGrid(false);
}

void startEditor(graphicsN_c & g, screen_c & s, levelPlayer_c & lp, ant_c & ant, const std::string & user)
{
  nextState = ST_CHOOSE_LEVEL;
  currentState = ST_INIT;
  window = 0;

  screen = &s;
  gr = &g;
  gr->setEditorMode(true);

  loadLevels();

  l = &lp;
  a = &ant;
  userName = user;

  gr->setCursor(l->levelX()/2, l->levelY()/2, 1, 1);

  dt = DominoTypeStandard;
  bgLayer = 0;
}

static void handleCommonKeys(const SDL_Event & event)
{
  Uint8 *keystate = SDL_GetKeyState(NULL);

  if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_UP)
  {
    if (keystate[SDLK_RSHIFT] || keystate[SDLK_LSHIFT])
      gr->setCursor(gr->getCursorX(), gr->getCursorY(), gr->getCursorW(), gr->getCursorH()-1);
    else
      gr->setCursor(gr->getCursorX(), gr->getCursorY()-1, gr->getCursorW(), gr->getCursorH());
  }

  if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_DOWN)
  {
    if (keystate[SDLK_RSHIFT] || keystate[SDLK_LSHIFT])
      gr->setCursor(gr->getCursorX(), gr->getCursorY(), gr->getCursorW(), gr->getCursorH()+1);
    else
      gr->setCursor(gr->getCursorX(), gr->getCursorY()+1, gr->getCursorW(), gr->getCursorH());
  }

  if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LEFT)
  {
    if (keystate[SDLK_RSHIFT] || keystate[SDLK_LSHIFT])
      gr->setCursor(gr->getCursorX(), gr->getCursorY(), gr->getCursorW()-1, gr->getCursorH());
    else
      gr->setCursor(gr->getCursorX()-1, gr->getCursorY(), gr->getCursorW(), gr->getCursorH());
  }

  if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHT)
  {
    if (keystate[SDLK_RSHIFT] || keystate[SDLK_LSHIFT])
      gr->setCursor(gr->getCursorX(), gr->getCursorY(), gr->getCursorW()+1, gr->getCursorH());
    else
      gr->setCursor(gr->getCursorX()+1, gr->getCursorY(), gr->getCursorW(), gr->getCursorH());
  }

  if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_HOME)
  {
    if (keystate[SDLK_RSHIFT] || keystate[SDLK_LSHIFT])
      gr->setCursor(gr->getCursorX(), gr->getCursorY(), 1, gr->getCursorH());
    else
      gr->setCursor(0, gr->getCursorY(), gr->getCursorW(), gr->getCursorH());
  }

  if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_END)
  {
    if (keystate[SDLK_RSHIFT] || keystate[SDLK_LSHIFT])
      gr->setCursor(gr->getCursorX(), gr->getCursorY(), l->levelX(), gr->getCursorH());
    else
      gr->setCursor(l->levelX()-1, gr->getCursorY(), gr->getCursorW(), gr->getCursorH());
  }

  if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_PAGEUP)
  {
    if (keystate[SDLK_RSHIFT] || keystate[SDLK_LSHIFT])
      gr->setCursor(gr->getCursorX(), gr->getCursorY(), gr->getCursorW(), 1);
    else
      gr->setCursor(gr->getCursorX(), 0, gr->getCursorW(), gr->getCursorH());
  }

  if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_PAGEDOWN)
  {
    if (keystate[SDLK_RSHIFT] || keystate[SDLK_LSHIFT])
      gr->setCursor(gr->getCursorX(), gr->getCursorY(), gr->getCursorW(), l->levelY());
    else
      gr->setCursor(gr->getCursorX(), l->levelY()-1, gr->getCursorW(), gr->getCursorH());
  }

  if (event.type == SDL_KEYDOWN && event.key.keysym.sym == 'g')
  {
    // toggle grid
    gr->setShowGrid(!gr->getShowGrid());
  }
}

static void setDominoStatus(DominoType dt)
{
  switch (dt)
  {
    case DominoTypeStandard:   gr->setStatus(_("Selected Standard domino")); break;
    case DominoTypeStopper:    gr->setStatus(_("Selected Stopper domino")); break;
    case DominoTypeSplitter:   gr->setStatus(_("Selected Splitter domino")); break;
    case DominoTypeExploder:   gr->setStatus(_("Selected Exploder domino")); break;
    case DominoTypeDelay:      gr->setStatus(_("Selected Delay domino")); break;
    case DominoTypeTumbler:    gr->setStatus(_("Selected Tumbler domino")); break;
    case DominoTypeBridger:    gr->setStatus(_("Selected Bridger domino")); break;
    case DominoTypeVanish:     gr->setStatus(_("Selected Vanisher domino")); break;
    case DominoTypeTrigger:    gr->setStatus(_("Selected Trigger domino")); break;
    case DominoTypeAscender:   gr->setStatus(_("Selected Ascender domino")); break;
    case DominoTypeConnectedA: gr->setStatus(_("Selected Connected A domino")); break;
    case DominoTypeConnectedB: gr->setStatus(_("Selected Connected B domino")); break;
    case DominoTypeCounter1:   gr->setStatus(_("Selected Counter Stopper 1 domino")); break;
    case DominoTypeCounter2:   gr->setStatus(_("Selected Counter Stopper 2 domino")); break;
    case DominoTypeCounter3:   gr->setStatus(_("Selected Counter Stopper 3 domino")); break;
    default:                   gr->setStatus(_("Selected unknown domino")); break;
  }
}

bool eventEditor(const SDL_Event & event)
{
  changeState();

  switch (currentState)
  {
    case ST_INIT:
    case ST_EXIT:
      break;

    case ST_CHOOSE_LEVEL:
      if (!window)
      {
        nextState = ST_EXIT;
      }
      else
      {
        window->handleEvent(event);

        if (window->isDone())
        {
          unsigned int sel = window->getSelection();

          if (sel < levels->getLevelNames().size())
          {
            // a real level choosen... load that one and go into editor
            levelName = levels->getLevelNames()[sel];
            levelFileName = levels->getFilename(levelName);

            levels->loadLevel(*l, levelName, "");
            nextState = ST_EDIT_HOME;
            gr->setPaintData(l, 0, screen);
            bgLayer = 0;
          }
          else
          {
            // no level loaded
            sel -= levels->getLevelNames().size();

            if (sel == 0)
              nextState = ST_NEW_LEVEL;
            else if (levels->getLevelNames().size() && sel == 1)
              nextState = ST_DELETE_LEVEL;
            else
              nextState = ST_EXIT;
          }
        }
      }
      break;

    case ST_NEW_LEVEL:
      if (!window)
      {
        nextState = ST_EXIT;
      }
      else
      {
        window->handleEvent(event);

        if (window->isDone())
        {
          std::string fname = getHome() + "levels/" + window->getText() + ".level";
          struct stat st;

          // first check if the resulting filename would be
          if (stat(fname.c_str(), &st) == 0)
          {
            nextState = ST_DOUBLE_NAME;
            messageContinue = ST_CHOOSE_LEVEL;
          }
          else
          {
            // create the new level
            {
              levelData_c l;
              l.setName(window->getText());
              l.setAuthor(userName);
              std::ofstream f(fname.c_str());
              l.save(f);
            }
            loadLevels();
            levelName = window->getText();
            levels->loadLevel(*l, levelName, "");

            gr->setPaintData(l, 0, screen);

            levelFileName = levels->getFilename(levelName);
            nextState = ST_EDIT_HOME;
            bgLayer = 0;
          }
        }
      }
      break;

    case ST_DELETE_LEVEL:
      if (!window)
      {
        nextState = ST_EXIT;
      }
      else
      {
        window->handleEvent(event);

        if (window->isDone())
        {
          unsigned int sel = window->getSelection();

          if (sel < levels->getLevelNames().size())
          {
            std::string name = levels->getLevelNames()[sel];
            std::string fname = levels->getFilename(name);

            remove(fname.c_str());
          }
          nextState = ST_CHOOSE_LEVEL;
        }
      }
      break;

    case ST_DOUBLE_NAME:
      if (!window)
      {
        nextState = ST_EXIT;
      }
      else
      {
        window->handleEvent(event);

        if (window->isDone())
        {
          nextState = messageContinue;
        }
      }
      break;

    case ST_EDIT_MENU:
      if (!window)
      {
        nextState = ST_EXIT;
      }
      else
      {
        window->handleEvent(event);

        if (window->isDone())
        {
          switch (window->getSelection())
          {
            case 0:
              // theme
              nextState = ST_EDIT_HOME;
              break;

            case 1:
              // name
              nextState = ST_EDIT_HOME;
              break;

            case 2:
              // time
              nextState = ST_EDIT_HOME;
              break;

            case 3:
              // hint
              nextState = ST_EDIT_HOME;
              break;

            case 4:
              // authors
              nextState = ST_EDIT_HOME;
              break;

            case 5:
              // play
              {
                std::ofstream out(levelFileName.c_str());
                l->save(out);
              }
              loadLevels();
              a->initForLevel();
              gr->setShowGrid(false);
              nextState = ST_EDIT_PLAY;
              break;

            case 6:
              // save
              {
                std::ofstream out(levelFileName.c_str());
                l->save(out);
              }
              loadLevels();
              nextState = messageContinue;
              break;

            case 7:
              // another level
              nextState = ST_CHOOSE_LEVEL;
              break;

            case 8:
              // leave
              nextState = ST_EXIT;
              break;

            default:
              nextState = messageContinue;
              break;
          }
        }
      }
      break;

    case ST_EDIT_HELP:
      if (!window)
      {
        nextState = ST_EXIT;
      }
      else
      {
        window->handleEvent(event);

        if (window->isDone())
        {
          nextState = messageContinue;
        }
      }
      break;

    case ST_EDIT_DOMINOS_SELECTOR:

      if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_LCTRL)
      {
        setDominoStatus(dt);
        nextState = ST_EDIT_HOME;
      }

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_UP)
        if (dt > D_OVL_COL)
        {
          dt = (DominoType)(dt-D_OVL_COL);
          updateDominoOverlay();
        }

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LEFT)
        if (dt > 1)
        {
          dt = (DominoType)(dt-1);
          updateDominoOverlay();
        }

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_DOWN)
        if (dt+D_OVL_COL <= DominoTypeLastNormal )
        {
          dt = (DominoType)(dt+D_OVL_COL);
          updateDominoOverlay();
        }

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHT)
        if (dt+1 <= DominoTypeLastNormal )
        {
          dt = (DominoType)(dt+1);
          updateDominoOverlay();
        }

      break;

    case ST_EDIT_BG_SELECTOR:

      if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_LCTRL)
      {
        setDominoStatus(dt);
        nextState = ST_EDIT_BACKGROUND;
      }

      break;

    case ST_EDIT_HOME:

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
      {
        nextState = ST_EDIT_MENU;
        messageContinue = ST_EDIT_HOME;
      }
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F1)
      {
        nextState = ST_EDIT_HELP;
        messageContinue = ST_EDIT_HOME;
      }

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_TAB)
      {
        nextState = ST_EDIT_BACKGROUND;
        gr->setStatus(_("Background editing mode"));
      }

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == '-')
      {
        if (SDL_GetKeyState(NULL)[SDLK_LSHIFT] || SDL_GetKeyState(NULL)[SDLK_RSHIFT])
        {
          bool val = !l->getPlatform(gr->getCursorX(), gr->getCursorY());

          for (uint8_t x = gr->getCursorX(); x < gr->getCursorX()+gr->getCursorW(); x++)
            for (uint8_t y = gr->getCursorY(); y < gr->getCursorY()+gr->getCursorH(); y++)
              l->setPlatform(x, y, val);
        }
        else
        {
          // toggle platforms
          for (uint8_t x = gr->getCursorX(); x < gr->getCursorX()+gr->getCursorW(); x++)
            for (uint8_t y = gr->getCursorY(); y < gr->getCursorY()+gr->getCursorH(); y++)
              l->setPlatform(x, y, !l->getPlatform(x, y));
        }
      }

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == 'h')
      {
        if (SDL_GetKeyState(NULL)[SDLK_LSHIFT] || SDL_GetKeyState(NULL)[SDLK_RSHIFT])
        {
          bool val = !l->getLadder(gr->getCursorX(), gr->getCursorY());

          // toggle ladders
          for (uint8_t x = gr->getCursorX(); x < gr->getCursorX()+gr->getCursorW(); x++)
            for (uint8_t y = gr->getCursorY(); y < gr->getCursorY()+gr->getCursorH(); y++)
              l->setLadder(x, y, val);
        }
        else
        {
          // toggle ladders
          for (uint8_t x = gr->getCursorX(); x < gr->getCursorX()+gr->getCursorW(); x++)
            for (uint8_t y = gr->getCursorY(); y < gr->getCursorY()+gr->getCursorH(); y++)
              l->setLadder(x, y, !l->getLadder(x, y));
        }
      }

      // door placement
      if (  (event.type == SDL_KEYDOWN && event.key.keysym.sym == 'o')
          ||(event.type == SDL_KEYDOWN && event.key.keysym.sym == 'i'))
      {
        if (gr->getCursorH() != 2 || gr->getCursorW() != 1)
          gr->setStatus(_("Doors can only beplaced in 1x2 sized boxes"));
        else if (gr->getCursorY()+2 >= l->levelY())
          gr->setStatus(_("Doors can not be placed that low down in the level"));
        else
        {
          int x, y;

          if (event.type == SDL_KEYDOWN && event.key.keysym.sym == 'i')
          {
            x = l->getExitX();
            y = l->getExitY();
          }
          else
          {
            x = l->getEntryX();
            y = l->getEntryY();
          }

          if (x == gr->getCursorX() && abs(y-(gr->getCursorY()+2)) < 2)
            gr->setStatus(_("Doors must not overlap"));
          else
          {
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == 'i')
              l->setEntry(gr->getCursorX(), gr->getCursorY()+2);
            else
              l->setExit(gr->getCursorX(), gr->getCursorY()+2);
          }
        }
      }

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LCTRL)
      {
        nextState = ST_EDIT_DOMINOS_SELECTOR;
      }

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == ' ')
      {
        DominoType val = dt;
        if (l->getDominoType(gr->getCursorX(), gr->getCursorY()) != DominoTypeEmpty)
          val = DominoTypeEmpty;

        for (uint8_t x = gr->getCursorX(); x < gr->getCursorX()+gr->getCursorW(); x++)
          for (uint8_t y = gr->getCursorY(); y < gr->getCursorY()+gr->getCursorH(); y++)
            l->setDominoType(x, y, val);
      }

      handleCommonKeys(event);

      break;

    case ST_EDIT_BACKGROUND:

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
      {
        nextState = ST_EDIT_MENU;
        messageContinue = ST_EDIT_BACKGROUND;
        gr->setForegroundVisibility(true);
      }

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F1)
      {
        nextState = ST_EDIT_HELP;
        messageContinue = ST_EDIT_BACKGROUND;
      }

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_TAB)
      {
        nextState = ST_EDIT_HOME;
        gr->setStatus(_("Foreground editing mode"));
        gr->setForegroundVisibility(true);
      }

      // toggle foreground visibilty
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == 'f')
      {
        gr->setForegroundVisibility(!gr->getForegroundVisibility());
      }

      // choose layer to edit
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == '+')
      {
        if (bgLayer+1 < l->getNumBgLayer())
        {
          bgLayer++;
          gr->setBgDrawLayer(bgLayer);
        }
      }

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == '-')
      {
        if (bgLayer > 0)
        {
          bgLayer--;
          gr->setBgDrawLayer(bgLayer);
        }
      }

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F2) gr->setBgDrawMode(0);
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F3) gr->setBgDrawMode(1);
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F4) gr->setBgDrawMode(2);

      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LCTRL)
      {
        nextState = ST_EDIT_BG_SELECTOR;
      }

      handleCommonKeys(event);

      break;


    case ST_EDIT_PLAY:
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        nextState = ST_EDIT_HOME;
        levels->loadLevel(*l, levelName, "");
      }

      break;

  }

  return currentState == ST_EXIT;
}

void stepEditor(void)
{
  changeState();

  switch (currentState)
  {
    case ST_INIT:
    case ST_EXIT:
    case ST_CHOOSE_LEVEL:
    case ST_NEW_LEVEL:
    case ST_DELETE_LEVEL:
    case ST_DOUBLE_NAME:
    case ST_EDIT_MENU:
    case ST_EDIT_HELP:
      break;


    case ST_EDIT_PLAY:
      {
        unsigned int keyMask = getKeyMask();
        rec.addEvent(keyMask);
        uint16_t failReason = a->performAnimation(keyMask);

        if (l->levelInactive())
        {
          switch (failReason) {
            case LS_undecided:
              break;
            case LS_solved:
              rec.save("levels/"+levelName+"_");
              // intentionally fall throgh
            default:
              nextState = ST_EDIT_HOME;
              levels->loadLevel(*l, levelName, "");
              break;
          }
        }
      }
      // intentionally fall though to repaint the level

    case ST_EDIT_HOME:
    case ST_EDIT_BACKGROUND:
    case ST_EDIT_DOMINOS_SELECTOR:
    case ST_EDIT_BG_SELECTOR:
      gr->drawLevel();
      break;

  }
}

