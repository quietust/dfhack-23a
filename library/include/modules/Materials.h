/*
https://github.com/peterix/dfhack
Copyright (c) 2009-2012 Petr Mrázek (peterix@gmail.com)

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must
not claim that you wrote the original software. If you use this
software in a product, an acknowledgment in the product documentation
would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
distribution.
*/

#pragma once
#ifndef CL_MOD_MATERIALS
#define CL_MOD_MATERIALS
/**
 * \defgroup grp_materials Materials module - used for reading raws mostly
 * @ingroup grp_modules
 */
#include "Export.h"
#include "Module.h"
#include "Types.h"
#include "BitArray.h"

#include "DataDefs.h"
#include "df/material_type.h"
#include "df/matgloss_wood.h"
#include "df/matgloss_stone.h"
#include "df/matgloss_gem.h"
#include "df/matgloss_plant.h"

#include <vector>
#include <string>

namespace df
{
    struct item;
    struct creature_raw;
    struct material_vec_ref;

    union job_item_flags1;
    union job_item_flags2;
}

namespace DFHack
{
    struct t_matpair {
        int16_t mat_type;
        int16_t mat_subtype;

        t_matpair(int16_t type = -1, int16_t subtype = -1)
            : mat_type(type), mat_subtype(subtype) {}
    };

    struct DFHACK_EXPORT MaterialInfo {
        df::material_type type;
        int16_t subtype;

        enum Mode {
            None,
            Builtin,
            Wood,
            Stone,
            Gem,
            Plant,
            Creature,
            Invalid
        };
        Mode mode;

        df::matgloss_wood *wood;
        df::matgloss_stone *stone;
        df::matgloss_gem *gem;
        df::matgloss_plant *plant;
        df::creature_raw *creature;

    public:
        MaterialInfo(df::material_type type = (df::material_type)-1, int16_t subtype = -1) { decode(type, subtype); }
        MaterialInfo(const t_matpair &mp) { decode((df::material_type)mp.mat_type, mp.mat_subtype); }
        template<class T> MaterialInfo(T *ptr) { decode(ptr); }

        bool isValid() const { return mode != Invalid; }

        bool isNone() const { return mode == None; }
        bool isBuiltin() const { return mode == Builtin; }
        bool isWood() const { return mode == Wood; }
        bool isStone() const { return mode == Stone; }
        bool isGem() const { return mode == Gem; }
        bool isPlant() const { return mode == Plant; }
        bool isCreature() const { return mode == Creature; }

        bool decode(df::material_type type, int16_t subtype = -1);
        bool decode(df::item *item);
        bool decode(const df::material_vec_ref &vr, int idx);
        bool decode(const t_matpair &mp) { return decode((df::material_type)mp.mat_type, mp.mat_subtype); }

        template<class T> bool decode(T *ptr) {
            // Assume and exploit a certain naming convention
            return ptr ? decode(ptr->material, ptr->matgloss) : decode(-1);
        }

        bool find(const std::string &token);
        bool find(const std::vector<std::string> &tokens);

        bool findBuiltin(df::material_type type, const std::string &token);
        bool findWood(df::material_type type, const std::string &token);
        bool findStone(df::material_type type, const std::string &token);
        bool findGem(df::material_type type, const std::string &token);
        bool findPlant(df::material_type type, const std::string &token);
        bool findCreature(df::material_type type, const std::string &token);

        std::string getToken();
        std::string toString(uint16_t temp = 10015);

        bool matches(const MaterialInfo &mat)
        {
            if (!mat.isValid()) return true;
            return (type == mat.type) &&
                   (mat.subtype == -1 || subtype == mat.subtype);
        }
    };

    inline bool operator== (const MaterialInfo &a, const MaterialInfo &b) {
        return a.type == b.type && a.subtype == b.subtype;
    }
    inline bool operator!= (const MaterialInfo &a, const MaterialInfo &b) {
        return a.type != b.type || a.subtype != b.subtype;
    }

    typedef int16_t t_materialType, t_materialSubtype, t_itemType, t_itemSubtype;

    /**
     * A copy of the game's material data.
     * \ingroup grp_materials
     */
    class DFHACK_EXPORT t_matgloss
    {
    public:
        std::string id; // raw name
        std::string name; // a sensible human-readable name
        uint8_t fore;
        uint8_t back;
        uint8_t bright;

        int32_t value;        // Material value
        uint8_t wall_tile;    // Tile when a natural wall

    public:
        t_matgloss();
    };

    /**
     * \ingroup grp_materials
     */
    struct t_descriptor_color
    {
        std::string id;
        std::string name;
        float red;
        float green;
        float blue;
    };
    /**
     * \ingroup grp_materials
     */
    struct t_matglossPlant
    {
        std::string id;
        std::string name;
        uint8_t fore;
        uint8_t back;
        uint8_t bright;
        std::string drink_name;
        std::string food_name;
        std::string extract_name;
    };
    /**
     * \ingroup grp_materials
     */
    struct t_bodypart
    {
        std::string id;
        std::string category;
        std::string singular;
        std::string plural;
    };
    /**
     * \ingroup grp_materials
     */
    struct t_matglossOther
    {
        std::string id;
    };
    /**
     * \ingroup grp_materials
     */
    struct t_creatureextract
    {
        std::string id;
    };
    /**
     * \ingroup grp_materials
     */
    struct t_creaturetype
    {
        std::string id;
        std::vector <t_creatureextract> extract;
        uint8_t tile_character;
        struct
        {
            uint16_t fore;
            uint16_t back;
            uint16_t bright;
        } tilecolor;
    };

    /**
     * this structure describes what are things made of in the DF world
     * \ingroup grp_materials
     */
    struct t_material
    {
        t_itemType item_type;
        t_itemSubtype item_subtype;
        t_materialType mat_type;
        t_materialSubtype mat_subtype;
        uint32_t flags;
    };
    /**
     * The Materials module
     * \ingroup grp_modules
     * \ingroup grp_materials
     */
    class DFHACK_EXPORT Materials : public Module
    {
    public:
        Materials();
        ~Materials();
        bool Finish();

        std::vector<t_matgloss> race;
        std::vector<t_creaturetype> raceEx;
        std::vector<t_descriptor_color> color;
        std::vector<t_matglossOther> other;
        std::vector<t_matgloss> alldesc;

        bool CopyInorganicMaterials (std::vector<t_matgloss> & inorganic);
        bool CopyWoodMaterials (std::vector<t_matgloss> & tree);
        bool CopyPlantMaterials (std::vector<t_matgloss> & plant);

        bool ReadCreatureTypes (void);
        bool ReadCreatureTypesEx (void);
        bool ReadDescriptorColors(void);

        bool ReadAllMaterials(void);
    };
    DFHACK_EXPORT std::string getMaterialDescription(t_materialType type, t_materialSubtype subtype);
}
#endif

