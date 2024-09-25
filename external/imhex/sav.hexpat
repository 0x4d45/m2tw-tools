struct NullStr
{
    char value[];
};

struct AsciiStr
{
    u16 length;
    char value[length];
};

struct UnicodeStr
{
    u16 length;
    char16 value[length];
};

enum Difficulty : u32
{
    EASY = 0,
    MEDIUM = 1,
    HARD = 2,
    VERY_HARD = 3,
};

enum Season: u8
{
    SUMMER = 0,
    WINTER = 1,
};

struct SectionEntry
{
    NullStr name;
    u32 version;
};

struct SectionTable
{
    u32 length;
    SectionEntry entries[length];
};

struct SectionHeader
{
    u16 [[name("???")]];
    u32 offset;
    u32 size;
};

struct Section
{
    SectionHeader header;
    u8 data[header.size - sizeof(header)];
};

struct MercenaryUnit
{
    AsciiStr name;
};

struct MercenaryPool
{
    AsciiStr name;
    u32 num_units;
    MercenaryUnit units;
};

struct Mercenaries
{
    u16 [[name("???")]];
    u32 num_pools;
    MercenaryPool pools;
};

u16 magic @ 0;
UnicodeStr packs_root @ $;
$ += 50;
UnicodeStr campaign_name1 @ $;
$ += 54;
Difficulty campaign_difficulty @ $;
Difficulty battle_difficulty @ $;
$ += 1636;
Season season @ $;
$ += 1;
u16 date @ $;
$ += 8;
SectionTable section_table @ $;
SectionHeader global_header @ $;
UnicodeStr campaign_map_dir @ $;
UnicodeStr campaign_name2 @ $;
Section section1 @ $;
Section section2 @ $;
$ += 2;
UnicodeStr descr_strat @ $;
$ += 3;
Section section3 @ $;
Section section4 @ $;
Section section5 @ $;
Section section6 @ $;
Section section7 @ $;
Section section8 @ $;
Section section9 @ $;
Section section10 @ $;
Section section11 @ $;
Section section12 @ $;
Section section13 @ $;
Mercenaries mercenaries @ $;
