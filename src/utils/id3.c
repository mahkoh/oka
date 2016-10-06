#include <string.h>
#include <stddef.h>

#include "utils/utils.h"
#include "utils/id3.h"
#include "utils/metadata.h"
#include "utils/xmalloc.h"

enum {
    ID3_TDOR = METADATA_NUM_TAGS + 1,
    ID3_TORY,
    ID3_TDRC,
    ID3_TDRL,
    ID3_TYER,
};

static const struct {
    union {
        char tag_str[4];
        u32 tag;
    };
    int key;
} id3_map[] = {
    { { "TALB" }, METADATA_ALBUM       },
    { { "TBPM" }, METADATA_BPM         },
    { { "TCMP" }, METADATA_COMPILATION },
    { { "TCON" }, METADATA_GENRE       },
    { { "TIT2" }, METADATA_TITLE       },
    { { "TPE1" }, METADATA_ARTIST      },
    { { "TPE2" }, METADATA_ALBUMARTIST },
    { { "TPE4" }, METADATA_REMIXER     },
    { { "TPOS" }, METADATA_DISC        },
    { { "TRCK" }, METADATA_TRACK       },
    
    { { "TDOR" }, ID3_TDOR             },
    { { "TORY" }, ID3_TORY             },
    
    { { "TDRC" }, ID3_TDRC             },
    { { "TDRL" }, ID3_TDRL             },
    { { "TYER" }, ID3_TYER             },
};

static int id3_tag_to_metadata(const char tag_str[static 4])
{
    u32 tag;
    memcpy(&tag, tag_str, 4);
    for (size_t i = 0; i < N_ELEMENTS(id3_map); i++) {
        if (tag == id3_map[i].tag)
            return id3_map[i].key;
    }
    return METADATA_INVALID;
}

static void id3_fill_year(enum metadata_key key, const char *txt, 
        char *metadata[static METADATA_NUM_TAGS])
{
    auto date = xnew_uninit(metadata_date_format);
    size_t i = 0;
    for (; i < sizeof(*date) - 1 && txt[i]; i++)
        (*date)[i] = txt[i];
    for (; i < sizeof(*date) - 1; i++)
        (*date)[i] = '0';
    (*date)[sizeof(*date) - 1] = 0;
    metadata[key] = *date;
}

static void id3_fill_timestamp(enum metadata_key key, const char *txt, 
        char *metadata[static METADATA_NUM_TAGS])
{
    auto date = xnew_uninit(metadata_date_format);
    size_t i = 0;
    for (; i < sizeof(*date) - 1 && txt[i]; i++) {
        if (*txt == '-') {
            txt++;
            continue;
        }
        (*date)[i] = txt[i];
    }
    for (; i < sizeof(*date) - 1; i++)
        (*date)[i] = '0';
    (*date)[sizeof(*date) - 1] = 0;
    metadata[key] = *date;
}

static const char *id3_genres[] = {
    "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", "Hip-Hop",
    "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B", "Rap", "Reggae", "Rock",
    "Techno", "Industrial", "Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack",
    "Euro-Techno", "Ambient", "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance",
    "Classical", "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise",
    "AlternRock", "Bass", "Soul", "Punk", "Space", "Meditative", "Instrumental Pop",
    "Instrumental Rock", "Ethnic", "Gothic", "Darkwave", "Techno-Industrial",
    "Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult",
    "Gangsta", "Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native American",
    "Cabaret", "New Wave", "Psychadelic", "Rave", "Showtunes", "Trailer", "Lo-Fi",
    "Tribal", "Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", "Rock & Roll",
    "Hard Rock", "Folk", "Folk-Rock", "National Folk", "Swing", "Fast Fusion", "Bebob",
    "Latin", "Revival", "Celtic", "Bluegrass", "Avantgarde", "Gothic Rock",
    "Progressive Rock", "Psychedelic Rock", "Symphonic Rock", "Slow Rock", "Big Band",
    "Chorus", "Easy Listening", "Acoustic", "Humour", "Speech", "Chanson", "Opera",
    "Chamber Music", "Sonata", "Symphony", "Booty Bass", "Primus", "Porn Groove",
    "Satire", "Slow Jam", "Club", "Tango", "Samba", "Folklore", "Ballad", "Power Ballad",
    "Rhythmic Soul", "Freestyle", "Duet", "Punk Rock", "Drum Solo", "A capella",
    "Euro-House", "Dance Hall", "Goa", "Drum & Bass", "Club-House", "Hardcore", "Terror",
    "Indie", "BritPop", "Negerpunk", "Polsk Punk", "Beat", "Christian Gangsta Rap",
    "Heavy Metal", "Black Metal", "Crossover", "Contemporary Christian", "Christian Rock",
    "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop", "Synthpop" "Abstract",
    "Art Rock", "Baroque", "Bhangra", "Big Beat", "Breakbeat", "Chillout", "Downtempo",
    "Dub", "EBM", "Eclectic", "Electro", "Electroclash", "Emo", "Experimental", "Garage",
    "Global", "IDM", "Illbient", "Industro-Goth", "Jam Band", "Krautrock", "Leftfield",
    "Lounge", "Math Rock", "New Romantic", "Nu-Breakz", "Post-Punk", "Post-Rock",
    "Psytrance", "Shoegaze", "Space Rock", "Trop Rock", "World Music", "Neoclassical",
    "Audiobook", "Audio Theatre", "Neue Deutsche Welle", "Podcast", "Indie Rock",
    "G-Funk", "Dubstep", "Garage Rock", "Psybient",
};

static void id3_fill_genre(const char *txt, char *metadata[static METADATA_NUM_TAGS])
{
    if (txt[0] == '(') {
        if (txt[1] == 'R' && txt[2] == 'X' && txt[3] == ')') {
            metadata[METADATA_GENRE] = xstrdup("Remix");
            return;
        }

        if (txt[1] == 'C' && txt[2] == 'R' && txt[3] == ')') {
            metadata[METADATA_GENRE] = xstrdup("Cover");
            return;
        }

        char *end;
        unsigned long num = strtoul(txt + 1, &end, 10);
        if (end != txt + 1 && *end == ')' && num < N_ELEMENTS(id3_genres)) {
            metadata[METADATA_GENRE] = xstrdup(id3_genres[num]);
            return;
        }
    }

    metadata[METADATA_GENRE] = xstrdup(txt);
}

static void id3_fill_date(int tag, const char *txt,
        char *metadata[static METADATA_NUM_TAGS])
{
    auto fn = id3_fill_timestamp;
    enum metadata_key field = METADATA_DATE;
    if (tag == ID3_TDOR || tag == ID3_TORY)
        field = METADATA_ORIGINALDATE;
    if (tag == ID3_TORY || tag == ID3_TYER)
        fn = id3_fill_year;
    fn(field, txt, metadata);
}

void id3_metadata(const char tag[static 4], const char *txt,
        char *metadata[static METADATA_NUM_TAGS])
{
    auto key = id3_tag_to_metadata(tag);
    if (key > METADATA_NUM_TAGS)
        id3_fill_date(key, txt, metadata);
    else if (key == METADATA_GENRE)
        id3_fill_genre(txt, metadata);
    else if (key != METADATA_INVALID)
        metadata[key] = xstrdup(txt);
}

// vim: et:sw=4:tw=90:ts=4:sts=4:cc=+1
