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

#ifndef __LEVELSET_H__
#define __LEVELSET_H__

#include "textsections.h"

#include <string>
#include <vector>
#include <map>

class levelData_c;

class levelset_c {

  private:

    std::string name;
    std::string description;
    unsigned int priority;
    std::vector<std::string> levelNames;
    std::map<std::string, std::string> checksums;
    std::map<std::string, std::string> checksumsNoTime;
    std::map<std::string, std::string> filenames; // contains the filenames for the levels, if known, "" otherwise
    std::map<std::string, textsections_c> levels;

  public:

    levelset_c(const std::string & path, const std::string & userString, bool noindexrequired = false);
    const std::string getName(void) const { return name; }
    const std::string getDescription(void) const { return description; }
    const unsigned int getPriority(void) const { return priority; }

    const std::vector<std::string> & getLevelNames(void) const { return levelNames; }
    const std::string & getChecksum(const std::string & levelName) const;
    const std::string & getChecksumNoTime(const std::string & levelName) const;
    const std::string & getFilename(const std::string & levelName) const;
    void loadLevel(levelData_c & level, const std::string & levelName, const std::string & userString) const;
};

class levelsetList_c {

  private:

    std::map<std::string, levelset_c> levelsets;
    std::vector<std::string> levelsetNames;
    std::vector<std::pair<unsigned int, std::string> > sortHelper;

  public:

    levelsetList_c() {};
    void load(const std::string & path, const std::string & userString);
    const std::vector<std::string> & getLevelsetNames(void) const { return levelsetNames; }
    const levelset_c & getLevelset(const std::string & levelsetName) const;
};


// function to collect all the authors of one levelset
std::string collectAuthors(const levelset_c & ls);

// load all the levels in the given directory
levelsetList_c * loadAllLevels(const std::string & datadir, const std::string & userString);

#endif
