#include "levelplayer.h"

#include "textsections.h"
#include "sha1.h"
#include "graphics.h"
#include "soundsys.h"
#include "screen.h"
#include "ant.h"

#include <stdio.h>

#include <sstream>
#include <iomanip>

void levelPlayer_c::load(const textsections_c & sections) {

  openDoorEntry = openDoorExit = false;
  triggerFalln = false;
  finishCheckDone = false;

  levelDisplay_c::load(sections);
}

void levelPlayer_c::performDoors(void) {

  if (openDoorEntry) {

    if (getEntryDoor() == FgElementDoor0)
      soundSystem_c::instance()->startSound(soundSystem_c::SE_DOOR_OPEN);

    if (getEntryDoor() < FgElementDoor3) {
      openEntryDoorStep();
      background.markDirty(getEntryX(), getEntryY());
      target.markDirty(getEntryX(), getEntryY());
    }

  } else {

    if (getEntryDoor() == FgElementDoor3)
      soundSystem_c::instance()->startSound(soundSystem_c::SE_DOOR_CLOSE);

    if (getEntryDoor() > FgElementDoor0) {
      closeEntryDoorStep();
      background.markDirty(getEntryX(), getEntryY());
      target.markDirty(getEntryX(), getEntryY());
    }
  }

  if (openDoorExit) {

    if (getExitDoor() == FgElementDoor0)
      soundSystem_c::instance()->startSound(soundSystem_c::SE_DOOR_OPEN);

    if (getExitDoor() < FgElementDoor3) {
      openExitDoorStep();
      background.markDirty(getExitX(), getExitY());
      target.markDirty(getExitX(), getExitY());
    }
  } else {

    if (getExitDoor() == FgElementDoor3)
      soundSystem_c::instance()->startSound(soundSystem_c::SE_DOOR_CLOSE);

    if (getExitDoor() > FgElementDoor0) {
      closeExitDoorStep();
      background.markDirty(getExitX(), getExitY());
      target.markDirty(getExitX(), getExitY());
    }
  }
}

int levelPlayer_c::pickUpDomino(int x, int y) {
  int dom = level[y][x].dominoType;
  level[y][x].dominoType = DominoTypeEmpty;
  level[y][x].dominoState = 8;
  level[y][x].dominoDir = 0;
  level[y][x].dominoYOffset = 0;
  level[y][x].dominoExtra = 0;

  return dom;
}

void levelPlayer_c::putDownDomino(int x, int y, int domino, bool pushin) {

  if (level[y][x].dominoType != 0)
  { // there is a domino in the place where we want to put our domino
    if (pushin)
      DominoCrash(x, y, domino, 0);
    else
      DominoCrash(x, y, domino, 0x70);
  }
  else if (x > 0 && (level[y][x-1].dominoType != DominoTypeEmpty) && (level[y][x-1].dominoState >= 12))
  { // there is no domino in our place but the left neighbor is falling towards us
    DominoCrash(x, y, domino, 0);
  }
  else if (x < 19 && (level[y][x+1].dominoType != DominoTypeEmpty) && level[y][x+1].dominoState <= 4)
  { // there is no domino in our place but the right neighbor is falling towards us
    DominoCrash(x, y, domino, 0);
  }
  else
  { // we can place the domino
    level[y][x].dominoType = domino;
    level[y][x].dominoState = 8;
    level[y][x].dominoDir = 0;
    level[y][x].dominoYOffset = 0;
    level[y][x].dominoExtra = 0;
  }
}

void levelPlayer_c::fallingDomino(int x, int y) {
  if (level[y][x].dominoType == DominoTypeAscender)
    level[y][x].dominoExtra = 0x60;
  else
    level[y][x].dominoExtra = 0x70;

  soundSystem_c::instance()->startSound(soundSystem_c::SE_ASCENDER);
}

bool levelPlayer_c::pushDomino(int x, int y, int dir) {

  bool retVal = true;

  if (getDominoExtra(x, y) == 0x70)
  {
    return true;
  }

  // first check for possible crashes
  switch (getDominoType(x, y)) {

    // if we want to push a splitter the stone must not be active right now
    case DominoTypeSplitter:
      if (getDominoDir(x, y) != 0)
      {
        DominoCrash(x, y, level[y][x+dir].dominoYOffset, level[y][x+dir].dominoExtra);
        return false;
      }
      break;

      // there is never a problem with those types
    case DominoTypeExploder:
    case DominoTypeDelay:
    case DominoTypeVanish:
    case DominoTypeAscender:
    case DominoTypeCrash0:
    case DominoTypeCrash1:
    case DominoTypeCrash2:
    case DominoTypeCrash3:
    case DominoTypeCrash4:
    case DominoTypeCrash5:
    case DominoTypeRiserCont:
      break;

      // the following stones must not fall against the push direction
    case DominoTypeTumbler:
    case DominoTypeBridger:
    case DominoTypeTrigger:
    case DominoTypeEmpty:
    case DominoTypeStandard:
    case DominoTypeStopper:
      if (getDominoDir(x, y) == -dir)
      {
        DominoCrash(x, y, level[y][x+dir].dominoYOffset, level[y][x+dir].dominoExtra);
        return false;
      }
      break;
  }

  // now push the dominos, we only push, if the domino is not already falling
  switch(getDominoType(x, y)) {

    // these are the default stones, they fall into the given direction
    // and immediately start falling
    case DominoTypeStandard:
    case DominoTypeTumbler:
    case DominoTypeBridger:
    case DominoTypeVanish:
    case DominoTypeTrigger:
      if (getDominoState(x, y) == 8) {
        soundSystem_c::instance()->startSound(level[y][x].dominoType-1);
        level[y][x].dominoDir = dir;
        level[y][x].dominoState += dir;
      }
      break;

      // the splitter is special it only falls in the left direction
      // even though the pieces fall in both directions
    case DominoTypeSplitter:
      if (getDominoState(x, y) == 8) {
        soundSystem_c::instance()->startSound(soundSystem_c::SE_SPLITTER);
        level[y][x].dominoDir = -1;
        level[y][x].dominoState --;
      }
      break;

      // the exploder only explosed and does not fall in one direction
      // so dir is always set to -1
      // we also need to delay the domino falling against this domino, so
      // return false
    case DominoTypeExploder:
      if (getDominoState(x, y) == 8) {
        soundSystem_c::instance()->startSound(soundSystem_c::SE_EXPLODER);
        level[y][x].dominoDir = -1;
        level[y][x].dominoState --;
        retVal = false;
      }
      else if (getDominoDir(x, y))
      {
        retVal = false;
      }
      break;

      // for the delay we additionally check, if the delay timer is
      // already running, if it is, we don't push
      // otherwise we start the delay
    case DominoTypeDelay:
      if (getDominoState(x, y) == 8) {
        if (getDominoExtra(x, y) == 0) {
          soundSystem_c::instance()->startSound(soundSystem_c::SE_DELAY);
          level[y][x].dominoDir = dir;
          level[y][x].dominoExtra = 20;

        }
        retVal = false;
      }
      break;

      // we return false then pushing the riser, because it will rise
      // and the domino has to wait
    case DominoTypeAscender:
      if (getDominoState(x, y) == 16) {
        level[y][x].dominoDir = dir;
      }
      else
      {
        if (getDominoState(x, y) == 8 && level[y][x].dominoYOffset > -6) {
          if (getDominoExtra(x, y) != 0x60) {
            soundSystem_c::instance()->startSound(soundSystem_c::SE_ASCENDER);
          }
          level[y][x].dominoDir = dir;
          level[y][x].dominoExtra = 0x60;
          retVal = false;
        }
      }
      break;

      // for this types we always return false to stop dominos
      // falling against this block
    case DominoTypeStopper:
    case DominoTypeCrash0:
    case DominoTypeCrash1:
    case DominoTypeCrash2:
    case DominoTypeCrash3:
    case DominoTypeCrash4:
    case DominoTypeCrash5:
      retVal = false;
      break;
  }

  return retVal;
}


// the 2 tigger falln functions. They just call the
// normal domino falln function and additionally set the trigger bit
void levelPlayer_c::DTA_9(int x, int y) {
  DTA_1(x, y);

  triggerFalln = true;
}
void levelPlayer_c::DTA_N(int x, int y) {
  DTA_K(x, y);

  triggerFalln = true;
}

// this is for the stopper, splitter and exploder dominos, when they
// are falling after beeing lost when going over the edge
// we check, if we are still falling and only handle the falling case
void levelPlayer_c::DTA_F(int x, int y) {
  if (getDominoExtra(x, y) == 0x70)
    DTA_E(x, y);
}

// this is for the delay domino. We check, if we are falling down when
// falling over the edge or when the delay time is up
// if the delay time is still running, decrement and wait
void levelPlayer_c::DTA_G(int x, int y) {
  if (getDominoExtra(x, y) <= 1 || getDominoExtra(x, y) == 0x70)
    DTA_E(x, y);
  else
    level[y][x].dominoExtra--;
}

// crash case with dust clouds, we need to call DTA_E because
// dominos might crash in the air and the rubble needs to fall down
void levelPlayer_c::DTA_B(int x, int y) {
  DTA_E(x, y);

  target.markDirty(x+1, y);
  target.markDirty(x+1, y-1);
}

// splitter opening simply call the normal domino falling
// function and mark some more blocks because we split left and
// right and the normal function will only mark one side
void levelPlayer_c::DTA_D(int x, int y) {
  target.markDirty(x+1, y-1);
  target.markDirty(x+1, y);

  DTA_4(x, y);

  if (getDominoState(x, y) == 5)
    target.markDirty(x, y-2);
}

// the final vansher state, remove the vanisher
// from the level and mark things dirty
void levelPlayer_c::DTA_8(int x, int y) {
  target.markDirty(x, y);
  target.markDirty(x+getDominoDir(x, y), y);
  target.markDirty(x, y-1);
  target.markDirty(x+getDominoDir(x, y), y-1);

  level[y][x].dominoType = DominoTypeEmpty;
  level[y][x].dominoDir = 0;
  level[y][x].dominoState = 0;
}

// this is the nearly falln down left case
void levelPlayer_c::DTA_2(int x, int y) {

  if (getDominoExtra(x, y) == 0x40 || getDominoExtra(x, y) == 0x70)
  {
    DTA_4(x, y);
    return;
  }

  // if we move in the other direction (e.g. tumbler) simply continue
  if (getDominoDir(x, y) == 1)
  {
    DTA_4(x, y);
    return;
  }

  // if there is a domino left of us, we can not fall down further
  if (getDominoType(x-1, y) != 0)
    return;

  // if here is a domino 2 to the left of us and that domino has falln down
  // far enough to the right, we can not fall farther
  if (x >= 2 && getDominoType(x-2, y) != 0 && getDominoState(x-2, y) > 13)
    return;

  // if the domino is a splitter, we need to check, if it is in one of the
  // states where the left halve is far enough down
  // if this is not the case or it is no splitter fall further
  if (getDominoType(x-2, y) != DominoTypeSplitter ||
      getDominoState(x-2, y) != 1 &&
      getDominoState(x-2, y) != 10 &&
      getDominoState(x-2, y) != 12 &&
      getDominoState(x-2, y) != 13)
  {
    DTA_4(x, y);
  }
}

// nearly falln down right
void levelPlayer_c::DTA_J(int x, int y) {

  if (getDominoExtra(x, y) == 0x40 || getDominoExtra(x, y) == 0x70)
  {
    DTA_4(x, y);
    return;
  }

  // if we fall in the other direction (tumbler) we simply contnue
  if (getDominoDir(x, y) == -1)
  {
    DTA_4(x, y);
    return;
  }

  // if our right neighbor if not empty, we stop
  if (getDominoType(x+1, y) != 0)
    return;

  // if the 2nd next right neighbor is not empty and far enough falln to the left
  // we stop
  if (x <= 17 && getDominoType(x+2, y) != 0 && getDominoState(x+2, y) < 3)
    return;

  // if the domino is a splitter, we need to check, if it is in one of the
  // states where the right halve is far enough down
  // if this is not the case or it is no splitter fall further
  if (getDominoType(x+2, y) != DominoTypeSplitter ||
      getDominoState(x+2, y) != 1 &&
      getDominoState(x+2, y) != 9 &&
      getDominoState(x+2, y) != 11 &&
      getDominoState(x+2, y) != 14)
  {
    DTA_4(x, y);
  }
}

// normal falling case
void levelPlayer_c::DTA_4(int x, int y) {
  if (getDominoExtra(x, y) == 0x40 || getDominoExtra(x, y) == 0x70)
  {
    level[y][x].dominoYOffset += 2;
    level[y][x].dominoState += getDominoDir(x, y);
  }
  else if (getDominoExtra(x, y) == 0x60)
  {
    level[y][x].dominoYOffset -= 2;
    level[y][x].dominoState += getDominoDir(x, y);
  }

  // update state
  level[y][x].dominoState += getDominoDir(x, y);

  if (getDominoType(x, y) == DominoTypeAscender &&
      getDominoState(x, y) == 8 &&
      level[y][x].dominoYOffset == -10)
  {
    level[y][x].dominoState = 16;
  }

  target.markDirty(x, y);
  target.markDirty(x, y-1);

  // add some dirty blocks depending on the direction we have falln
  if (getDominoState(x, y) > 8)
  {
    target.markDirty(x+1, y);
    target.markDirty(x+1, y-1);
  }

  if (getDominoState(x, y) < 8)
  {
    target.markDirty(x-1, y);
    target.markDirty(x-1, y-1);
  }

  if (level[y][x].dominoYOffset == 8)
  {
    target.markDirty(x+getDominoDir(x, y), y);
    target.markDirty(x+getDominoDir(x, y), y-1);
  }

  if (level[y][x].dominoType == DominoTypeAscender)
  {
    target.markDirty(x+getDominoDir(x, y), y-2);
    target.markDirty(x-getDominoDir(x, y), y-2);
    target.markDirty(x, y-2);
  }
}

// exploder making its hole
void levelPlayer_c::DTA_5(int x, int y) {

  level[y][x].dominoType = 0;
  level[y][x].dominoState = 0;
  level[y][x].fg = FgElementEmpty;

  if (level[y][x+1].fg == FgElementPlatformMiddle ||
      level[y][x+1].fg == FgElementPlatformLadderDown ||
      level[y][x+1].fg == FgElementPlatformLadderUp)
  {
    level[y][x+1].fg = FgElementPlatformStart;
  }
  else if (level[y][x+1].fg == FgElementPlatformEnd)
  {
    level[y][x+1].fg = FgElementPlatformStrip;
  }

  if (level[y][x-1].fg == FgElementPlatformMiddle ||
      level[y][x-1].fg == FgElementPlatformLadderDown ||
      level[y][x-1].fg == FgElementPlatformLadderUp)
  {
    level[y][x-1].fg = FgElementPlatformEnd;
  }
  else if (level[y][x-1].fg == FgElementPlatformStart)
  {
    level[y][x-1].fg = FgElementPlatformStrip;
  }

  target.markDirty(x, y);
  target.markDirty(x-1, y);
  target.markDirty(x+1, y);

  background.markDirty(x-1, y);
  background.markDirty(x, y);
  background.markDirty(x+1, y);
}

// hitting next domino to the left
void levelPlayer_c::DTA_3(int x, int y) {

  // if we hit a step, stop falling
  if (x > 0 && getFg(x-1, y) == FgElementPlatformStep4)
    return;

  if (level[y][x].dominoYOffset == 8 && x > 0 && getFg(x-1, y) == FgElementPlatformStep1)
    return;

  // if the next somino is empty, continue falling
  if (getDominoType(x-1, y) == DominoTypeEmpty) {
    DTA_4(x, y);
    return;
  }

  // if the next domino is not a stopper and not a delay, we
  // simply push that domino and continue falling
  if (getDominoType(x-1, y) != DominoTypeStopper &&
      getDominoType(x-1, y) != DominoTypeDelay)
  {
    if (pushDomino(x-1, y, -1))
      DTA_4(x, y);
    return;
  }

  // if the next neibor is a delay we only continue falling, if
  // it has started falling as well
  if (getDominoType(x-1, y) == DominoTypeDelay)
  {
    if (getDominoState(x-1, y) != 8) {
      DTA_4(x, y);
      return;
    }
  }

  // if the right neighbor is a splitter and the splitter is already falln
  // we try to continue falling
  if (getDominoType(x+1, y) == DominoTypeSplitter && getDominoState(x+1, y) != 8)
  {
    if (pushDomino(x-1, y, -1))
      DTA_4(x, y);
    return;
  }

  // if there is a domino to the right of us that has falln we try to continue
  if (getDominoType(x+1, y) != DominoTypeEmpty && getDominoState(x+1, y) < 8)
  {
    if (pushDomino(x-1, y, -1))
      DTA_4(x, y);
    return;
  }

  // now the only case left is to reverse direction but still push the domino to our left
  level[y][x].dominoDir = 1;
  pushDomino(x-1, y, -1);
  soundSystem_c::instance()->startSound(soundSystem_c::SE_STOPPER);
  DTA_4(x, y);
}

// same as DTA_3 but for the right direction
void levelPlayer_c::DTA_I(int x, int y) {

  if (x < 19 && getFg(x+1, y) == FgElementPlatformStep7)
    return;

  if (level[y][x].dominoYOffset == 8 && x < 19 && getFg(x-1, y) == FgElementPlatformStep6)
    return;

  if (getDominoType(x+1, y) == DominoTypeEmpty) {
    DTA_4(x, y);
    return;
  }

  if (getDominoType(x+1, y) != DominoTypeStopper &&
      getDominoType(x+1, y) != DominoTypeDelay)
  {
    if (pushDomino(x+1, y, 1))
      DTA_4(x, y);
    return;
  }

  if (getDominoType(x+1, y) == DominoTypeDelay)
  {
    if (getDominoState(x+1, y) != 8) {
      DTA_4(x, y);
      return;
    }
  }

  if (getDominoType(x-1, y) == DominoTypeSplitter && getDominoState(x-1, y) != 8)
  {
    if (pushDomino(x+1, y, 1))
      DTA_4(x, y);
    return;
  }

  if (getDominoType(x-1, y) != DominoTypeEmpty && getDominoState(x-1, y) > 8)
  {
    if (pushDomino(x+1, y, 1))
      DTA_4(x, y);
    return;
  }

  level[y][x].dominoDir = -1;
  pushDomino(x+1, y, 1);
  soundSystem_c::instance()->startSound(soundSystem_c::SE_STOPPER);
  DTA_4(x, y);
}

// handle dominos crashing into something
void levelPlayer_c::DominoCrash(int x, int y, int type, int extra) {

  // what do we crash into?
  int next = level[y][x].dominoType;

  // depending on what crashed we get a new pile

  // standard + standard     -> DominoCarsh3 little yellow pile
  // standard + DominoCrash3 -> DominoCarsh0 big yellow pile
  // DominoCrash3 + other    -> DominoCrash1 big mixed pile
  // blocker + blocker       -> DominoCrash5 little red pile
  // blocker + DominoCrash5  -> DominoCrash2 big red pile
  // ...
  //



  // for all combinations for participants do whats shown in the table above

  if (next == DominoTypeStandard)
  {
         if (type == DominoTypeStandard)                                                         next = DominoTypeCrash3;
    else if (type == DominoTypeCrash0 || type == DominoTypeCrash3)                               next = DominoTypeCrash0;
    else if (type < DominoTypeCrash0)                                                            next = DominoTypeCrash4;
    else                                                                                         next = DominoTypeCrash1;
  }
  else if (next == DominoTypeCrash3 || next == DominoTypeCrash0)
  {
         if (type == DominoTypeStandard || type == DominoTypeCrash0 || type == DominoTypeCrash3) next = DominoTypeCrash0;
    else                                                                                         next = DominoTypeCrash1;
  }
  else if (next == DominoTypeStopper)
  {
         if (type == DominoTypeStopper)                                                          next = DominoTypeCrash5;
    else if (type == DominoTypeCrash2 || type == DominoTypeCrash5)                               next = DominoTypeCrash2;
    else if (type < DominoTypeCrash0)                                                            next = DominoTypeCrash4;
    else                                                                                         next = DominoTypeCrash1;
  }
  else if (next == DominoTypeCrash5 || next == DominoTypeCrash2)
  {
         if (type == DominoTypeStopper || type == DominoTypeCrash2 || type == DominoTypeCrash5)  next = DominoTypeCrash2;
    else                                                                                         next = DominoTypeCrash1;
  }
  else if (next < DominoTypeCrash0)
  {
         if (type < DominoTypeCrash0)                                                            next = DominoTypeCrash4;
    else                                                                                         next = DominoTypeCrash1;
  }
  else
  {
    next = DominoTypeCrash1;
  }

  // set the resulting domino animation
  level[y][x].dominoType = next;
  level[y][x].dominoState = 1;
  level[y][x].dominoDir = 1;
  if (level[y][x].dominoExtra == 0x70 || extra == 0x70 || level[y][x].dominoExtra == 0x60 || extra == 0x60)
  {
    level[y][x].dominoExtra = 0x70;
    level[y][x].dominoYOffset &= 0xFC;
  }
  else
  {
    level[y][x].dominoExtra = 0;
  }

  target.markDirty(x-1, y);
  target.markDirty(x, y);
  target.markDirty(x+1, y);

  soundSystem_c::instance()->startSound(soundSystem_c::SE_EXPLODER);
}

// vertial stone
void levelPlayer_c::DTA_E(int x, int y) {

  if (level[y][x].dominoExtra == 0x40)
  {
    level[y][x].dominoExtra = 0;
  }

  // start falling to the side, once we hit the ground and the
  // domino had been given a direction
  if (level[y][x].dominoExtra != 0x70)
  {
    if (level[y][x].dominoDir != 0)
    {
      DTA_4(x, y);
    }
    return;
  }

  // continue falling down
  if (level[y][x].dominoYOffset != 0)
  {
    // if we have not yet reached the next level of the level data
    if (level[y][x].dominoYOffset != 12) {
      level[y][x].dominoYOffset += 4;
      target.markDirty(x, y-1);
      target.markDirty(x, y);
      return;
    }

    // if we have not yet fallen out of the level put
    // the domino into the next block below
    if (y < 12)
    {
      level[y+1][x].dominoType = level[y][x].dominoType;
      level[y+1][x].dominoState = level[y][x].dominoState;
      level[y+1][x].dominoDir = level[y][x].dominoDir;
      level[y+1][x].dominoYOffset = 0;
      level[y+1][x].dominoExtra = 0x70;
    }

    // remove the old domino
    level[y][x].dominoType = DominoTypeEmpty;
    level[y][x].dominoState = 0;
    level[y][x].dominoDir = 0;
    level[y][x].dominoYOffset = 0;
    level[y][x].dominoExtra = 0;

    target.markDirty(x, y+1);
    target.markDirty(x, y);
    target.markDirty(x, y-1);
    return;
  }

  // we have reached a special position where we need to check
  // for the ground again
  if (isTherePlatform(x, y))
  {
    // we still crash if there is a domino below us
    if (getDominoType(x, y+1) != DominoTypeEmpty)
    {
      DominoCrash(x, y+1, level[y][x].dominoType, level[y][x].dominoExtra);
    }

    // no more falling
    level[y][x].dominoExtra = 0;

    // we need to set the yOffset properly for the halve steps
    if (level[y][x].fg == 8 || level[y][x].fg == 11)
    {
      level[y][x].dominoYOffset = 8;
    }

    target.markDirty(x, y);
    target.markDirty(x, y+1);
    return;
  }

  // no ground below us, continue falling

  // we can continue, if there is either no domino or no more
  // level below us
  if (y >= 12 || level[y+1][x].dominoType == DominoTypeEmpty)
  {
    level[y][x].dominoYOffset += 4;
    target.markDirty(x, y);
    target.markDirty(x, y+1);
    return;
  }

  // if there is no splitter below us, we crash
  if (level[y+1][x].dominoType != DominoTypeSplitter)
  {
    DominoCrash(x, y+1, level[y][x].dominoType, level[y][x].dominoExtra);

    level[y][x].dominoType = DominoTypeEmpty;
    level[y][x].dominoState = 0;
    level[y][x].dominoDir = 0;
    level[y][x].dominoYOffset = 0;
    level[y][x].dominoExtra = 0;

    target.markDirty(x, y);
    target.markDirty(x, y+1);

    return;
  }

  // there is a splitter below us, so start that splitter
  // and we vanish
  pushDomino(x, y+1, -1);

  level[y+1][x].dominoExtra = level[y][x].dominoType;
  level[y][x].dominoType = DominoTypeEmpty;
  level[y][x].dominoState = 0;
  level[y][x].dominoDir = 0;
  level[y][x].dominoYOffset = 0;
  level[y][x].dominoExtra = 0;

  target.markDirty(x, y);
  target.markDirty(x, y+1);
}

// splitter parts falling further
void levelPlayer_c::DTA_C(int x, int y) {

  // this table contains the positions of the 2 splitter halves
  // for each splitter state, 8 = vertial, 1 = horizontal
  static const int SplitterLookup[] = {
    0, 0,
    1, 1,
    2, 2,
    3, 3,
    4, 4,
    5, 5,
    6, 6,
    7, 7,
    8, 8,
    2, 1,
    1, 2,
    3, 1,
    1, 3,
    2, 3,
    3, 2
  };

  // first find the positions of the 2 halves for the
  // current state
  int a = SplitterLookup[level[y][x].dominoState*2+1];
  int b = SplitterLookup[level[y][x].dominoState*2];

  // calculate the new position of the left halve
  if (a == 3)
  {
    // left halve is at a pushing place, check if we hit the wall
    if (x > 0 && level[y][x-1].fg != 0xA)
    {
      // no wall, check, if we hit a domino and if so try to push it
      if (level[y][x-1].dominoType == DominoTypeEmpty)
      {
        a--;
      }
      else if (pushDomino(x-1, y, -1))
      {
        a--;
      }
    }
  }
  else if (a == 2 && level[y][x-1].dominoType == 0)
  {
    a--;
  }

  // same as above but for right halve
  if (b == 3)
  {
    if (x < 19)
    {
      if (level[y][x+1].dominoType == DominoTypeEmpty)
      {
        b--;
      }
      else if (pushDomino(x+1, y, 1))
      {
        b--;
      }
    }
  }
  else if (b == 2 && level[y][x+1].dominoType == 0)
  {
    b--;
  }

  // now find the new state of the plitter domino
  for (int i = 0; i < 15; i++)
  {
    if (SplitterLookup[2*i+1] == a && SplitterLookup[2*i] == b)
    {
      if (level[y][x].dominoState != i)
      {
        level[y][x].dominoState = i;
        target.markDirty(x, y);
        target.markDirty(x+1, y);
        target.markDirty(x-1, y);
        target.markDirty(x, y-1);
        target.markDirty(x-1, y-1);
        target.markDirty(x+1, y-1);
      }
      return;
    }
  }

  // this must not happen...
  printf("oops missing splitter image");
}

// bridger left this is mainly a lot of ifs to
// find out the new level elements that need to
// be placed
void levelPlayer_c::DTA_7(int x, int y) {

  int fg2;

  if (x >= 2)
  {
    fg2 = level[y][x-2].fg;
  }
  else
  {
    fg2 = 0;
  }

  int fg1;

  if (x >= 1)
  {
    fg1 = level[y][x-1].fg;
  }
  else
  {
    fg1 = 0;
  }

  int fg = level[y][x].fg;

  if (fg != FgElementPlatformStart && fg != FgElementPlatformStrip)
  {
    DTA_1(x, y);
    return;
  }

  int doit = 0;

  if (fg1 == FgElementPlatformEnd)
  {
    fg1 = FgElementPlatformMiddle;
    if (fg == FgElementPlatformStart)
    {
      fg = FgElementPlatformMiddle;
    }
    else
    {
      fg = FgElementPlatformEnd;
    }
    doit = 1;
  }
  else if (fg1 != FgElementEmpty || fg2 != FgElementPlatformEnd)
  {
    if (fg1 == FgElementPlatformStrip)
    {
      fg1 = FgElementPlatformStart;
      if (fg == FgElementPlatformStart)
      {
        fg = FgElementPlatformMiddle;
      }
      else
      {
        fg = FgElementPlatformEnd;
      }
      doit = 1;
    }
    else
    {
      if (fg1 == FgElementEmpty && fg2 == FgElementPlatformStrip)
      {
        fg2 = FgElementPlatformStart;
        fg1 = FgElementPlatformMiddle;
        if (fg == FgElementPlatformStart)
        {
          fg = FgElementPlatformMiddle;
        }
        else
        {
          fg = FgElementPlatformEnd;
        }
        doit = 1;
      }
    }
  }
  else
  {
    fg2 = FgElementPlatformMiddle;
    fg1 = FgElementPlatformMiddle;
    if (fg == FgElementPlatformStart)
    {
      fg = FgElementPlatformMiddle;
    }
    else
    {
      fg = FgElementPlatformEnd;
    }
    doit = 1;
  }

  // if we can't build a bridge, we continue falling, maybe topple over and
  // continue downwards
  if (doit == 0)
  {
    DTA_1(x, y);
    return;
  }

  if (x >= 2)
  {
    level[y][x-2].fg = fg2;
    target.markDirty(x-2, y);
  }

  if (x >= 1)
  {
    level[y][x-1].fg = fg1;
  }

  level[y][x].fg = fg;
  level[y][x].dominoType = 0;
  level[y][x].dominoDir = 0;
  target.markDirty(x, y);
  target.markDirty(x-1, y);
  target.markDirty(x, y-1);
  target.markDirty(x-1, y-1);

  background.markDirty(x-2, y);
  background.markDirty(x-1, y);
  background.markDirty(x  , y);
}

// Brider right same as DTA_7 but for other direction
void levelPlayer_c::DTA_M(int x, int y) {

  int fg2;

  if (x < 18)
  {
    fg2 = level[y][x+2].fg;
  }
  else
  {
    fg2 = 0;
  }

  int fg1;

  if (x < 19)
  {
    fg1 = level[y][x+1].fg;
  }
  else
  {
    fg1 = 0;
  }

  int fg = level[y][x].fg;

  if (fg != FgElementPlatformEnd && fg != FgElementPlatformStrip)
  {
    DTA_K(x, y);
    return;
  }

  int doit = 0;

  if (fg1 == FgElementPlatformStart)
  {
    if (fg == FgElementPlatformEnd)
    {
      fg = FgElementPlatformMiddle;
    }
    else
    {
      fg = FgElementPlatformStart;
    }
    fg1 = FgElementPlatformMiddle;
    doit = 1;
  }
  else if (fg1 != FgElementEmpty || fg2 != FgElementPlatformStart)
  {
    if (fg1 == FgElementPlatformStrip)
    {
      fg1 = FgElementPlatformStart;
      if (fg == FgElementPlatformEnd)
      {
        fg = FgElementPlatformMiddle;
      }
      else
      {
        fg = FgElementPlatformStart;
      }
      doit = 1;
    }
    else
    {
      if (fg1 == FgElementEmpty && fg2 == FgElementPlatformStrip)
      {
        fg2 = FgElementPlatformEnd;
        fg1 = FgElementPlatformMiddle;
        if (fg == FgElementPlatformEnd)
        {
          fg = FgElementPlatformMiddle;
        }
        else
        {
          fg = FgElementPlatformStart;
        }
        doit = 1;
      }
    }
  }
  else
  {
    fg2 = FgElementPlatformMiddle;
    fg1 = FgElementPlatformMiddle;
    if (fg == FgElementPlatformEnd)
    {
      fg = FgElementPlatformMiddle;
    }
    else
    {
      fg = FgElementPlatformStart;
    }
    doit = 1;
  }

  if (doit == 0)
  {
    DTA_K(x, y);
    return;
  }

  if (x < 18)
  {
    level[y][x+2].fg = fg2;
    target.markDirty(x+2, y);
  }

  if (x < 19)
  {
    level[y][x+1].fg = fg1;
  }

  level[y][x].fg = fg;
  level[y][x].dominoType = 0;
  level[y][x].dominoDir = 0;
  target.markDirty(x, y);
  target.markDirty(x+1, y);
  target.markDirty(x, y-1);
  target.markDirty(x+1, y-1);

  background.markDirty(x  , y);
  background.markDirty(x+1, y);
  background.markDirty(x+2, y);
}


// riser
void levelPlayer_c::DTA_A(int x, int y) {

  int a;

  if (level[y][x].dominoExtra == 0x50)
    a = 1;
  else
    a = 2;

  int b;
  if (x > 0)
    b = level[y-a][x-1].fg;
  else
    b = FgElementEmpty;

  int c = level[y-a][x].fg;

  if ((c == FgElementPlatformStart || c == FgElementPlatformStrip) &&
      b == FgElementEmpty)
  {
    if (a == 1)
    {
      level[y][x-1].dominoExtra = 0x60;
      level[y][x-1].dominoType = DominoTypeAscender;
      level[y][x-1].dominoState = 14;
      level[y][x-1].dominoDir = -1;
      level[y][x-1].dominoYOffset = level[y][x].dominoYOffset-2;
    }
    else
    {
      if (y > 0)
      {
        level[y-1][x-1].dominoExtra = 0x60;
        level[y-1][x-1].dominoType = DominoTypeAscender;
        level[y-1][x-1].dominoState = 14;
        level[y-1][x-1].dominoDir = -1;
        level[y-1][x-1].dominoYOffset = level[y][x].dominoYOffset+14;
      }
    }

    level[y][x].dominoType = DominoTypeEmpty;
    level[y][x].dominoState = 0;
    level[y][x].dominoDir = 0;
    level[y][x].dominoYOffset = 0;
    level[y][x].dominoExtra = 0;

    target.markDirty(x, y-a);
    target.markDirty(x-1, y-a);
    target.markDirty(x, y-a+1);
    target.markDirty(x-1, y-a+1);

    return;
  }

  if (c == 0)
  {
    level[y][x].dominoType = DominoTypeEmpty;
    level[y][x].dominoState = 0;
    level[y][x].dominoDir = 0;
    level[y][x].dominoYOffset = 0;
    level[y][x].dominoExtra = 0;

    level[y+1-a][x].dominoExtra = 0x60;
    level[y+1-a][x].dominoType = DominoTypeAscender;
    level[y+1-a][x].dominoState = 8;
    level[y+1-a][x].dominoDir = -1;
    level[y+1-a][x].dominoYOffset = 0;

    target.markDirty(x, y-a);
    target.markDirty(x-1, y-a);
    target.markDirty(x, y-a+1);
    target.markDirty(x-1, y-a+1);

    return;
  }

  if (level[y][x].dominoExtra != 0x50)
  {
    level[y-1][x].dominoType = level[y][x].dominoType;
    level[y-1][x].dominoState = level[y][x].dominoState;
    level[y-1][x].dominoDir = level[y][x].dominoDir;
    level[y-1][x].dominoYOffset = level[y][x].dominoYOffset+16;
    level[y-1][x].dominoExtra = 0x50;

    level[y][x].dominoType = DominoTypeEmpty;
    level[y][x].dominoState = 0;
    level[y][x].dominoDir = 0;
    level[y][x].dominoYOffset = 0;
    level[y][x].dominoExtra = 0;
  }
}


// Riser
void levelPlayer_c::DTA_O(int x, int y) {

  int a;

  if (level[y][x].dominoExtra == 0x50)
    a = 1;
  else
    a = 2;

  int b;
  if (x < 19)
    b = level[y-a][x+1].fg;
  else
    b = FgElementEmpty;

  int c = level[y-a][x].fg;

  if ((c == FgElementPlatformEnd || c == FgElementPlatformStrip) &&
      b == FgElementEmpty)
  {
    if (a == 1)
    {
      if (level[y][x+1].dominoType == DominoTypeEmpty)
      {
        level[y][x+1].dominoExtra = 0x60;
        level[y][x+1].dominoType = DominoTypeAscender;
        level[y][x+1].dominoState = 2;
        level[y][x+1].dominoDir = 1;
        level[y][x+1].dominoYOffset = level[y][x].dominoYOffset-2;
      }
      else
      {
        DominoCrash(x+1, y, level[y][x].dominoType, level[y][x].dominoExtra);
      }
    }
    else
    {
      if (y > 0)
      {
        if (level[y-1][x+1].dominoType == DominoTypeEmpty)
        {
          level[y-1][x+1].dominoExtra = 0x60;
          level[y-1][x+1].dominoType = DominoTypeAscender;
          level[y-1][x+1].dominoState = 2;
          level[y-1][x+1].dominoDir = 1;
          level[y-1][x+1].dominoYOffset = level[y][x].dominoYOffset+14;
        }
        else
        {
          DominoCrash(x+1, y-1, level[y][x].dominoType, level[y][x].dominoExtra);
        }
      }
    }

    level[y][x].dominoType = DominoTypeEmpty;
    level[y][x].dominoState = 0;
    level[y][x].dominoDir = 0;
    level[y][x].dominoYOffset = 0;
    level[y][x].dominoExtra = 0;

    target.markDirty(x, y-a);
    target.markDirty(x+1, y-a);
    target.markDirty(x, y-a+1);
    target.markDirty(x+1, y-a+1);

    return;
  }

  if (c == 0)
  {
    level[y][x].dominoType = DominoTypeEmpty;
    level[y][x].dominoState = 0;
    level[y][x].dominoDir = 0;
    level[y][x].dominoYOffset = 0;
    level[y][x].dominoExtra = 0;

    if (level[y+1-a][x].dominoType == DominoTypeEmpty)
    {
      level[y+1-a][x].dominoExtra = 0x60;
      level[y+1-a][x].dominoType = DominoTypeAscender;
      level[y+1-a][x].dominoState = 8;
      level[y+1-a][x].dominoDir = 1;
      level[y+1-a][x].dominoYOffset = 0;
    }
    else
    {
      DominoCrash(x, y+1-a, level[y][x].dominoType, level[y][x].dominoExtra);
    }

    target.markDirty(x, y-a);
    target.markDirty(x+1, y-a);
    target.markDirty(x, y-a+1);
    target.markDirty(x+1, y-a+1);

    return;
  }

  if (level[y][x].dominoExtra != 0x50)
  {
    level[y-1][x].dominoType = level[y][x].dominoType;
    level[y-1][x].dominoState = level[y][x].dominoState;
    level[y-1][x].dominoDir = level[y][x].dominoDir;
    level[y-1][x].dominoYOffset = level[y][x].dominoYOffset+16;
    level[y-1][x].dominoExtra = 0x50;

    level[y][x].dominoType = DominoTypeEmpty;
    level[y][x].dominoState = 0;
    level[y][x].dominoDir = 0;
    level[y][x].dominoYOffset = 0;
    level[y][x].dominoExtra = 0;
  }
}

// riser risign vertically
void levelPlayer_c::DTA_H(int x, int y) {

  int riserDir = level[y][x].dominoDir;

  if (level[y][x].dominoExtra == 0x60)
  {
    if (level[y][x].dominoYOffset == 4 && y > 1)
    {
      if (isTherePlatform(x, y-2))
      {
        if (level[y-1][x].fg == 0xA || level[y-1][x].fg == 0xD)
        {
          level[y][x].dominoState = 16;
          level[y][x].dominoExtra = 0x50;

          target.markDirty(x, y);
          target.markDirty(x, y-2);
          target.markDirty(x, y-1);
          return;
        }
      }
    }
    if (level[y][x].dominoYOffset == -6 && y > 1)
    {
      if (isTherePlatform(x, y-2))
      {
        if (level[y-1][x].fg == 9 || level[y-1][x].fg == 0xE)
        {
          level[y][x].dominoState = 16;
          level[y][x].dominoExtra = 0x50;
          target.markDirty(x, y);
          target.markDirty(x, y-2);
          target.markDirty(x, y-1);
          return;
        }
      }
    }
    if (level[y][x].dominoYOffset == -10)
    {
      if (y > 1)
      {
        if (isTherePlatform(x, y-2))
        {
          level[y][x].dominoState = 16;
          level[y][x].dominoExtra = 0;

          target.markDirty(x, y);
          target.markDirty(x, y-2);
          target.markDirty(x, y-1);
          return;
        }
      }

      if (y > 0)
      {
        if (level[y-1][x].dominoType == DominoTypeEmpty)
        {
          level[y-1][x].dominoExtra = 0x60;
          level[y-1][x].dominoYOffset = 4;
          level[y-1][x].dominoDir = level[y][x].dominoDir;
          level[y-1][x].dominoState = 8;
          level[y-1][x].dominoType = DominoTypeAscender;
        }
        else
        {
          DominoCrash(x, y-1, level[y][x].dominoType, level[y][x].dominoExtra);
        }
      }

      level[y][x].dominoType = 0;
      level[y][x].dominoState = 0;
      level[y][x].dominoDir = 0;
      level[y][x].dominoYOffset = 0;
      level[y][x].dominoExtra = 0;

      target.markDirty(x, y);
      target.markDirty(x, y-2);
      target.markDirty(x, y-1);
      return;
    }

    if (level[y][x].dominoYOffset == -8 && y > 1)
    {
      if (isTherePlatform(x, y-2))
      {
        level[y][x].dominoState = 16;
      }
    }

    if (level[y][x].dominoYOffset == -6 && y > 1)
    {
      if (isTherePlatform(x, y-2))
      {
        level[y][x].dominoState = 17;
      }
    }
    level[y][x].dominoYOffset -= 2;

    target.markDirty(x, y);
    target.markDirty(x, y-2);
    target.markDirty(x, y-1);
    return;
  }

  if (riserDir != 0)
  {
    if (level[y][x].dominoState == 16)
      level[y][x].dominoState = 8;

    DTA_4(x, y);
  }
}

// Stone completely falln down right used for
// standard, Trigger, Delay, Bridger
void levelPlayer_c::DTA_K(int x, int y) {
  int fg;

  if (x < 19)
    fg = level[y][x+1].fg;
  else
    fg = 0;

  if (fg == FgElementLadder)
    fg = 0;

  if ((getFg(x, y) == FgElementPlatformEnd ||
        getFg(x, y) == FgElementPlatformStrip)
      &&
      fg == FgElementEmpty
     )
  {

    if (getDominoType(x+1, y+1) != DominoTypeEmpty)
    {
      DominoCrash(x+1, y+1, getDominoType(x, y), getDominoExtra(x, y));
      level[y][x].dominoType = 0;
      level[y][x].dominoState = 0;
      level[y][x].dominoDir = 0;

      target.markDirty(x, y-1);
      target.markDirty(x+1, y-1);
      target.markDirty(x, y);
      target.markDirty(x+1, y);
    }
    else
    {
      level[y][x+1].dominoType = getDominoType(x, y);
      level[y][x+1].dominoState = 2;
      level[y][x+1].dominoDir = getDominoDir(x, y);
      level[y][x+1].dominoYOffset = 2;
      level[y][x+1].dominoExtra = 0x70;

      soundSystem_c::instance()->startSound(soundSystem_c::SE_ASCENDER);

      level[y][x].dominoType = 0;
      level[y][x].dominoState = 0;
      level[y][x].dominoDir = 0;

      target.markDirty(x, y-1);
      target.markDirty(x+1, y-1);
      target.markDirty(x, y);
      target.markDirty(x+1, y);
    }
  }
  else
  {
    if (x < 19 && level[y][x].fg == FgElementPlatformStep1 && fg == FgElementPlatformStep2)
    {
      if (level[y][x+1].dominoType != DominoTypeEmpty)
      {
        DominoCrash(x+1, y, level[y][x].dominoType, level[y][x].dominoExtra);
      }
      else
      {
        level[y][x+1].dominoType = level[y][x].dominoType;
        level[y][x+1].dominoState = 2;
        level[y][x+1].dominoDir = level[y][x].dominoDir;
        level[y][x+1].dominoYOffset = 2;
        level[y][x+1].dominoExtra = 0x40;
      }

      level[y][x].dominoType = 0;
      level[y][x].dominoState = 0;
      level[y][x].dominoDir = 0;
      level[y][x].dominoYOffset = 0;

      target.markDirty(x, y-1);
      target.markDirty(x+1, y-1);
      target.markDirty(x, y);
      target.markDirty(x+1, y);

      return;
    }

    if (x < 19 && y < 11 && getFg(x, y) == FgElementPlatformStep2)
    {
      if (level[y+1][x+1].dominoType != 0)
      {
        DominoCrash(x+1, y+1, level[y][x].dominoType, level[y][x].dominoExtra);
      }
      else
      {
        level[y+1][x+1].dominoType = level[y][x].dominoType;
        level[y+1][x+1].dominoState = 2;
        level[y+1][x+1].dominoDir = level[y][x].dominoDir;
        level[y+1][x+1].dominoYOffset = -6;
        level[y+1][x+1].dominoExtra = 0x40;
      }

      level[y][x].dominoType = 0;
      level[y][x].dominoState = 0;
      level[y][x].dominoDir = 0;
      level[y][x].dominoYOffset = 0;

      target.markDirty(x, y-1);
      target.markDirty(x+1, y-1);
      target.markDirty(x, y);
      target.markDirty(x+1, y);

      return;
    }

    level[y][x].dominoDir = 0;
    return;
  }

}

// Stone completely falln down left used for
// standard, Trigger, Delay, Bridger
void levelPlayer_c::DTA_1(int x, int y) {

  int fg;

  if (x > 0)
    fg = level[y][x-1].fg;
  else
    fg = 0;

  if (fg == FgElementLadder)
    fg = 0;

  if ((getFg(x, y) == FgElementPlatformStart ||
        getFg(x, y) == FgElementPlatformStrip)
      &&
      fg == FgElementEmpty
     )
  {

    if (getDominoType(x-1, y+1) != DominoTypeEmpty)
    {
      DominoCrash(x-1, y+1, getDominoType(x, y), getDominoExtra(x, y));
      level[y][x].dominoType = 0;
      level[y][x].dominoState = 0;
      level[y][x].dominoDir = 0;

      target.markDirty(x, y-1);
      target.markDirty(x-1, y-1);
      target.markDirty(x, y);
      target.markDirty(x-1, y);

      return;
    }
    else
    {
      level[y][x-1].dominoType = getDominoType(x, y);
      level[y][x-1].dominoState = 0xE;
      level[y][x-1].dominoDir = getDominoDir(x, y);
      level[y][x-1].dominoYOffset = 2;
      level[y][x-1].dominoExtra = 0x70;

      soundSystem_c::instance()->startSound(soundSystem_c::SE_ASCENDER);

      level[y][x].dominoType = 0;
      level[y][x].dominoState = 0;
      level[y][x].dominoDir = 0;

      target.markDirty(x, y-1);
      target.markDirty(x-1, y-1);
      target.markDirty(x, y);
      target.markDirty(x-1, y);
    }
  }
  else
  {
    if (x > 0 && level[y][x].fg == FgElementPlatformStep6 && fg == FgElementPlatformStep5)
    {
      if (level[y][x-1].dominoType != DominoTypeEmpty)
      {
        DominoCrash(x-1, y, level[y][x].dominoType, level[y][x].dominoExtra);
      }
      else
      {
        level[y][x-1].dominoType = level[y][x].dominoType;
        level[y][x-1].dominoState = 0xE;
        level[y][x-1].dominoDir = level[y][x].dominoDir;
        level[y][x-1].dominoYOffset = 2;
        level[y][x-1].dominoExtra = 0x40;
      }

      level[y][x].dominoType = 0;
      level[y][x].dominoState = 0;
      level[y][x].dominoDir = 0;
      level[y][x].dominoYOffset = 0;

      target.markDirty(x, y-1);
      target.markDirty(x-1, y-1);
      target.markDirty(x, y);
      target.markDirty(x-1, y);

      return;
    }

    if (x > 0 && y < 11 && getFg(x, y) == FgElementPlatformStep5)
    {
      if (level[y+1][x-1].dominoType != 0)
      {
        DominoCrash(x-1, y+1, level[y][x].dominoType, level[y][x].dominoExtra);
      }
      else
      {
        level[y+1][x-1].dominoType = level[y][x].dominoType;
        level[y+1][x-1].dominoState = 0xE;
        level[y+1][x-1].dominoDir = level[y][x].dominoDir;
        level[y+1][x-1].dominoYOffset = -6;
        level[y+1][x-1].dominoExtra = 0x40;
      }

      level[y][x].dominoType = 0;
      level[y][x].dominoState = 0;
      level[y][x].dominoDir = 0;
      level[y][x].dominoYOffset = 0;

      target.markDirty(x, y-1);
      target.markDirty(x-1, y-1);
      target.markDirty(x, y);
      target.markDirty(x-1, y);

      return;
    }

    level[y][x].dominoDir = 0;
    return;
  }

}

// Tumbler falln down left
void levelPlayer_c::DTA_6(int x, int y) {

  int fg;

  if (x > 0)
    fg = level[y][x-1].fg;
  else
    fg = 0;

  if (fg == FgElementLadder)
    fg = 0;

  if ((level[y][x].fg == FgElementPlatformStart || level[y][x].fg == FgElementPlatformStrip) && fg == 0)
  {
    if (level[y+1][x-1].dominoType != 0)
    {
      DominoCrash(x-1, y+1, level[y][x].dominoType, level[y][x].dominoExtra);
    }
    else
    {
      level[y][x-1].dominoType = level[y][x].dominoType;
      level[y][x-1].dominoState = 14;
      level[y][x-1].dominoDir = level[y][x].dominoDir;
      level[y][x-1].dominoYOffset = 2;
      level[y][x-1].dominoExtra = 0x70;

      soundSystem_c::instance()->startSound(soundSystem_c::SE_ASCENDER);
    }

    level[y][x].dominoType = 0;
    level[y][x].dominoState = 0;
    level[y][x].dominoDir = 0;

    target.markDirty(x, y);
    target.markDirty(x-1, y);
    target.markDirty(x, y-1);
    target.markDirty(x-1, y-1);

    return;
  }

  if (x > 0 && level[y][x].fg == FgElementPlatformStep6 && fg == FgElementPlatformStep5)
  {
    if (level[y][x-1].dominoType != 0)
    {
      DominoCrash(x-1, y, level[y][x].dominoType, level[y][x].dominoExtra);
    }
    else
    {
      level[y][x-1].dominoType = level[y][x].dominoType;
      level[y][x-1].dominoState = 14;
      level[y][x-1].dominoDir = level[y][x].dominoDir;
      level[y][x-1].dominoYOffset = 2;
      level[y][x-1].dominoExtra = 0x40;
    }

    level[y][x].dominoType = 0;
    level[y][x].dominoState = 0;
    level[y][x].dominoDir = 0;
    level[y][x].dominoYOffset = 0;

    target.markDirty(x, y);
    target.markDirty(x-1, y);
    target.markDirty(x, y-1);
    target.markDirty(x-1, y-1);

    return;
  }

  if (x > 0 && y < 11 && level[y][x].fg == FgElementPlatformStep5)
  {
    if (level[y+1][x-1].dominoType != 0)
    {
      DominoCrash(x-1, y+1, level[y][x].dominoType, level[y][x].dominoExtra);
    }
    else
    {
      level[y+1][x-1].dominoType = level[y][x].dominoType;
      level[y+1][x-1].dominoState = 14;
      level[y+1][x-1].dominoDir = level[y][x].dominoDir;
      level[y+1][x-1].dominoYOffset = -6;
      level[y+1][x-1].dominoExtra = 0x40;
    }

    level[y][x].dominoType = 0;
    level[y][x].dominoState = 0;
    level[y][x].dominoDir = 0;
    level[y][x].dominoYOffset = 0;

    target.markDirty(x, y);
    target.markDirty(x-1, y);
    target.markDirty(x, y-1);
    target.markDirty(x-1, y-1);

    return;
  }

  if (level[y][x-1].dominoType != 0)
  {
    DominoCrash(x-1, y, level[y][x].dominoType, level[y][x].dominoExtra);
  }
  else
  {
    level[y][x-1].dominoType = DominoTypeTumbler;
    level[y][x-1].dominoState = 14;
    level[y][x-1].dominoDir = level[y][x].dominoDir;
    level[y][x-1].dominoYOffset = 0;
  }

  level[y][x].dominoType = 0;
  level[y][x].dominoDir = 0;

  target.markDirty(x, y);
  target.markDirty(x-1, y);
  target.markDirty(x, y-1);
  target.markDirty(x-1, y-1);


}

// Tumbler falln down right
void levelPlayer_c::DTA_L(int x, int y) {

  int fg;

  if (x < 19)
    fg = level[y][x+1].fg;
  else
    fg = 0;

  if (fg == FgElementLadder)
    fg = 0;

  if ((level[y][x].fg == FgElementPlatformEnd || level[y][x].fg == FgElementPlatformStrip) && fg == 0)
  {
    if (level[y+1][x+1].dominoType != 0)
    {
      DominoCrash(x+1, y+1, level[y][x].dominoType, level[y][x].dominoExtra);
    }
    else
    {
      level[y][x+1].dominoType = level[y][x].dominoType;
      level[y][x+1].dominoState = 2;
      level[y][x+1].dominoDir = level[y][x].dominoDir;
      level[y][x+1].dominoYOffset = 2;
      level[y][x+1].dominoExtra = 0x70;

      soundSystem_c::instance()->startSound(soundSystem_c::SE_ASCENDER);
    }

    level[y][x].dominoType = 0;
    level[y][x].dominoState = 0;
    level[y][x].dominoDir = 0;

    target.markDirty(x, y);
    target.markDirty(x+1, y);
    target.markDirty(x, y-1);
    target.markDirty(x+1, y-1);

    return;
  }

  if (x < 19 && level[y][x].fg == FgElementPlatformStep1 && fg == FgElementPlatformStep2)
  {
    if (level[y][x+1].dominoType != 0)
    {
      DominoCrash(x+1, y, level[y][x].dominoType, level[y][x].dominoExtra);
    }
    else
    {
      level[y][x+1].dominoType = level[y][x].dominoType;
      level[y][x+1].dominoState = 2;
      level[y][x+1].dominoDir = level[y][x].dominoDir;
      level[y][x+1].dominoYOffset = 2;
      level[y][x+1].dominoExtra = 0x40;
    }

    level[y][x].dominoType = 0;
    level[y][x].dominoState = 0;
    level[y][x].dominoDir = 0;
    level[y][x].dominoYOffset = 0;

    target.markDirty(x, y);
    target.markDirty(x+1, y);
    target.markDirty(x, y-1);
    target.markDirty(x+1, y-1);

    return;
  }

  if (x < 19 && y < 11 && level[y][x].fg == FgElementPlatformStep2)
  {
    if (level[y+1][x+1].dominoType != 0)
    {
      DominoCrash(x+1, y+1, level[y][x].dominoType, level[y][x].dominoExtra);
    }
    else
    {
      level[y+1][x+1].dominoType = level[y][x].dominoType;
      level[y+1][x+1].dominoState = 2;
      level[y+1][x+1].dominoDir = level[y][x].dominoDir;
      level[y+1][x+1].dominoYOffset = -6;
      level[y+1][x+1].dominoExtra = 0x40;
    }

    level[y][x].dominoType = 0;
    level[y][x].dominoState = 0;
    level[y][x].dominoDir = 0;
    level[y][x].dominoYOffset = 0;

    target.markDirty(x, y);
    target.markDirty(x+1, y);
    target.markDirty(x, y-1);
    target.markDirty(x+1, y-1);

    return;
  }

  if (level[y][x+1].dominoType != 0)
  {
    DominoCrash(x+1, y, level[y][x].dominoType, level[y][x].dominoExtra);
  }
  else
  {
    level[y][x+1].dominoType = DominoTypeTumbler;
    level[y][x+1].dominoState = 2;
    level[y][x+1].dominoDir = level[y][x].dominoDir;
  }

  level[y][x].dominoType = 0;
  level[y][x].dominoDir = 0;

  target.markDirty(x, y);
  target.markDirty(x+1, y);
  target.markDirty(x, y-1);
  target.markDirty(x+1, y-1);
}

void levelPlayer_c::callStateFunction(int type, int state, int x, int y) {

  switch ((type-1)*17+state-1) {

    // DominoTypeStandard
    case   0: DTA_1(x, y); break;
    case   1: DTA_2(x, y); break;
    case   2: DTA_3(x, y); break;
    case   3: DTA_4(x, y); break;
    case   4: DTA_4(x, y); break;
    case   5: DTA_4(x, y); break;
    case   6: DTA_4(x, y); break;
    case   7: DTA_E(x, y); break;
    case   8: DTA_4(x, y); break;
    case   9: DTA_4(x, y); break;
    case  10: DTA_4(x, y); break;
    case  11: DTA_4(x, y); break;
    case  12: DTA_I(x, y); break;
    case  13: DTA_J(x, y); break;
    case  14: DTA_K(x, y); break;

              // DominoTypeStopper
    case  24: DTA_F(x, y); break;

              // DominoTypeSplitter
    case  35: DTA_C(x, y); break;
    case  36: DTA_C(x, y); break;
    case  37: DTA_D(x, y); break;
    case  38: DTA_D(x, y); break;
    case  39: DTA_D(x, y); break;
    case  40: DTA_D(x, y); break;
    case  41: DTA_F(x, y); break;
    case  42: DTA_C(x, y); break;
    case  43: DTA_C(x, y); break;
    case  44: DTA_C(x, y); break;
    case  45: DTA_C(x, y); break;
    case  46: DTA_C(x, y); break;
    case  47: DTA_C(x, y); break;

              // DominoTypeExploder
    case  51: DTA_5(x, y); break;
    case  52: DTA_4(x, y); break;
    case  53: DTA_4(x, y); break;
    case  54: DTA_4(x, y); break;
    case  55: DTA_4(x, y); break;
    case  56: DTA_4(x, y); break;
    case  57: DTA_4(x, y); break;
    case  58: DTA_F(x, y); break;

              // DominoTypeDelay
    case  68: DTA_1(x, y); break;
    case  69: DTA_2(x, y); break;
    case  70: DTA_3(x, y); break;
    case  71: DTA_4(x, y); break;
    case  72: DTA_4(x, y); break;
    case  73: DTA_4(x, y); break;
    case  74: DTA_4(x, y); break;
    case  75: DTA_G(x, y); break;
    case  76: DTA_4(x, y); break;
    case  77: DTA_4(x, y); break;
    case  78: DTA_4(x, y); break;
    case  79: DTA_4(x, y); break;
    case  80: DTA_I(x, y); break;
    case  81: DTA_J(x, y); break;
    case  82: DTA_K(x, y); break;

              // DominoTypeTumbler
    case  85: DTA_6(x, y); break;
    case  86: DTA_2(x, y); break;
    case  87: DTA_3(x, y); break;
    case  88: DTA_4(x, y); break;
    case  89: DTA_4(x, y); break;
    case  90: DTA_4(x, y); break;
    case  91: DTA_4(x, y); break;
    case  92: DTA_E(x, y); break;
    case  93: DTA_4(x, y); break;
    case  94: DTA_4(x, y); break;
    case  95: DTA_4(x, y); break;
    case  96: DTA_4(x, y); break;
    case  97: DTA_I(x, y); break;
    case  98: DTA_J(x, y); break;
    case  99: DTA_L(x, y); break;

              // DominoTypeBridger
    case 102: DTA_7(x, y); break;
    case 103: DTA_2(x, y); break;
    case 104: DTA_3(x, y); break;
    case 105: DTA_4(x, y); break;
    case 106: DTA_4(x, y); break;
    case 107: DTA_4(x, y); break;
    case 108: DTA_4(x, y); break;
    case 109: DTA_E(x, y); break;
    case 110: DTA_4(x, y); break;
    case 111: DTA_4(x, y); break;
    case 112: DTA_4(x, y); break;
    case 113: DTA_4(x, y); break;
    case 114: DTA_I(x, y); break;
    case 115: DTA_J(x, y); break;
    case 116: DTA_M(x, y); break;

              // DominoTypeVanish
    case 119: DTA_8(x, y); break;
    case 120: DTA_4(x, y); break;
    case 121: DTA_3(x, y); break;
    case 122: DTA_4(x, y); break;
    case 123: DTA_4(x, y); break;
    case 124: DTA_4(x, y); break;
    case 125: DTA_4(x, y); break;
    case 126: DTA_E(x, y); break;
    case 127: DTA_4(x, y); break;
    case 128: DTA_4(x, y); break;
    case 129: DTA_4(x, y); break;
    case 130: DTA_4(x, y); break;
    case 131: DTA_I(x, y); break;
    case 132: DTA_4(x, y); break;
    case 133: DTA_8(x, y); break;

              // DominoTypeTrigger
    case 136: DTA_9(x, y); break;
    case 137: DTA_2(x, y); break;
    case 138: DTA_3(x, y); break;
    case 139: DTA_4(x, y); break;
    case 140: DTA_4(x, y); break;
    case 141: DTA_4(x, y); break;
    case 142: DTA_4(x, y); break;
    case 143: DTA_E(x, y); break;
    case 144: DTA_4(x, y); break;
    case 145: DTA_4(x, y); break;
    case 146: DTA_4(x, y); break;
    case 147: DTA_4(x, y); break;
    case 148: DTA_I(x, y); break;
    case 149: DTA_J(x, y); break;
    case 150: DTA_N(x, y); break;

              // DominoTypeAscender
    case 153: DTA_A(x, y); break;
    case 154: DTA_4(x, y); break;
    case 155: DTA_3(x, y); break;
    case 156: DTA_4(x, y); break;
    case 157: DTA_4(x, y); break;
    case 158: DTA_4(x, y); break;
    case 159: DTA_4(x, y); break;
    case 160: DTA_H(x, y); break;
    case 161: DTA_4(x, y); break;
    case 162: DTA_4(x, y); break;
    case 163: DTA_4(x, y); break;
    case 164: DTA_4(x, y); break;
    case 165: DTA_I(x, y); break;
    case 166: DTA_4(x, y); break;
    case 167: DTA_O(x, y); break;
    case 168: DTA_H(x, y); break;
    case 169: DTA_H(x, y); break;

              // DominoTypeCrash0
    case 170: DTA_B(x, y); break;
    case 171: DTA_B(x, y); break;
    case 172: DTA_B(x, y); break;
    case 173: DTA_B(x, y); break;
    case 174: DTA_B(x, y); break;

              // DominoTypeCrash1
    case 187: DTA_B(x, y); break;
    case 188: DTA_B(x, y); break;
    case 189: DTA_B(x, y); break;
    case 190: DTA_B(x, y); break;
    case 191: DTA_B(x, y); break;

              // DominoTypeCrash2
    case 204: DTA_B(x, y); break;
    case 205: DTA_B(x, y); break;
    case 206: DTA_B(x, y); break;
    case 207: DTA_B(x, y); break;
    case 208: DTA_B(x, y); break;

              // DominoTypeCrash3
    case 221: DTA_B(x, y); break;
    case 222: DTA_B(x, y); break;
    case 223: DTA_B(x, y); break;
    case 224: DTA_B(x, y); break;
    case 225: DTA_B(x, y); break;

              // DominoTypeCrash4
    case 238: DTA_B(x, y); break;
    case 239: DTA_B(x, y); break;
    case 240: DTA_B(x, y); break;
    case 241: DTA_B(x, y); break;
    case 242: DTA_B(x, y); break;

              // DominoTypeCrash5
    case 255: DTA_B(x, y); break;
    case 256: DTA_B(x, y); break;
    case 257: DTA_B(x, y); break;
    case 258: DTA_B(x, y); break;
    case 259: DTA_B(x, y); break;

              // DominoTypeRiserCont
              // DominoTypeQuaver
  }
}

int levelPlayer_c::performDominos(ant_c & a) {

  a.performAnimation(target);

  for (int y = 0; y < 13; y++)
    for (int x = 0; x < 20; x++)
      if (getDominoType(x, y) != DominoTypeEmpty &&
          getDominoState(x, y) != 0) {

        callStateFunction(getDominoType(x, y), getDominoState(x, y), x, y);

        if (getDominoType(x, y) == DominoTypeAscender)
        {
          if (target.isDirty(x-1, y)) target.markDirty(x-1, y-1);
          if (target.isDirty(x  , y)) target.markDirty(x  , y-1);
          if (target.isDirty(x+1, y)) target.markDirty(x+1, y-1);
        }
      }

  timeTick();

  if (triggerIsFalln() && !finishCheckDone)
  {
    finishCheckDone = true;

    int reason = 0;

    if (levelCompleted(reason) && !a.carrySomething() && a.isLiving())
    {
      a.success();
    }
    else
    {
      a.fail();

      if (reason)
        return reason;
      else if (a.carrySomething())
        return 4;
      else
        return 5;
    }
  }

  return 0;
}

bool levelPlayer_c::levelCompleted(int & fail) {

  for (int y = 0; y < 13; y++)
    for (int x = 0; x < 20; x++) {
      if (level[y][x].dominoType >= DominoTypeCrash0 &&
          level[y][x].dominoType <= DominoTypeCrash5)
      {
        fail = 3;
        return false;
      }

      if (level[y][x].dominoType != DominoTypeEmpty && level[y][x].dominoType != DominoTypeStopper)
      {
        if (level[y][x].dominoType == DominoTypeSplitter)
        { // for splitters is must have started to fall
          if (level[y][x].dominoState > 2 && level[y][x].dominoState <= 8)
          {
            fail = 6;
            return false;
          }
        }
        else if (level[y][x].dominoType == DominoTypeTrigger)
        { // triggers must as least have started to fall
          if (level[y][x].dominoState == 8)
          {
            fail = 6;
            return false;
          }
        }
        else if (level[y][x].dominoType == DominoTypeTumbler)
        { // tumbler must lie on something or lean against a wall
          // not falln far enough
          if (level[y][x].dominoState > 3 && level[y][x].dominoState < 13) {
            fail = 6;
            return false;
          }

          // check if we lean against a step
          if (level[y][x].dominoState == 3 &&
              level[y][x-1].fg != FgElementPlatformStep4 &&
              level[y][x-1].fg != FgElementPlatformStep7)
          {
            fail = 6;
            return false;
          }

          if (level[y][x].dominoState == 13 &&
              level[y][x+1].fg != FgElementPlatformStep4 &&
              level[y][x+1].fg != FgElementPlatformStep7)
          {
            fail = 6;
            return false;
          }

          // falln far enough but neighbor empty
          if (   level[y][x].dominoState <= 2
              && level[y][x-1].dominoType == DominoTypeEmpty
              && (level[y][x-2].dominoType == DominoTypeEmpty || level[y][x-2].dominoState < 14))
          {
            fail = 6;
            return false;
          }
          if (   level[y][x].dominoState >= 14
              && level[y][x+1].dominoType == DominoTypeEmpty
              && (level[y][x+2].dominoType == DominoTypeEmpty || level[y][x+2].dominoState > 2)) {
            fail = 6;
            return false;
          }

        }
        else
        {
          if (level[y][x].dominoState == 8)
          {
            fail = 4;
            return false;
          }
          if (level[y][x].dominoState > 3 && level[y][x].dominoState < 13)
          {
            // here we certainly fail
            fail = 6;
            return false;
          }
          // in this case we might still succeed, when we lean against a block
          if (   level[y][x].dominoState == 3
              && level[y][x-1].fg != FgElementPlatformStep4
              && (   level[y][x-1].dominoType != DominoTypeStopper
                  || level[y][x+1].dominoType == DominoTypeEmpty
                  || (level[y][x+1].dominoDir != -1 && level[y][x+1].dominoType != DominoTypeSplitter))
             )
          {
            // here we lean against a of a blocker and can not go back
            fail = 6;
            return false;
          }

          if (   level[y][x].dominoState == 13
              && level[y][x+1].fg != FgElementPlatformStep6
              && (   level[y][x+1].dominoType != DominoTypeStopper
                  || level[y][x-1].dominoType == DominoTypeEmpty
                  || (level[y][x-1].dominoDir != 1 && level[y][x-1].dominoType != DominoTypeSplitter))
             )
          {
            // here we lean against a step
            fail = 6;
            return false;
          }
        }
      }
    }

  return true;
}
